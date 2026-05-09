# Source Code For Vic 20 SMD Recreation
The build presently requires real 6522 VIA ic's.  This code is all experimental.

## VIA_6522
Start of a software emulated 6522.  Will probably end up with the VIA's on the iCE40,
but wanted to do some tests to see if emulation is a viable alternative.

## VIA_6522_Tester
RP2350 program to bitbang test conditions into a real 6522.  At the moment it is
only setting up timer 1 to loop forever, triggering IRQ interrupts every 6 clocks.

![6522 Running Timer 1](Images/Timer_1.png?raw=true "6522 Running Timer 1")
