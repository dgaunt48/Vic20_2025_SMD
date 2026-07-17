//------------------------------------------------------------------------------------------------
//---- VIA 6522 Snoop ... 2026 Dave Gaunt                                                     ----
//------------------------------------------------------------------------------------------------
//---- Version 0.1                                                                            ----
//------------------------------------------------------------------------------------------------
#include <stdio.h>
#include "types.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"

#include "vga111.h"

#define VGA_CLOCK				(25000000)			/* 25Mhz VGA Dot Clock */

enum device_pins {
	PIN_ADDRESS_BIT0 = 0,
	PIN_ADDRESS_BIT1,
	PIN_ADDRESS_BIT2,
	PIN_ADDRESS_BIT3,
	PIN_VIA1_CS1,			/* User Port */
	PIN_VIA2_CS1,			/* Keyboard */
	PIN_READ_WRITE,
	PIN_RESET,
	PIN_IRQ,				/* VIA 2 */
	PIN_NMI,				/* VIA 1 */
	PIN_IO0 = 22,			/* CS2 active low */
	PIN_CLK_VGA = 24,
	PIN_S02_READ = 26,
	PIN_VSYNC,
	PIN_HSYNC,
	PIN_RED,
	PIN_GREEN,
	PIN_BLUE,
	PIN_DATA_BIT0 = 32,
	PIN_DATA_BIT1,
	PIN_DATA_BIT2,
	PIN_DATA_BIT3,
	PIN_DATA_BIT4,
	PIN_DATA_BIT5,
	PIN_DATA_BIT6,
	PIN_DATA_BIT7
};

static_assert(24 == PIN_CLK_VGA, "VGA Clock must be on PIN 24!");
static_assert(32 == PIN_DATA_BIT0, "Data Bus must be in High Bits");

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
enum via_register_names
{
	VIA_REG_PORTB = 0,
	VIA_REG_PORTA,
	VIA_REG_DATA_DIRB,
	VIA_REG_DATA_DIRA,
	VIA_REG_TIMER1_L,
	VIA_REG_TIMER1_H,
	VIA_REG_TIMER1_LATCH_L,
	VIA_REG_TIMER1_LATCH_H,
	VIA_REG_TIMER2_L,
	VIA_REG_TIMER2_H,
	VIA_REG_SHIFT,
	VIA_REG_AUXILIARY_CONTROL,
	VIA_REG_PERIPHERAL_CONTROL,
	VIA_REG_INTERRUPT_FLAGS,
	VIA_REG_INTERRUPT_ENABLE,
	VIA_REG_PORTA_NO_HANDSHAKE
};

// Register Name Strings For Debug View.
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

