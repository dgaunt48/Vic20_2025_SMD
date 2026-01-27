# Vic20_2025_SMD
Vic20 Recreation using only parts currently manufactured.

NOTE: Original (PAL) VIC chip is still required in first prototype, will be optional in next revision.

Hooked up the test harness and passed all tests in DiagROM.

![Passed DiagROM](Pics/Passed_DiagROM.jpg?raw=true "Passed Diag ROM")

First Prototype Board.

RP2350A is only there as a HDMI encoder.
ICE40 FPGA has main 5k RAM, Colour RAM, Character ROM and all glue logic.
RP2350B configures FPGA core and 128k RAM chip used For Expansion RAM / ROM on 6502 Side Of Bus.  RAM IC gets copys of ROMS (including KERNAL and BASIC) before banking and R/W access is enforced.

![First Prototype](Pics/First_Prototype.jpg?raw=true "First Prototype Board")

First Experiments With The Vic20

![First Experiments](Pics/First_Experiments.jpg?raw=true "First Experiments With The Vic20")
