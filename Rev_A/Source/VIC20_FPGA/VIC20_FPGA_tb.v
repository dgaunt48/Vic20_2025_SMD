//------------------------------------------------------------------------------------------------
//---- VIC20_FPGA_TB.v - 2026 Dave Gaunt	                                               	  ----
//------------------------------------------------------------------------------------------------
//---- v1.0 - Test Bench For Vic20 FPGA CORE					                              ----
//------------------------------------------------------------------------------------------------

`default_nettype none
`define DUMPSTR(x) `"x.vcd`"
`timescale 10ns / 1ns

`define TEST_TIMER_1
`define TEST_PORTS
`define TEST_CAB

`include "/APIO/iCE40/VIA_6522/VIARegisters.vh"

module VIC20_FPGA_TB();

parameter DURATION = 3000;

reg bFPGACoreClock = 0;					// 25 Mhz FPGA Core Clock
always #2 bFPGACoreClock = ~bFPGACoreClock;

reg bPhase2Clock = 0;					// Vic20 1,108,404 Hz Clock
always #50 bPhase2Clock = ~bPhase2Clock;

reg bReset_n = 0;
reg bCS = 0;
reg bCS_n = 1;
reg bRead = 1;
reg [3:0] nRS = 0;
reg bCA1 = 1;
reg bCB1 = 1;

wire bCA2;
wire bCB2;
wire [7:0] nData;
wire [7:0] nPortA;
wire [7:0] nPortB;
wire bIRQ_n;

reg [7:0] nWriteData = 0;

genvar i;
generate
    for (i = 0; i < 8; i = i + 1)
	begin // : pullup_bus
        pullup (nPortA[i]);
        pullup (nPortB[i]);
    end
endgenerate

assign nData = bRead ? 8'bz : nWriteData;

VIA_6522 UUT (
	.bFPGACoreClock(bFPGACoreClock),
	.bPhase2Clock(bPhase2Clock),
	.bReset_n(bReset_n),
	.bCS(bCS),
	.bCS_n(bCS_n),
	.bRead(bRead),
	.nRS(nRS[3:0]),
	.bCA1(bCA1),
	.bCB1(bCB1),

	.bCA2(bCA2),
	.bCB2(bCB2),
	.nData(nData[7:0]),
	.nPortA(nPortA[7:0]),
	.nPortB(nPortB[7:0]),

	.bIRQ_n(bIRQ_n)
);

initial begin
	$dumpvars(0, VIC20_FPGA_TB);

	#50 bReset_n = 1;					// Release VIA From Reset
	#51

	#45 bCS = 1;
	bCS_n = 0;
	nRS = VIA_REG_IER;
	nWriteData = 8'hC5;
	bRead = 0;
	#55 bCS = 0;
	bCS_n = 1;
	bRead = 1;

	#45 bCS = 1;						// Disable Interrupts VIA_IER_CA2, VIA_IER_SR 0x40
	bCS_n = 0;
	nRS = VIA_REG_IER;
	nWriteData = 8'h05;
	bRead = 0;
	#55 bCS = 0;
	bCS_n = 1;
	bRead = 1;

	#45 bCS = 1;						// Read Interrupt Enable Register
	bCS_n = 0;
	nRS = VIA_REG_IER;
	#55 bCS = 0;
	bCS_n = 1;

	#(DURATION) $display("End of simulation");
	$finish;
end

endmodule
