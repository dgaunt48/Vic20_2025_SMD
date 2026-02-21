//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
#include <stdio.h>
#include "types.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/spi.h"

#include "hsync.pio.h"
#include "vsync.pio.h"
#include "rgb.pio.h"

#include "MCP23S17_GPIOExpander.h"
#include "VicChars.h"

#define MCP23S17_SPI_VIA_1		(0)         /* SPI IC ADDRESS */

#define VGA_RESOLUTION_X    	(640)
#define VGA_RESOLUTION_Y  		(480)
#define TERMINAL_CHARS_WIDE		(VGA_RESOLUTION_X >> 3)
#define TERMINAL_CHARS_HIGH		(VGA_RESOLUTION_Y >> 3)

enum vga_pins {
	PIN_ADDRESS_BIT0 = 0,
	PIN_ADDRESS_BIT1,
	PIN_ADDRESS_BIT2,
	PIN_ADDRESS_BIT3,
	PIN_ADDRESS_BIT4,
	PIN_ADDRESS_BIT5,
	PIN_READ_WRITE,
	PIN_RESET,
	PIN_IRQ,
	PIN_NMI,
	PIN_SPI0_CS = 17,
	PIN_SPI0_SCK,
	PIN_SPI0_TX,
	PIN_SPI0_RX,
	PIN_IO0 = 22,
	PIN_S02 = 26,
	PIN_VSYNC,
	PIN_HSYNC,
	PIN_RED,
	PIN_GREEN,
	PIN_BLUE
};

enum rgbColours {RGB_BLACK, RGB_RED, RGB_GREEN, RGB_YELLOW, RGB_BLUE, RGB_MAGENTA, RGB_CYAN, RGB_WHITE};

u8 aVGAScreenBuffer[(VGA_RESOLUTION_X * VGA_RESOLUTION_Y) >> 1];
u8* address_pointer = aVGAScreenBuffer;

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
enum via_irq_flags
{
	VIA_IRQ_CA2 = 0,
	VIA_IRQ_CA1,
	VIA_IRQ_SHIFT,
	VIA_IRQ_CB2,
	VIA_IRQ_CB1,
	VIA_IRQ_TIMER2,
	VIA_IRQ_TIMER1,
	VIA_IRQ_SET_CLR
};

typedef struct
{
	union
	{
		u8 m_aReg[16];
		struct
		{
			u8 m_u8PortB;					/* 0 */
			u8 m_u8PortA;					/* 1 */
			u8 m_uDataDirB;					/* 2 */
			u8 m_uDataDirA;					/* 3 */
			union
			{
				u16	m_uTimer1;
				struct
				{
					u8 m_uTimer1_L;			/* 4 */
					u8 m_uTimer1_H;			/* 5 */
				};
			};
			union
			{
				u16	m_uTimer1_Latch;
				struct
				{
					u8 m_uTimer1_Latch_L;	/* 6 */
					u8 m_uTimer1_Latch_H;	/* 7 */
				};
			};
			union
			{
				u16	m_uTimer2;
				struct
				{
					u8 m_uTimer2_L;			/* 8 */
					u8 m_uTimer2_H;			/* 9 */
				};
			};
			u8 m_uShiftReg;					/* A */
			u8 m_uAuxiliaryCtrl;			/* B */
			u8 m_uPeripheralCtrl;			/* C */
			u8 m_uInterruptFlags;			/* D */
			u8 m_uInterruptEnable;			/* E */
			u8 m_u8PortA_NoHandshake;		/* F */
		};
	};
} ViaRegisters;
static_assert(sizeof(ViaRegisters) == 16);

static const char s_aszRegisterNames[16][16] =
{
/*  "123456789ABCDEF"	*/
	"Port B",
	"Port A",
	"Dir B",
	"Dir A",
	"Timer 1 L",
	"Timer 1 H",
	"T1 Latch L",
	"T1 Latch H",
	"Timer 2 L",
	"Timer 2 H",
	"Shift Reg",
	"Aux Ctrl",
	"Periph Ctrl",
	"Int Flags",
	"Int Enable",
	"PA No HShake"
/*  "123456789ABCDEF"	*/
};

static volatile ViaRegisters s_aViaRegs[2] = {0};

typedef struct
{
	u8	m_uOffset;
	u8	m_uData;
} RegisterBuffer;

