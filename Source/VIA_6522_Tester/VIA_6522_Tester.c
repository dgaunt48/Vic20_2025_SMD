//------------------------------------------------------------------------------------------------
//---- VIA 6522 Tester ... 2026 Dave Gaunt                                                    ----
//------------------------------------------------------------------------------------------------
//---- Version 0.1                                                                            ----
//------------------------------------------------------------------------------------------------
#include <stdio.h>
#include "types.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"

#include "vga111.h"

#define	VIA_REGISTER_DISPLAY_X	(20)
#define VIA_REGISTER_DISPLAY_Y	(5)

#define VIC_PAL_CLOCK       	(4433618)
#define VIC_CPU_CLOCK			(VIC_PAL_CLOCK >> 2)
#define VIA_SLOW_CLOCK			(100000)			/* 100 khz Clock */
#define VIA_CLOCK				(VIA_SLOW_CLOCK)

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
	PIN_CLK = 24,			/* Clock Signal Generator */
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

static_assert(24 == PIN_CLK, "Clock must be on PIN 24!");

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

static volatile ViaRegisters s_viaRegs = {0};

typedef struct
{
	u8	m_uOffset;
	u8	m_uData;
} RegisterBuffer;

#define VIA_RING_BUFFER_SIZE	(64)			/* Must Be A Power Of 2! */
static volatile RegisterBuffer s_aRegBuffer[VIA_RING_BUFFER_SIZE];
static volatile u8 s_uRegHead = VIA_RING_BUFFER_SIZE - 1;
static volatile u8 s_uRegTail = VIA_RING_BUFFER_SIZE - 1;

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

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
static u8 ReadVIARegister(const u8 uRegisterIndex)
{
	u32 uLow32Pins = gpioc_lo_in_get();

	// Wait for S02 To Assert Low
	while (1 == ((uLow32Pins >> PIN_S02_READ) & 1) )
		uLow32Pins = gpioc_lo_in_get();

	busy_wait_at_least_cycles(500);

	// Put Register Address On BUS
	gpio_put_masked(0xF << PIN_ADDRESS_BIT0, uRegisterIndex << PIN_ADDRESS_BIT0);

	// Enable VIA
	gpio_put(PIN_IO0, false);

	// Wait for S02 To Assert High
	while (0 == ((uLow32Pins >> PIN_S02_READ) & 1) )
		uLow32Pins = gpioc_lo_in_get();

	busy_wait_at_least_cycles(500);

	u32 uHi32Pins = gpioc_hi_in_get();

	// Disable VIA
	gpio_put(PIN_IO0, true);

	return uHi32Pins & 0xFF;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
static void WriteVIARegister(const u8 uRegisterIndex, const u8 uValue)
{
	u32 uLow32Pins = gpioc_lo_in_get();

	// Wait for S02 To Assert Low
	while (1 == ((uLow32Pins >> PIN_S02_READ) & 1) )
		uLow32Pins = gpioc_lo_in_get();

	busy_wait_at_least_cycles(500);

	// Put Register Address On BUS
	gpio_put_masked(0xF << PIN_ADDRESS_BIT0, uRegisterIndex << PIN_ADDRESS_BIT0);

	gpioc_hi_oe_set(0xFF);
    gpioc_hi_out_xor((gpioc_hi_out_get() ^ uValue) & 0xFF);
	gpio_put(PIN_READ_WRITE, false);				// Write Mode

	// Enable VIA
	gpio_put(PIN_IO0, false);

	// Wait for S02 To Assert High
	while (0 == ((uLow32Pins >> PIN_S02_READ) & 1) )
		uLow32Pins = gpioc_lo_in_get();

	busy_wait_at_least_cycles(500);

	gpio_put(PIN_READ_WRITE, true);					// Read Mode
	gpioc_hi_oe_clr(0xFF);

	// Disable VIA
	gpio_put(PIN_IO0, true);
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
static void PushVIARegister(const u8 uRegisterIndex, const u8 uValue)
{
	// Push Register Set Onto Ring Buffer.
	const u8 uRegTail = (s_uRegTail + 1) & (VIA_RING_BUFFER_SIZE - 1);
	assert(uRegTail != s_uRegHead);		// Ring Buffer Is Full !!!

	s_aRegBuffer[uRegTail].m_uOffset = uRegisterIndex;
	s_aRegBuffer[uRegTail].m_uData = uValue;
	s_uRegTail = uRegTail;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
static void __not_in_flash_func(function_core1)(void)
{
	save_and_disable_interrupts();
	u32 uLow32Pins = gpioc_lo_in_get();
	u32 uS02 = (uLow32Pins >> PIN_S02_READ) & 1;
	
	// Wait for IO0 To Return Hi OR S02 To Assert Low
	while ( (0 == ((uLow32Pins >> PIN_IO0) & 1)) || (1 == ((uLow32Pins >> PIN_S02_READ) & 1)) )
		uLow32Pins = gpioc_lo_in_get();

	u8 uAddress = 0;

	while(true)
 	{

		if(uAddress)
		{
			WriteVIARegister(VIA_REG_DATA_DIRA, 0x55);
			WriteVIARegister(VIA_REG_DATA_DIRB, 0xAA);

//			const u8 uPortA = ReadVIARegister(VIA_REG_PORTA);
//			PushVIARegister(VIA_REG_PORTA, uPortA);
		}
		else
		{
			const u8 uDirA = ReadVIARegister(VIA_REG_DATA_DIRA);
			PushVIARegister(VIA_REG_DATA_DIRA, uDirA);
			const u8 uDirB = ReadVIARegister(VIA_REG_DATA_DIRB);
			PushVIARegister(VIA_REG_DATA_DIRB, uDirB);
		}

		uAddress = !uAddress;
	}
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
int main()
{
	stdio_init_all();

	gpio_init(PIN_RESET);						// Put The VIA Into Reset
	gpio_set_dir(PIN_RESET, GPIO_OUT);
	gpio_put(PIN_RESET, false);

	// Set All Address Pins To Output
	for(u32 uPin=PIN_ADDRESS_BIT0; uPin<=PIN_ADDRESS_BIT3; ++uPin)
	{
		gpio_init(uPin);
		gpio_set_dir(uPin, GPIO_OUT);
		gpio_put(uPin, false);
	}

	gpio_init(PIN_VIA1_CS1);					// VIA 1 Active High
	gpio_set_dir(PIN_VIA1_CS1, GPIO_OUT);
	gpio_put(PIN_VIA1_CS1, false);				// User Port VIA Disabled

	gpio_init(PIN_VIA2_CS1);					// VIA 2 Active High
	gpio_set_dir(PIN_VIA2_CS1, GPIO_OUT);
	gpio_put(PIN_VIA2_CS1, true);				// KeyBoard VIA Enabled

	gpio_init(PIN_READ_WRITE);
	gpio_set_dir(PIN_READ_WRITE, GPIO_OUT);
	gpio_put(PIN_READ_WRITE, true);				// Read Mode
	
	gpio_init(PIN_IO0);							// Active Low
	gpio_set_dir(PIN_IO0, GPIO_OUT);
	gpio_put(PIN_IO0, true);
	
	gpio_init(PIN_IRQ);							// IRQ Active Low
	gpio_set_dir(PIN_IRQ, GPIO_IN);

	gpio_init(PIN_NMI);							// IRQ Active Low
	gpio_set_dir(PIN_NMI, GPIO_IN);

	// Set All Data Pins To Input
	for(u32 uPin=PIN_DATA_BIT0; uPin<=PIN_DATA_BIT7; ++uPin)
	{
		gpio_init(uPin);
		gpio_set_dir(uPin, GPIO_IN);
	}

	multicore_launch_core1(function_core1);

	// Create The Phase 2 Clock
	gpio_init(PIN_S02_READ);
	gpio_set_dir(PIN_S02_READ, GPIO_IN);
	clock_gpio_init(PIN_CLK, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS, ((float)SYS_CLK_HZ / (float)VIA_CLOCK));

	vga_Init(PIN_RED, PIN_HSYNC, PIN_VSYNC);
	vga_FilledRect(0, 0, VGA_RESOLUTION_X, VGA_RESOLUTION_Y, RGB111_GREEN);
	vga_FilledRect(1, 1, VGA_RESOLUTION_X-2, VGA_RESOLUTION_Y-2, RGB111_BLACK);

	gpio_put(PIN_RESET, true);			// Release VIA From RESET

	// Draw All The Constant Text To The Screen
	char szTempString[128];
	vga_DrawString(VIA_REGISTER_DISPLAY_X + 7, VIA_REGISTER_DISPLAY_Y, "VIA 6522", RGB111_CYAN);

	for (u32 uRegisterIndex=0; uRegisterIndex<16; ++uRegisterIndex)
	{ 
		sprintf(szTempString, "0x%04X", 0x9110 + uRegisterIndex);
		vga_DrawString(VIA_REGISTER_DISPLAY_X, VIA_REGISTER_DISPLAY_Y + 2 + uRegisterIndex, szTempString, RGB111_BLUE);
		vga_DrawString(VIA_REGISTER_DISPLAY_X + 7, VIA_REGISTER_DISPLAY_Y + 2 + uRegisterIndex, "0x", RGB111_YELLOW);
		vga_DrawString(VIA_REGISTER_DISPLAY_X + 13, VIA_REGISTER_DISPLAY_Y + 2 + uRegisterIndex, s_aszRegisterNames[uRegisterIndex], RGB111_CYAN);
	}

	while(true)
	{
		// Update The Register List From The Ring Buffer.
		if (s_uRegHead != s_uRegTail)
		{
			const u8 uRegister = s_aRegBuffer[s_uRegHead].m_uOffset & (VIA_RING_BUFFER_SIZE - 1);
			const u8 uData = s_aRegBuffer[s_uRegHead].m_uData;
			s_viaRegs.m_aReg[uRegister] = uData;
			s_uRegHead = (s_uRegHead + 1) & (VIA_RING_BUFFER_SIZE - 1);
		}

		for (u32 uRegisterIndex=0; uRegisterIndex<16; ++uRegisterIndex)
		{ 
			// Write The Register Values To The Appropriate Screen Position
			const u16 uHexPair = byteToHex(s_viaRegs.m_aReg[uRegisterIndex]);
			vga_DrawPetsciiChar((VIA_REGISTER_DISPLAY_X + 9) << 3, ((VIA_REGISTER_DISPLAY_Y + 2) + uRegisterIndex) << 3, uHexPair >> 8, RGB111_YELLOW);
			vga_DrawPetsciiChar((VIA_REGISTER_DISPLAY_X + 10) << 3, ((VIA_REGISTER_DISPLAY_Y + 2) + uRegisterIndex) << 3, uHexPair & 255, RGB111_YELLOW);
		}
	}
}