static volatile ViaRegisters s_aViaRegs[2] = {0};

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
static void __not_in_flash_func(function_core1)(void)
{
	save_and_disable_interrupts();

	u32 uS02 = 1;
	u32 uViaIndex = 0;
 	u32 uLow32Pins = gpioc_lo_in_get();

	// Wait for IO0 To Return Hi AND S02 To Assert Low
	while ( (0 == ((uLow32Pins >> PIN_IO0) & 1)) || (1 == ((uLow32Pins >> PIN_S02_READ) & 1)) )
		uLow32Pins = gpioc_lo_in_get();

	while(true)
 	{
		while(true)
		{
			uViaIndex = 0;
	 		uLow32Pins = gpioc_lo_in_get();

			// If S02 Is Hi
			if ((uLow32Pins >> PIN_S02_READ) & 1)
			{
				if (0 == uS02)
				{
					// S02 Has Transitioned From Low To Hi
					uS02 = 1;

					if ((uLow32Pins >> PIN_VIA1_CS1) & 1)
						uViaIndex = 1;

					if ((uLow32Pins >> PIN_VIA2_CS1) & 1)
						uViaIndex = 2;
						
					// And IO0 Is Low Then Process VIA IO
					if ((0 == ((uLow32Pins >> PIN_IO0) & 1)) && (0 != uViaIndex ))
						break;
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
		const u32 uRegisterIndex = (uLow32Pins >> PIN_ADDRESS_BIT0) & 0xF;
		const u32 uHigh32Pins = gpioc_hi_in_get();
		const u32 uData = (uHigh32Pins >> (PIN_DATA_BIT0 - 32)) & 0xFF;
		s_aViaRegs[uViaIndex - 1].m_aReg[uRegisterIndex] = uData;
	}
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
int main()
{
	stdio_init_all();

	gpio_init(PIN_S02_READ);
	gpio_set_dir(PIN_S02_READ, GPIO_IN);

	// CS1 Is Address Line 4 Or 5 On The VIC.
	gpio_init(PIN_VIA1_CS1);
	gpio_set_dir(PIN_VIA1_CS1, GPIO_IN);

	gpio_init(PIN_VIA2_CS1);
	gpio_set_dir(PIN_VIA2_CS1, GPIO_IN);

	gpio_init(PIN_IO0);
	gpio_set_dir(PIN_IO0, GPIO_IN);

	gpio_init(PIN_READ_WRITE);
	gpio_set_dir(PIN_READ_WRITE, GPIO_IN);

	// IRQ Active Low
	gpio_init(PIN_IRQ);
	gpio_set_dir(PIN_IRQ, GPIO_IN);
	gpio_put(PIN_IRQ, true);

	// Set All Data Pins To Input
	for(u32 uPin=PIN_DATA_BIT0; uPin<=PIN_DATA_BIT7; ++uPin)
	{
		gpio_init(uPin);
		gpio_set_dir(uPin, GPIO_IN);
	}

	// Set All Address Pins To Input
	for(u32 uPin=PIN_ADDRESS_BIT0; uPin<=PIN_ADDRESS_BIT3; ++uPin)
	{
		gpio_init(uPin);
		gpio_set_dir(uPin, GPIO_IN);
	}

	clock_gpio_init(PIN_CLK_VGA, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS, ((float)SYS_CLK_HZ / (float)(VGA_CLOCK)));

	gpio_init(PIN_RESET);
	gpio_set_dir(PIN_RESET, GPIO_IN);

	multicore_launch_core1(function_core1);

	vga_Init(PIN_RED, PIN_HSYNC, PIN_VSYNC);
	vga_FilledRect(0, 0, VGA_RESOLUTION_X, VGA_RESOLUTION_Y, RGB111_GREEN);
	vga_FilledRect(1, 1, VGA_RESOLUTION_X-2, VGA_RESOLUTION_Y-2, RGB111_BLACK);

	// Draw All The Constant Text To The Screen
	char szTempString[128];
	vga_DrawString(28, 18, "VIA 6522 Bus Snoop", RGB111_MAGENTA);

	for (u32 uRegisterIndex=0; uRegisterIndex<16; ++uRegisterIndex)
	{ 
		sprintf(szTempString, "0x%04X", 0x9110 + uRegisterIndex);
		vga_DrawString(13, 20 + uRegisterIndex, szTempString, RGB111_BLUE);
		szTempString[4] = '2';
		vga_DrawString(43, 20 + uRegisterIndex, szTempString, RGB111_BLUE);

		vga_DrawString(20, 20 + uRegisterIndex, "0x", RGB111_YELLOW);
		vga_DrawString(50, 20 + uRegisterIndex, "0x", RGB111_YELLOW);

		vga_DrawString(25, 20 + uRegisterIndex, s_aszRegisterNames[uRegisterIndex], RGB111_CYAN);
		vga_DrawString(55, 20 + uRegisterIndex, s_aszRegisterNames[uRegisterIndex], RGB111_CYAN);
	}

	while(true)
	{
		u16 uHexPair;

		// Loop For All 16 Registers
		for (u32 uRegisterIndex=0; uRegisterIndex<16; ++uRegisterIndex)
		{ 
			// Write The Register Values To The Appropriate Screen Position
			uHexPair = byteToHex(s_aViaRegs[0].m_aReg[uRegisterIndex]);
			vga_DrawPetsciiChar(22 << 3, (20 + uRegisterIndex) << 3, uHexPair >> 8, RGB111_YELLOW);
			vga_DrawPetsciiChar(23 << 3, (20 + uRegisterIndex) << 3, uHexPair & 255, RGB111_YELLOW);

			// Write The Register Values To The Appropriate Screen Position
			uHexPair = byteToHex(s_aViaRegs[1].m_aReg[uRegisterIndex]);
			vga_DrawPetsciiChar(52 << 3, (20 + uRegisterIndex) << 3, uHexPair >> 8, RGB111_YELLOW);
			vga_DrawPetsciiChar(53 << 3, (20 + uRegisterIndex) << 3, uHexPair & 255, RGB111_YELLOW);
		}
	}
}
