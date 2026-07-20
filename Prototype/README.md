# Vic20_2025_SMD_PROTOTYPE

## Added VIA 6522 to FPGA.
Hacked the hell out of the I/O board (all that mess on the left) to interface with an iCE40 FPGA. There should be just enough pins to add both VIA's to the $10 iCE40 that forms the core of this build.  Only VIA 2 (the keyboard scan and serial edge triggers) is running on the FPGA at the moment. A Real 6522 VIA (near the serial connector) is working together nicely with the FPGA VIA core to complete the serial I/O.

![New VIA Core](Images/Added_6522_FPGA.jpg?raw=true "New VIA Core")

## First Prototype Board.
RP2350A is only there as a HDMI encoder.
ICE40 FPGA has main 5k RAM, Colour RAM, Character ROM and all glue logic.
RP2350B configures FPGA core and 128k RAM chip used For Expansion RAM / ROM on 6502 Side Of Bus.  RAM IC gets copys of ROMS (including KERNAL and BASIC) before banking and R/W access is enforced.  

![First Prototype](Images/First_Prototype.jpg?raw=true "First Prototype Board")

## First Experiments With The Vic20

![First Experiments](Images/First_Experiments.jpg?raw=true "First Experiments With The Vic20")