static volatile RegisterBuffer s_aRegBuffer[16];
static volatile u8 s_uRegHead = 15;
static volatile u8 s_uRegTail = 15;

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
void initVGA()
{
	// Choose which PIO instance to use (there are two instances, each with 4 state machines)
	PIO pio = pio0;
	const uint hsync_offset = pio_add_program(pio, &hsync_program);
	const uint vsync_offset = pio_add_program(pio, &vsync_program);
	const uint rgb_offset = pio_add_program(pio, &rgb_program);

	// Manually select a few state machines from pio instance pio0.
	uint hsync_sm = 0;
	uint vsync_sm = 1;
	uint rgb_sm = 2;
	hsync_program_init(pio, hsync_sm, hsync_offset, PIN_HSYNC);
	vsync_program_init(pio, vsync_sm, vsync_offset, PIN_VSYNC);
	rgb_program_init(pio, rgb_sm, rgb_offset, PIN_RED);

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// ============================== PIO DMA Channels =================================================
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	// DMA channels - 0 sends color data, 1 reconfigures and restarts 0
	int rgb_chan_0 = 0;
	int rgb_chan_1 = 1;

	// Channel Zero (sends color data to PIO VGA machine)
	dma_channel_config c0 = dma_channel_get_default_config(rgb_chan_0);  	// default configs
	channel_config_set_transfer_data_size(&c0, DMA_SIZE_8);              	// 8-bit txfers
	channel_config_set_read_increment(&c0, true);                        	// yes read incrementing
	channel_config_set_write_increment(&c0, false);                      	// no write incrementing
	channel_config_set_dreq(&c0, DREQ_PIO0_TX2) ;                        	// DREQ_PIO0_TX2 pacing (FIFO)
	channel_config_set_chain_to(&c0, rgb_chan_1);                        	// chain to other channel

	dma_channel_configure
	(
		rgb_chan_0,                                                        	// Channel to be configured
		&c0,                                                               	// The configuration we just created
		&pio->txf[rgb_sm],                                                 	// write address (RGB PIO TX FIFO)
		&aVGAScreenBuffer,                                                 	// The initial read address (pixel color array)
		(VGA_RESOLUTION_X * VGA_RESOLUTION_Y) >> 1,                        	// Number of transfers; in this case each is 1 byte.
		false                                                              	// Don't start immediately.
	);

	// Channel One (reconfigures the first channel)
	dma_channel_config c1 = dma_channel_get_default_config(rgb_chan_1);  	// default configs
	channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);             	// 32-bit txfers
	channel_config_set_read_increment(&c1, false);                       	// no read incrementing
	channel_config_set_write_increment(&c1, false);                      	// no write incrementing
	channel_config_set_chain_to(&c1, rgb_chan_0);                        	// chain to other channel

	dma_channel_configure
	(
		rgb_chan_1,                         	// Channel to be configured
		&c1,                                	// The configuration we just created
		&dma_hw->ch[rgb_chan_0].read_addr,  	// Write address (channel 0 read address)
		&address_pointer,                   	// Read address (POINTER TO AN ADDRESS)
		1,                                 	 	// Number of transfers, in this case each is 4 byte
		false                               	// Don't start immediately.
	);

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

	// Initialize PIO state machine counters. This passes the information to the state machines
	// that they retrieve in the first 'pull' instructions, before the .wrap_target directive
	// in the assembly. Each uses these values to initialize some counting registers.
	#define H_ACTIVE   655    // (active + frontporch - 1) - one cycle delay for mov
	#define V_ACTIVE   479    // (active - 1)
	#define RGB_ACTIVE 319    // (horizontal active)/2 - 1
	// #define RGB_ACTIVE 639 // change to this if 1 pixel/byte
	pio_sm_put_blocking(pio, hsync_sm, H_ACTIVE);
	pio_sm_put_blocking(pio, vsync_sm, V_ACTIVE);
	pio_sm_put_blocking(pio, rgb_sm, RGB_ACTIVE);

	// Start the two pio machine IN SYNC
	// Note that the RGB state machine is running at full speed,
	// so synchronization doesn't matter for that one. But, we'll
	// start them all simultaneously anyway.
	pio_enable_sm_mask_in_sync(pio, ((1u << hsync_sm) | (1u << vsync_sm) | (1u << rgb_sm)));

	// Start DMA channel 0. Once started, the contents of the pixel color array
	// will be continously DMA's to the PIO machines that are driving the screen.
	// To change the contents of the screen, we need only change the contents
	// of that array.
	dma_start_channel_mask((1u << rgb_chan_0)) ;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
