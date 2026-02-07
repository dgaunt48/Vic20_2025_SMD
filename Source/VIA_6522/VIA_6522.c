
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

#include "VicChars.h"

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
void FormatHexDumpLine(uint32_t uCharX, uint32_t uCharY, const uint32_t uAddress, const uint8_t* pLineBuffer, const uint8_t uColour)
{
	// Write Address Offset in 6 byte hex.
	for(int i=5; i>=0; --i)
		DrawPetsciiChar((uCharX + 5 - i) << 3, uCharY << 3, aHexTable[(uAddress >> (i << 2)) & 15], uColour);

	// Write 16 bytes worth of hex values.
 	for(uint32_t uIndex=0; uIndex<16; ++uIndex)
	{
	    const uint16_t uHexPair = byteToHex(pLineBuffer[uIndex]);
		DrawPetsciiChar((uCharX + 8 + (uIndex * 3)) << 3, uCharY << 3, uHexPair >> 8, uColour);
		DrawPetsciiChar((uCharX + 9 + (uIndex * 3)) << 3, uCharY << 3, uHexPair & 255, uColour);

		// Write ASCII version of byte.
		DrawPetsciiChar((uCharX + 57 + uIndex) << 3, uCharY << 3, pLineBuffer[uIndex], uColour);
	}
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
#define __scratch_x_func(func_name)   __scratch_x(__STRING(func_name)) func_name

typedef struct
{
	union
	{
		u8 m_aReg[16];
		struct
		{
			u8 m_u8PortB;
			u8 m_u8PortA;
			u8 m_uDataDirB;
			u8 m_uDataDirA;
			u8 m_uTimer1_L;
			u8 m_uTimer1_H;
			u8 m_uTimer1_Load_L;
			u8 m_uTimer1_Load_H;
			u8 m_uTimer2_L;
			u8 m_uTimer2_H;
			u8 m_uShiftReg;
			u8 m_uAuxiliaryCtrl;
			u8 m_uPeripheralCtrl;
			u8 m_uInterruptFlags;
			u8 m_uInterruptEnable;
			u8 m_u8PortA_NoHandshake;
		};
	};
} ViaRegisters;

static volatile ViaRegisters s_aViaRegs[2];

static void __scratch_x_func(function_core1)(void)
{
	save_and_disable_interrupts();
 	u32 uLow32Pins = 0;

	while(true)
 	{
		// Wait for IO0 To Assert Low AND S02 To Assert HI
		while (1 == (((uLow32Pins >> PIN_IO0) | (~uLow32Pins >> PIN_S02)) & 1))
	 		uLow32Pins = gpioc_lo_in_get();

 		uLow32Pins = gpioc_lo_in_get();

		if (0 == ((uLow32Pins >> PIN_READ_WRITE) & 1))
		{
			delay_120ns();
			const u32 uHi32Pins = gpioc_hi_in_get();
			
			if ((uLow32Pins >> PIN_ADDRESS_BIT4) & 1)
				s_aViaRegs[0].m_aReg[uLow32Pins & 0xF] = (u8)uHi32Pins;

			if ((uLow32Pins >> PIN_ADDRESS_BIT5) & 1)
				s_aViaRegs[1].m_aReg[uLow32Pins & 0xF] = (u8)uHi32Pins;
		}

		// Wait for IO0 To Return Hi OR S02 To Assert Low
		while (0 == (((uLow32Pins >> PIN_IO0) | (~uLow32Pins >> PIN_S02)) & 1))
			uLow32Pins = gpioc_lo_in_get();
	}
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
int main()
{
	stdio_init_all();

	for(u32 uPin=0; uPin<10; ++uPin)
	{
		gpio_init(uPin);
		gpio_set_dir(uPin, GPIO_IN);
	}

	gpio_init(PIN_IO0);
	gpio_set_dir(PIN_IO0, GPIO_IN);

	gpio_init(PIN_S02);
	gpio_set_dir(PIN_S02, GPIO_IN);

	for(u32 uPin=32; uPin<48; ++uPin)
	{
		gpio_init(uPin);
		gpio_set_dir(uPin, GPIO_IN);
	}

	multicore_launch_core1(function_core1);

	initVGA();
	FilledRectangle(0, 0, VGA_RESOLUTION_X, VGA_RESOLUTION_Y, RGB_GREEN);
	FilledRectangle(1, 1, VGA_RESOLUTION_X-2, VGA_RESOLUTION_Y-2, RGB_BLACK);

	char szTempString[128];

	for (u32 uRegisterIndex=0; uRegisterIndex<16; ++uRegisterIndex)
	{ 
		sprintf(szTempString, "0x%04X", 0x9110 + uRegisterIndex);
		DrawString(3, 20 + uRegisterIndex, szTempString, RGB_BLUE);
		sprintf(szTempString, "0x%04X", 0x9120 + uRegisterIndex);
		DrawString(23, 20 + uRegisterIndex, szTempString, RGB_BLUE);
	}

	while(true)
	{
		u32 uTextXPos = 10;

		for (u32 uViaIndex=0; uViaIndex<2; ++uViaIndex)
		{
			for (u32 uRegisterIndex=0; uRegisterIndex<16; ++uRegisterIndex)
			{ 
				const u8 uValue = s_aViaRegs[uViaIndex].m_aReg[uRegisterIndex];
				sprintf(szTempString, "0x%02X", uValue);

				if ((1 == uViaIndex) && (0 == uRegisterIndex) && (0xF7 != uValue))
					DrawString(uTextXPos + 10, 20 + uRegisterIndex, szTempString, RGB_CYAN);

				DrawString(uTextXPos, 20 + uRegisterIndex, szTempString, RGB_YELLOW);
			}

			uTextXPos += 20;
		}
	}
}
