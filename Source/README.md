# Source Code For Vic 20 SMD Recreation
VIA FPGA Core is now running except for unimplemented timer 2.  Will be ported into this project when the new IO board design arrives from JLC.

## DiagnosticROM
Cut down diagnostic 6502 source so i can target individual test conditions.  Full Source 
to vc-20-diag.324173-01_blk5.bin is in my CBM_Flash_ROMs\Vic20_DiagROM repository.

## VIA_6522
Start of a software emulated 6522.  Will probably end up with the VIA's on the iCE40,
but wanted to do some tests to see if emulation is a viable alternative.

## VIA_6522_Snoop
Rp2350 Bus Snoop program to display all register reads and writes to both VIA IC's on a running Vic20.

## VIA_6522_Tester
RP2350 program to bitbang test conditions into a real 6522.  At the moment it is
only setting up timer 1 to loop forever, triggering IRQ interrupts every 6 clocks.

![6522 Running Timer 1](Images/Timer_1.png?raw=true "6522 Running Timer 1")
