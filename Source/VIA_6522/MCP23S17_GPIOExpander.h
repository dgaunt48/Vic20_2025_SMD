//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
#ifndef __MCP23S17_GPIOExpander_h_included
#define __MCP23S17_GPIOExpander_h_included

#include "types.h"
#include "hardware/spi.h"

// Internal functions DO NOT USE DIRECTLY !!!!
u8 MCP23S17_ReadRegSingle(const u32 uBuffer);
void MCP23S17_WriteRegSingle(const u32 uBuffer);
void MCP23S17_WriteRegPair(const u32 uBuffer);
// -------------------------------------------

#define MCP23S17_DEVICE_READ            (65)
#define MCP23S17_DEVICE_WRITE           (64)
#define MCP23S17_DEVICE_ADDRESS_SHIFT   (1)

#define MCP23S17_IOCON_HAEN             (1 << 3)
#define MCP23S17_IOCON_SEQOP            (1 << 5)

enum eMCP23S17
{
    MCP23S17_IODIRA = 0,
    MCP23S17_IODIRB,
    MCP23S17_IPOLA,
    MCP23S17_IPOLB,
    MCP23S17_GPINTENA,
    MCP23S17_GPINTENB,
    MCP23S17_DEFVALA,
    MCP23S17_DEFVALB,
    MCP23S17_INTCONA,
    MCP23S17_INTCONB,
    MCP23S17_IOCON,
    MCP23S17_IOCON_MIRROR,
    MCP23S17_GPPUA,
    MCP23S17_GPPUB,
    MCP23S17_INTFA,
    MCP23S17_INTFB,
    MCP23S17_INTCAPA,
    MCP23S17_INTCAPB,
    MCP23S17_GPIOA,
    MCP23S17_GPIOB,
    MCP23S17_OLATA,
    MCP23S17_OLATB
};

bool MCP23S17_Initialise(spi_inst_t* pSpi, const u32 uBaudRate, const u32 uClkPin, const u32 uTxPin, const u32 uRxPin, const u32 uCsPin);

static inline u8 MCP23S17_ReadRegisterSingle(const u32 uSPIAddress, const u32 uRegister)
{
    return MCP23S17_ReadRegSingle((uRegister << 8) | MCP23S17_DEVICE_READ | (uSPIAddress << MCP23S17_DEVICE_ADDRESS_SHIFT));
}

static inline void MCP23S17_WriteRegisterSingle(const u32 uSPIAddress, const u32 uRegister, const u32 uData)
{
    MCP23S17_WriteRegSingle((uData << 16) | (uRegister << 8) | MCP23S17_DEVICE_WRITE | (uSPIAddress << MCP23S17_DEVICE_ADDRESS_SHIFT));
}

static inline void MCP23S17_WriteRegisterPair(const u32 uSPIAddress, const u32 uRegister, const u32 uData1, const u32 uData2)
{
    MCP23S17_WriteRegPair((uData2 << 24) | (uData1 << 16) | (uRegister << 8) | MCP23S17_DEVICE_WRITE | (uSPIAddress << MCP23S17_DEVICE_ADDRESS_SHIFT));
}

#endif /* __MCP23S17_GPIOExpander_h_included */