void FilledRectangle(u32 uPositionX, u32 uPositionY, u32 uWidth, u32 uHeight, u32 uColour)
{
	if (uPositionX + uWidth >= VGA_RESOLUTION_X)
		uWidth = VGA_RESOLUTION_X - uPositionX;

	if (uPositionY + uHeight >= VGA_RESOLUTION_Y)
		uHeight = VGA_RESOLUTION_Y - uPositionY;

	if ((uWidth > 0) && (uHeight > 0))
	{
		u32 uPixelOffset = ((uPositionY * VGA_RESOLUTION_X) + uPositionX) >> 1;

		if (uPositionX & 1)
		{
			u32 uOffset = uPixelOffset++;
			--uWidth;

			for(u32 y=0; y<uHeight; ++y)
			{
				aVGAScreenBuffer[uOffset] = (aVGAScreenBuffer[uOffset] & 0b11000111) | (uColour << 3);
				uOffset += VGA_RESOLUTION_X >> 1;
			}
		}

		while (uWidth > 1)
		{
		u32 uOffset = uPixelOffset++;
		uWidth -= 2;

		for(u32 y=0; y<uHeight; ++y)
		{
			aVGAScreenBuffer[uOffset] = (uColour << 3) | uColour;
			uOffset += VGA_RESOLUTION_X >> 1;
		}
		}

		if (1 == uWidth)
		{
			for(u32 y=0; y<uHeight; ++y)
			{
				aVGAScreenBuffer[uPixelOffset] = (aVGAScreenBuffer[uPixelOffset] & 0b11111000) | uColour;
				uPixelOffset += VGA_RESOLUTION_X >> 1;
			}
		}
	}
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
void DrawPetsciiChar(const u32 uXPos, const u32 uYPos, const u8 uChar, const u8 uColour)
{
	for (u32 uLine=0; uLine<8; ++uLine)
	{
		u32 uPixelOffset = ((((uYPos + uLine) * VGA_RESOLUTION_X ) + uXPos) >> 1) + 3;
		u32 uCharLine = VicChars901460_03[2048 + (uChar << 3) + uLine];

		for (u32 x=0; x<4; ++x)
		{
			u8 uPixelPair = 0;

			if (uCharLine & 2)
				uPixelPair = uColour;

			if (uCharLine & 1)
				uPixelPair |= (uColour << 3);

			aVGAScreenBuffer[uPixelOffset--] = uPixelPair;
			uCharLine >>= 2;
		}
	}
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
void DrawString(uint32_t uCharX, uint32_t uCharY, const char* pszString, const uint8_t uColour)
{
	while (*pszString)
	{
		if (uCharX >= (TERMINAL_CHARS_WIDE-1))
		{
			uCharX = 1;
			++uCharY;
		}

		if (uCharY >= (TERMINAL_CHARS_HIGH-1))
			return;

		uint8_t c = *pszString++;

		if (c >= '`')
			c -= '`';

		DrawPetsciiChar(uCharX << 3, uCharY << 3, c, uColour);
		++uCharX;
	}
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
static const uint8_t aHexTable[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

static inline uint16_t byteToHex(const uint8_t uByte)
{
	return (aHexTable[(uByte >> 4) & 15] << 8) | aHexTable[uByte & 15];
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
/*
#define __scratch_x_func(func_name)   __scratch_x(__STRING(func_name)) func_name
static void __scratch_x_func(function_core1)(void)
{
	save_and_disable_interrupts();
 	u32 uLow32Pins = gpioc_lo_in_get();
	u32 uS02 = (uLow32Pins >> PIN_S02) & 1;

	while(true)
 	{
		while(true)
		{
	 		uLow32Pins = gpioc_lo_in_get();

			// If S02 Is Hi
			if ((uLow32Pins >> PIN_S02) & 1)
			{
				// And IO0 Is Low Then Process VIA IO
				if (0 == ((uLow32Pins >> PIN_IO0) & 1))
					break;

				if (0 == uS02)
				{
					// S02 Has Transitioned From Low To Hi - Check If The Timer Is Non-Zero
					if (0 != s_aViaRegs[1].m_uTimer1)
					{
						// If the Timer Will Go To Zero
						if (1 == s_aViaRegs[1].m_uTimer1)
						{
							s_aViaRegs[1].m_uTimer1 = s_aViaRegs[1].m_uTimer1_Latch;
							s_aViaRegs[1].m_uInterruptFlags |= (1 << VIA_IRQ_TIMER1);
						}
						else
						{
							s_aViaRegs[1].m_uTimer1--;
						}
					}

					// We Have Finished For This Transition Of S02
					uS02 = 1;
				}
			}
			else
			{
				// S02 Has Transitioned From Hi To Low
				uS02 = 0;
			}
		}

		delay_120ns();
		uLow32Pins = gpioc_lo_in_get();

		if (0 == ((uLow32Pins >> PIN_READ_WRITE) & 1))
		{
			// CPU Is In Write Mode So Copy Bus To Internal Registers
			const u32 uHi32Pins = gpioc_hi_in_get();
			const u8 uRegTail = (s_uRegTail + 1) & 15;

			// Ring Buffer Full !!!
		    assert(uRegTail != s_uRegHead);

			// Address Bit 4 Selects VIA 1
			if ((uLow32Pins >> PIN_ADDRESS_BIT4) & 1)
			{
				s_aRegBuffer[uRegTail].m_uOffset = uLow32Pins & 0xF;
				s_aRegBuffer[uRegTail].m_uData = (u8)uHi32Pins;
				s_uRegTail = uRegTail;
			}

			// Address Bit 5 Selects VIA 2	(KeyBoard, Cursor Blink Timer)
			if ((uLow32Pins >> PIN_ADDRESS_BIT5) & 1)
			{
				s_aRegBuffer[uRegTail].m_uOffset = (uLow32Pins & 0xF) + 16;
				s_aRegBuffer[uRegTail].m_uData = (u8)uHi32Pins;
				s_uRegTail = uRegTail;
			}
		}
		else
		{
			// CPU Is In Read Mode - So Present Data On Bus
			if ((uLow32Pins >> PIN_ADDRESS_BIT4) & 1)
			{
				const u8 uData = s_aViaRegs[0].m_aReg[uLow32Pins & 0xF];
				for(u32 uPin=0; uPin<8; ++uPin)
				{
					gpio_set_dir(uPin + 32, GPIO_OUT);
					gpio_put(uPin + 32, (uData >> uPin) & 1);
				}
			}

			if ((uLow32Pins >> PIN_ADDRESS_BIT5) & 1)
			{
				const u8 uData = s_aViaRegs[1].m_aReg[uLow32Pins & 0xF];
				for(u32 uPin=0; uPin<8; ++uPin)
				{
					gpio_set_dir(uPin + 32, GPIO_OUT);
					gpio_put(uPin + 32, (uData >> uPin) & 1);
				}

				if (4 == (uLow32Pins & 0xF))
					s_aViaRegs[1].m_uInterruptFlags &= ~(1 << VIA_IRQ_TIMER1);
			}
		}

		// Wait for IO0 To Return Hi OR S02 To Assert Low
		while (0 == (((uLow32Pins >> PIN_IO0) | (~uLow32Pins >> PIN_S02)) & 1))
			uLow32Pins = gpioc_lo_in_get();

		// Get Off The Bus
		for(u32 uPin=32; uPin<=40; ++uPin)
			gpio_set_dir(uPin, GPIO_IN);
	}
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
void ProcessVIA(void)
{
	// Are There Any Register Writes On The Ring Buffer?
	if (s_uRegHead != s_uRegTail)
	{
		const u32 uViaIndex = (s_aRegBuffer[s_uRegHead].m_uOffset >> 4) & 1;
		const u8 uRegister = s_aRegBuffer[s_uRegHead].m_uOffset & 15;
		const u8 uData = s_aRegBuffer[s_uRegHead].m_uData;

		switch(uRegister)
		{
			case 0:		// Port B
			{
				if (uViaIndex)
					MCP23S17_WriteRegisterSingle(MCP23S17_SPI_VIA_1, MCP23S17_OLATB, uData);
			}
			break;

			case 1:		// Port A
			{
				if (uViaIndex)
					MCP23S17_WriteRegisterSingle(MCP23S17_SPI_VIA_1, MCP23S17_OLATA, uData);
			}
			break;
	
			case 2:		// Direction B
			{
				s_aViaRegs[uViaIndex].m_uDataDirB = uData;

				if (uViaIndex)
					MCP23S17_WriteRegisterSingle(MCP23S17_SPI_VIA_1, MCP23S17_IODIRB, (~uData));
			}
			break;

			case 3:		// Direction A
			{
				s_aViaRegs[uViaIndex].m_uDataDirA = uData;

				if (uViaIndex)
					MCP23S17_WriteRegisterSingle(MCP23S17_SPI_VIA_1, MCP23S17_IODIRA, (~uData));
			}
			break;
			
			case 4:		// Timer 1 Low Order Counter
			{
				// Write Data Into The Low Order Latch... Not The Counter!!!
				s_aViaRegs[uViaIndex].m_uTimer1_Latch_L = uData;
			}
			break;

			case 5:		// Timer 1 High Order Counter
			{
				s_aViaRegs[uViaIndex].m_uTimer1_Latch_H = uData;
				s_aViaRegs[uViaIndex].m_uTimer1 = s_aViaRegs[uViaIndex].m_uTimer1_Latch;
				s_aViaRegs[uViaIndex].m_uInterruptFlags &= ~(1 << VIA_IRQ_TIMER1);
			}
			break;

			case 6:		// Timer 1 Low Order Latch
			{
				s_aViaRegs[uViaIndex].m_uTimer1_Latch_L = uData;
			}
			break;

			case 7:		// Timer 1 High Order Latch
			{
				s_aViaRegs[uViaIndex].m_uTimer1_Latch_H = uData;
				s_aViaRegs[uViaIndex].m_uInterruptFlags &= ~(1 << VIA_IRQ_TIMER1);
			}
			break;

			case 13:
			{
				if (uData & 0x80)
				{
					// Bit 7 Is High So Enable Any Specified Interrupts.
					s_aViaRegs[uViaIndex].m_uInterruptFlags |= (uData & 0x7F);
				}
				else
				{
					// Bit 7 Is Low So Disable Any Specified Interrupts.
					s_aViaRegs[uViaIndex].m_uInterruptFlags &= (~uData & 0x7F);
				}

				if (s_aViaRegs[uViaIndex].m_uInterruptFlags)
					s_aViaRegs[uViaIndex].m_uInterruptFlags |= (1 << VIA_IRQ_SET_CLR);
			}
			break;

			case 14:	// Interrupt Enable Flags
			{
				if (uData & 0x80)
				{
					// Bit 7 Is High So Enable Any Specified Interrupts.
					s_aViaRegs[uViaIndex].m_uInterruptEnable |= (uData & 0x7F);
				}
				else
				{
					// Bit 7 Is Low So Disable Any Specified Interrupts.
					s_aViaRegs[uViaIndex].m_uInterruptEnable &= (~uData & 0x7F);
				}
			}
			break;

			default:
			s_aViaRegs[uViaIndex].m_aReg[uRegister] = uData;
		}

		s_uRegHead = (s_uRegHead + 1) & 15;
	}

	const u8 uInterrupFlags = s_aViaRegs[1].m_uInterruptFlags & s_aViaRegs[1].m_uInterruptEnable;

	// If Any Enabled Interrupt Is Flagged The Set The Hi Bit
	s_aViaRegs[1].m_uInterruptFlags = (uInterrupFlags) ? (1 << VIA_IRQ_SET_CLR) | uInterrupFlags : 0;

	// Reflect The IRQ Bit On The IO Pin.
	gpio_put(PIN_IRQ, ((~s_aViaRegs[1].m_uInterruptFlags >> VIA_IRQ_SET_CLR) & 1));

	s_aViaRegs[1].m_u8PortB = MCP23S17_ReadRegisterSingle(MCP23S17_SPI_VIA_1, MCP23S17_GPIOB);
	const u8 uGPIOA = MCP23S17_ReadRegisterSingle(MCP23S17_SPI_VIA_1, MCP23S17_GPIOA);
	s_aViaRegs[1].m_u8PortA = uGPIOA;
	s_aViaRegs[1].m_u8PortA_NoHandshake = uGPIOA;
}
*/
//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
int main()
{
	stdio_init_all();

	// Set All Address Pins To Input
	for(u32 uPin=PIN_ADDRESS_BIT0; uPin<=PIN_ADDRESS_BIT5; ++uPin)
	{
		gpio_init(uPin);
		gpio_set_dir(uPin, GPIO_IN);
	}

	gpio_init(PIN_READ_WRITE);
	gpio_set_dir(PIN_READ_WRITE, GPIO_IN);

	gpio_init(PIN_RESET);
	gpio_set_dir(PIN_RESET, GPIO_IN);

	// IRQ Active Low
	gpio_init(PIN_IRQ);
	gpio_set_dir(PIN_IRQ, GPIO_IN);
//	gpio_put(PIN_IRQ, true);

	// NMI Active Low
	gpio_init(PIN_NMI);
	gpio_set_dir(PIN_NMI, GPIO_IN);
//	gpio_put(PIN_NMI, true);

	gpio_init(PIN_IO0);
	gpio_set_dir(PIN_IO0, GPIO_IN);

	gpio_init(PIN_S02);
	gpio_set_dir(PIN_S02, GPIO_IN);

	for(u32 uPin=32; uPin<48; ++uPin)
	{
		gpio_init(uPin);
		gpio_set_dir(uPin, GPIO_IN);
	}

//	MCP23S17_Initialise(spi0, 5000000, PIN_SPI0_SCK, PIN_SPI0_TX, PIN_SPI0_RX, PIN_SPI0_CS);

//	multicore_launch_core1(function_core1);

	initVGA();
	FilledRectangle(0, 0, VGA_RESOLUTION_X, VGA_RESOLUTION_Y, RGB_GREEN);
	FilledRectangle(1, 1, VGA_RESOLUTION_X-2, VGA_RESOLUTION_Y-2, RGB_BLACK);

//	MCP23S17_WriteRegisterPair(MCP23S17_SPI_VIA_1, MCP23S17_IODIRA, 0x00, 0x00);			// Set All Pins To Output
//	MCP23S17_WriteRegisterPair(MCP23S17_SPI_VIA_1, MCP23S17_GPIOA,  0x55, 0xAA);

	// Draw All The Constant Text To The Screen
	char szTempString[128];
	DrawString(20, 18, "VIA 1", RGB_CYAN);
	DrawString(50, 18, "VIA 2", RGB_CYAN);

	for (u32 uRegisterIndex=0; uRegisterIndex<16; ++uRegisterIndex)
	{ 
		sprintf(szTempString, "0x%04X", 0x9110 + uRegisterIndex);
		DrawString(13, 20 + uRegisterIndex, szTempString, RGB_BLUE);
		DrawString(20, 20 + uRegisterIndex, "0x", RGB_YELLOW);
		DrawString(25, 20 + uRegisterIndex, s_aszRegisterNames[uRegisterIndex], RGB_CYAN);

		sprintf(szTempString, "0x%04X", 0x9120 + uRegisterIndex);
		DrawString(43, 20 + uRegisterIndex, szTempString, RGB_BLUE);
		DrawString(50, 20 + uRegisterIndex, "0x", RGB_YELLOW);
		DrawString(55, 20 + uRegisterIndex, s_aszRegisterNames[uRegisterIndex], RGB_CYAN);
	}



	// TODO - REMOVE - HACK TO RUN WITHOUT RESET !!!!
	// s_aViaRegs[1].m_uTimer1 = 0x4826;
	// s_aViaRegs[1].m_uTimer1_Latch = 0x4826;
	// s_aViaRegs[1].m_uInterruptEnable = 0x40;
	// TODO - REMOVE - HACK TO RUN WITHOUT RESET !!!!


	while(true)
	{
		// u32 uTextXPos = 22;

		// // Loop For Both VIA's
		// for (u32 uViaIndex=0; uViaIndex<2; ++uViaIndex)
		// {
		// 	// Loop For All 16 Registers
		// 	for (u32 uRegisterIndex=0; uRegisterIndex<16; ++uRegisterIndex)
		// 	{ 
		// 		// Write The Register Values To The Appropriate Screen Position
		// 		const uint16_t uHexPair = byteToHex(s_aViaRegs[uViaIndex].m_aReg[uRegisterIndex]);
		// 		DrawPetsciiChar((uTextXPos    ) << 3, (20 + uRegisterIndex) << 3, uHexPair >> 8, RGB_YELLOW);
		// 		DrawPetsciiChar((uTextXPos + 1) << 3, (20 + uRegisterIndex) << 3, uHexPair & 255, RGB_YELLOW);

		// 		// Call This Often Or The Input Buffer Will Overflow!!!
		// 		ProcessVIA();
		// 	}

		// 	uTextXPos += 30;
		// }
	}
}
