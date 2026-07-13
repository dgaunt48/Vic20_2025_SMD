; ------------------------------------------------------------------------------
; --- 																		 ---
; ------------------------------------------------------------------------------

SCREEN_RAM	:= $1000

; ------------------------------------------------------------------------------
; --- VIC - VIDEO INTERFACE CHIP
; ------------------------------------------------------------------------------
VIC		    := $9000
VIC_CR0     := VIC+$0		; Screen Origin X
VIC_CR1     := VIC+$1		; Screen Origin Y
VIC_CR2     := VIC+$2		; Columns / Screen Memory
VIC_CR3     := VIC+$3		; Rows / Raster Bit 0
VIC_RASTER  := VIC+$4		; Current Raster Line
VIC_CR5     := VIC+$5		; Character / Screen Address Base
VIC_SNDB    := VIC+$6		; Bass Audio
VIC_SNDT    := VIC+$7		; Tenor Audio
VIC_SNDA    := VIC+$8		; Alto Audio
VIC_SNDN    := VIC+$9		; Noise Audio
VIC_SNDVOL  := VIC+$A		; Volume & Auxiliary Colour
VIC_POTX    := VIC+$B		; Gamepaddle 1 (X-axis) analog value
VIC_POTY    := VIC+$C		; Gamepaddle 2 (Y-axis) analog value
VIC_LPX     := VIC+$D		; Light Pen Horizontal (X) position
VIC_LPY     := VIC+$E		; Light Pen Vertical (Y) position
VIC_COLOR   := VIC+$F		; Control Register F (Border & Background Colors)

; ------------------------------------------------------------------------------
; --- VIA 1 - VERSATILE INTERFACE ADAPTER
; ------------------------------------------------------------------------------
VIA1        := $9110        ; VIA1 base address
VIA1_PB     := VIA1+$0      ; Port register B
VIA1_PA1    := VIA1+$1      ; Port register A
VIA1_DDRB	:= VIA1+$2      ; Data direction register B
VIA1_DDRA   := VIA1+$3      ; Data direction register A
VIA1_T1CL   := VIA1+$4      ; Timer 1, low byte
VIA1_T1CH   := VIA1+$5      ; Timer 1, high byte
VIA1_T1LL   := VIA1+$6      ; Timer 1 latch, low byte
VIA1_T1LH   := VIA1+$7      ; Timer 1 latch, high byte
VIA1_T2CL   := VIA1+$8      ; Timer 2, low byte
VIA1_T2CH   := VIA1+$9      ; Timer 2, high byte
VIA1_SR     := VIA1+$A      ; Shift register
VIA1_ACR    := VIA1+$B      ; Auxiliary control register
VIA1_PCR    := VIA1+$C      ; Peripheral control register
VIA1_IFR    := VIA1+$D      ; Interrupt flag register
VIA1_IER    := VIA1+$E      ; Interrupt enable register
VIA1_PA2    := VIA1+$F      ; Port register A w/o handshake

; ------------------------------------------------------------------------------
; --- VIA 2 - VERSATILE INTERFACE ADAPTER
; ------------------------------------------------------------------------------
VIA2        := $9120       	; VIA2 base address
VIA2_PB     := VIA2+$0     	; Port register B
VIA2_PA1    := VIA2+$1     	; Port register A
VIA2_DDRB   := VIA2+$2     	; Data direction register B
VIA2_DDRA   := VIA2+$3     	; Data direction register A
VIA2_T1CL   := VIA2+$4     	; Timer 1, low byte
VIA2_T1CH   := VIA2+$5     	; Timer 1, high byte
VIA2_T1LL   := VIA2+$6     	; Timer 1 latch, low byte
VIA2_T1LH   := VIA2+$7     	; Timer 1 latch, high byte
VIA2_T2CL   := VIA2+$8     	; Timer 2, low byte
VIA2_T2CH   := VIA2+$9     	; Timer 2, high byte
VIA2_SR     := VIA2+$A     	; Shift register
VIA2_ACR    := VIA2+$B     	; Auxiliary control register
VIA2_PCR    := VIA2+$C     	; Peripheral control register
VIA2_IFR    := VIA2+$D     	; Interrupt flag register
VIA2_IER    := VIA2+$E     	; Interrupt enable register
VIA2_PA2    := VIA2+$F     	; Port register A w/o handshake

COLOUR_RAM	:= $9400

; ------------------------------------------------------------------------------
; --- Zero Page Allocation
; ------------------------------------------------------------------------------
TOP_OF_STACK	= $F8
TEMP    		= $FB       ; 1 byte for our number
SCREEN_PTR 		= $FC    	; 2 bytes for the 16-bit screen address

; ------------------------------------------------------------------------------
; --- CARTRIDGE HEADER
; ------------------------------------------------------------------------------
.segment "HEADER"
.word   COLD_START    	   	; Address $A000 -> Points to execution entry
.word   WARM_START       	; Address $A002 -> Points to warm start entry
.byte   $41, $30        	; "A0"
.byte   $C3, $C2, $CD   	; "CBM" (with high bits set)

.segment "CODE"

