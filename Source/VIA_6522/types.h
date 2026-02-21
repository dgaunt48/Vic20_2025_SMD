//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
#ifndef __types_h_included
#define __types_h_included

#include <stdbool.h>

typedef unsigned char   u8;
typedef unsigned short  u16;
typedef unsigned int    u32;

//------------------------------------------------------------------------------------------------
//----  nop = 1,000,000,000 / 125,000,000 = 8 ns       RP2040                                 ----
//----  minumum write pulse width = 40 ns ... = 5 nop's                                       ----
//----                                                                                        ----
//----  nop = 1,000,000,000 / 150,000,000 = 6 ns       RP2350                                 ----
//----  minumum write pulse width = 40 ns ... = 7 nop's                                       ----
//------------------------------------------------------------------------------------------------
static inline void delay_40ns(void)
{
    asm volatile("nop \n nop \n nop \n nop \n nop \n nop \n nop");
    asm volatile("nop \n nop");
}

//------------------------------------------------------------------------------------------------
//----  nop = 1,000,000,000 / 125,000,000 = 8 ns       RP2040                                 ----
//----  minumum write pulse width = 120 ns ... = 15 nop's                                     ----
//----                                                                                        ----
//----  nop = 1,000,000,000 / 150,000,000 = 6 ns       RP2350                                 ----
//----  minumum write pulse width = 120 ns ... = 20 nop's                                     ----
//------------------------------------------------------------------------------------------------
static inline void delay_120ns(void)
{
    asm volatile("nop \n nop \n nop \n nop \n nop");
    asm volatile("nop \n nop \n nop \n nop \n nop");
    asm volatile("nop \n nop \n nop \n nop \n nop");
    asm volatile("nop \n nop \n nop \n nop \n nop");
}

#endif /* __types_h_included */
