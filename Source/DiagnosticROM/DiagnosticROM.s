
SCREEN_RAM	= $1000
VIC_BASE    = $9000     	; Base register address for MOS 6560/6561
VIA1_BASE   = $9110     	; VIA 1 (User Port / RS-232 / Control Port)
VIA2_BASE   = $9120     	; VIA 2 (Keyboard / Cassette / Jiffy Clock)
COLOUR_RAM	= $9400

.segment "HEADER"
.word   ColdStart    	   	; Address $A000 -> Points to execution entry
.word   WarmStart       	; Address $A002 -> Points to warm start entry
.byte   $41, $30        	; "A0"
.byte   $C3, $C2, $CD   	; "CBM" (with high bits set)

.segment "CODE"

ColdStart:
WarmStart:
	SEI               		; Disable interrupts
	CLD                 	; Clear decimal mode
	LDX 	#$FF
	TXS                 	; Initialize stack pointer
	
	LDA     #$00
	STA     VIA1_BASE+$0B   ; VIA 1 Auxiliary Control Register -> 0
	STA     VIA1_BASE+$0E   ; VIA 1 Interrupt Enable Register (Clear all bits)
	STA     VIA2_BASE+$0B   ; VIA 2 Auxiliary Control Register -> 0
	STA     VIA2_BASE+$0E   ; VIA 2 Interrupt Enable Register (Clear all bits)
	
	; Enable latching/disable interrupts clearly by writing with MSB low
	LDA     #$7F
	STA     VIA1_BASE+$0E   ; Clear all interrupt enable bits on VIA 1
	STA     VIA2_BASE+$0E   ; Clear all interrupt enable bits on VIA 2

.scope
	; Clear Zero Page ($0000-$00FF) to guarantee a known memory state
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
	STA     VIC_BASE-1,X
	DEX
	BNE     LOOP
.endscope

.scope
	; Write "HELLO WORLD" To The Screen
	LDX     #11
LOOP:
	LDA     HEADING-1,X
	STA     SCREEN_RAM-1,X
	DEX
	BNE     LOOP
.endscope

LOOP:
	LDA 	#$1B
	STA 	VIC_BASE+$0F  	; Store in VIC border/screen register
	JSR 	DELAY           ; Wait a fraction of a second
	LDA 	#$1A
	STA 	VIC_BASE+$0F    ; Store it
	JSR 	DELAY           ; Wait again
	JMP 	LOOP            ; Repeat infinitely

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

HEADING:
	.byte	"hello world"

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