; ------------------------------------------------------------------------------
; --- 
; ------------------------------------------------------------------------------
COLD_START:
WARM_START:
	SEI               		; Disable Interrupts
	CLD                 	; Clear Decimal Mode
	LDX 	#TOP_OF_STACK
	TXS                 	; Initialise Stack Pointer
	
	LDA     #$00			; Clear Auxiliary Control Registers
	STA     VIA1_ACR
	STA     VIA2_ACR

	LDA     #$7F			; Clear Interrupt Enable Registers
	STA     VIA1_IER
	STA     VIA2_IER

.scope
	; Clear Zero Page ($0000-$00FF)
	LDX     #$00
	LDA     #$00
LOOP:
	STA     $00,X
	INX
	BNE     LOOP
.endscope

	JSR		CLEAR_SCREEN

.scope
	; Initialise All Vic I Registers
	LDX     #16
LOOP:
	LDA     VIC_DATA-1,X
	STA     VIC_CR0-1,X
	DEX
	BNE     LOOP
.endscope

.scope
	; Write "HELLO WORLD" To The Screen
	LDX     #8
LOOP:
	LDA     HEADING-1,X
	STA     SCREEN_RAM-1+5,X
	DEX
	BNE     LOOP
.endscope

	LDA     #$FF
	STA     VIA2_DDRB

	LDA     #$7F
	STA     VIA2_PB

	LDA 	#$1B
	STA 	VIC_COLOR
LOOP:

	LDA		VIA2_PB
	STA		TEMP
	ASL		A
	ROL		VIA2_PB

	JSR		HEXOUT
	JSR 	DELAY

;	LDA 	#$1A
;	STA 	VIC_COLOR
;	JSR		HEXOUT
;	INC		TEMP
;	JSR 	DELAY

	JMP 	LOOP

; ------------------------------------------------------------------------------
; --- DELAY
; ------------------------------------------------------------------------------
.proc DELAY
	LDX 	#$FF
OUTER:  
	LDY 	#$FF
INNER:  
	DEY
	BNE 	INNER
	DEX
	BNE 	OUTER
	RTS
.endproc

; ------------------------------------------------------------------------------
; --- CLEAR_SCREEN
; ------------------------------------------------------------------------------
.proc CLEAR_SCREEN
	LDA		#$20
	LDX		#$00
LOOP:
	STA		SCREEN_RAM,X
	STA		SCREEN_RAM+$100,X
	LDA		#$06
	STA		COLOUR_RAM,X
	STA		COLOUR_RAM+$100,X
	LDA		#$20
	INX		
	BNE		LOOP
	RTS
.endproc

; ------------------------------------------------------------------------------
; --- HEXOUT
; ------------------------------------------------------------------------------
HEXOUT:
	LDA 	#<SCREEN_RAM
	STA 	SCREEN_PTR
	LDA		#>SCREEN_RAM
	STA 	SCREEN_PTR+1
	LDY 	#0
	LDA 	TEMP
	LSR		a
	LSR	 	a
	LSR 	a
	LSR 	a
	JSR 	CONVERT_DIGIT
	INY
	LDA		TEMP
	AND		#$0f
	JSR		CONVERT_DIGIT
	RTS

; ------------------------------------------------------------------------------
; --- CONVERT_DIGIT
; ------------------------------------------------------------------------------
CONVERT_DIGIT:
	CMP 	#$0a    	; Is it 0-9 or A-F?
	BCC 	IS_NUM
	ADC		#$6	    	; Adjust for A-F
IS_NUM:
	ADC 	#$30    	; Add PETSCII offset for numbers ('0')
	STA		(SCREEN_PTR),Y
	RTS

; ------------------------------------------------------------------------------
; ---																		 ---
; ------------------------------------------------------------------------------
HEADING:
	.byte	"via test"

VIC_DATA:
	; PAL values shown below. Swap with commented values for NTSC testing.
	.byte   $0C				; $9000: Horiz. origin (PAL=$0C, NTSC=$05)
	.byte   $26				; $9001: Vert. origin  (PAL=$26, NTSC=$19)
	.byte   $16				; $9002: Columns (Bit 7=va9 video map, Bits 6-0=22 columns)
	.byte   $2E				; $9003: Rows/Char height (Bit 7=raster b0, Bits 6-1=23 rows, Bit 0=8x8 chars)
	.byte   $00				; $9004: Raster counter
	.byte   $C2				; $9005: Video/Char memory map (RAM $1000, Character ROM $8000)
	.byte   $00				; $9006: Light pen X
	.byte   $00				; $9007: Light pen Y
	.byte   $00				; $9008: Paddle X
	.byte   $00				; $9009: Paddle Y
	.byte   $00				; $900A: Audio Bass toggle/freq
	.byte   $00				; $900B: Audio Alto toggle/freq
	.byte   $00				; $900C: Audio Soprano toggle/freq
	.byte   $00				; $900D: Audio Noise toggle/freq
	.byte   $00				; $900E: Auxiliary color / Volume (Bits 7-4=Aux color, Bits 3-0=Vol)
	.byte   $1B				; $900F: Screen/Border color (Bits 7-4=Background, Bit 3=Reverse, Bits 2-0=Border)
