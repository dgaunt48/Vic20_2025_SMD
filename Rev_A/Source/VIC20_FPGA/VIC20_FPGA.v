//------------------------------------------------------------------------------------------------
//---- VIC20_FPGA.v - 2023 Dave Gaunt	                                                      ----
//------------------------------------------------------------------------------------------------
//---- v0.1 - Vic20 FPGA Core                				                              	  ----
//------------------------------------------------------------------------------------------------

// FPGA Usage 395 LC 5% @ 144 Mhz

`include "/APIO/iCE40/VIA_6522/VIA_6522.v"

module VIC20_FPGA(
	input wire bFPGACoreClock,
	input wire bPhase2Clock,
	input wire bReset_n,
	input wire bCS,
	input wire bCS_n,
	input wire bRead,
	input wire [3:0] nRS,
	input wire bCA1,
	input wire bCB1,

	inout wire bCA2,
	inout wire bCB2,
	inout wire [7:0] nData,
	inout wire [7:0] nPortA,
	inout wire [7:0] nPortB,

	output wire bIRQ_n
);

VIA_6522 u_Serial_VIA(
	.bFPGACoreClock(bFPGACoreClock),
	.bPhase2Clock(bPhase2Clock),
	.bReset_n(bReset_n),
	.bCS(bCS),
	.bCS_n(bCS_n),
	.bRead(bRead),
	.nRS(nRS),
	.bCA1(bCA1),
	.bCB1(bCB1),
	.bCA2(bCA2),
	.bCB2(bCB2),
	.nData(nData),
	.nPortA(nPortA),
	.nPortB(nPortB),
	.bIRQ_n(bIRQ_n)
);

always @ (posedge bFPGACoreClock)
begin
	if (0 == bReset_n)
	begin

	end
	else
	begin

	end
end

endmodule
