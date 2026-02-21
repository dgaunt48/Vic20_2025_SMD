//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------

#include <stdio.h>
#include "types.h"
#include "pico/stdlib.h"

#include "MCP23S17_GPIOExpander.h"

static u32 s_uChipSelectMask = 0;

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool MCP23S17_Initialise(spi_inst_t* pSpi, const u32 uBaudRate, const u32 uClkPin, const u32 uTxPin, const u32 uRxPin, const u32 uCsPin)
{
	assert(0 == s_uChipSelectMask);
	s_uChipSelectMask = 1 << uCsPin;

	// 10MHz is the fastest rate supported by the MCP23S17
	assert(uBaudRate <= 10000000);
	spi_init(pSpi, uBaudRate);

	gpio_set_dir(uCsPin, GPIO_OUT);
	gpio_clr_mask(s_uChipSelectMask);
	gpio_set_function(uCsPin, GPIO_FUNC_SIO);
	gpio_set_function(uClkPin, GPIO_FUNC_SPI);
	gpio_set_function(uTxPin, GPIO_FUNC_SPI);
	gpio_set_function(uRxPin, GPIO_FUNC_SPI);

	// Toggle the CS bit before first communication after power up - Note 1 Page 8 MCP23S17.pdf
	gpio_set_mask(s_uChipSelectMask);
	delay_40ns();
	gpio_clr_mask(s_uChipSelectMask);
	delay_40ns();
	gpio_set_mask(s_uChipSelectMask);

	// HAEN (Hardware Address Enable Bit) is initally low so communicate with all MCP23S17 ic's on the SPI bus
	MCP23S17_WriteRegSingle((0xFF << 16) | (MCP23S17_GPPUA << 8) | MCP23S17_DEVICE_WRITE);      // Enable all 100k internal pull up resistors
	MCP23S17_WriteRegSingle((0xFF << 16) | (MCP23S17_GPPUB << 8) | MCP23S17_DEVICE_WRITE);
	MCP23S17_WriteRegSingle((0xFF << 16) | (MCP23S17_IODIRA << 8) | MCP23S17_DEVICE_WRITE);     // Set all I/O pins to input
	MCP23S17_WriteRegSingle((0xFF << 16) | (MCP23S17_IODIRB << 8) | MCP23S17_DEVICE_WRITE);
	MCP23S17_WriteRegSingle((0x00 << 16) | (MCP23S17_GPIOA << 8) | MCP23S17_DEVICE_WRITE);      // Set all I/O pins to low
	MCP23S17_WriteRegSingle((0x00 << 16) | (MCP23S17_GPIOB << 8) | MCP23S17_DEVICE_WRITE);

	// Set HAEN (Hardware Address Enable Bit) on all MCP23S17 ic's connected to the SPI bus
	MCP23S17_WriteRegisterSingle(0, MCP23S17_IOCON, MCP23S17_IOCON_SEQOP | MCP23S17_IOCON_HAEN);

	return true;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
u8 MCP23S17_ReadRegSingle(const u32 uBuffer)
{
	u8 uReturnValue;
	delay_120ns();
	gpio_clr_mask(s_uChipSelectMask);
	spi_write_blocking(spi0, (const u8*)&uBuffer, 2);
	spi_read_blocking(spi0, 0, &uReturnValue, 1);
	gpio_set_mask(s_uChipSelectMask);
	return uReturnValue;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
void MCP23S17_WriteRegSingle(const u32 uBuffer)
{
	delay_120ns();
	gpio_clr_mask(s_uChipSelectMask);
	spi_write_blocking(spi0, (const u8*)&uBuffer, 3);
	gpio_set_mask(s_uChipSelectMask);
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
void MCP23S17_WriteRegPair(const u32 uBuffer)
{
	delay_120ns();
	gpio_clr_mask(s_uChipSelectMask);
	spi_write_blocking(spi0, (const u8*)&uBuffer, 4);
	gpio_set_mask(s_uChipSelectMask);
}
