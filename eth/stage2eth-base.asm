
; stage1 executes us within our own segment, 16-bit real mode
; when linked together, this must be FIRST!
;
; we're jumped to from stage 1 to 0x0000:0x8000. basing segments
; from zero allows us to reference everything within the first
; 64K absolutely, and also for the other parts like the 32-bit
; protected mode part, to do absolute references to memory.

bits 16
use16

SECTION			.text
global			_start

; code starts here
_start:			push		cs
			pop		ds
			push		cs
			pop		es

; unlike our 8086 counterpart we go straight into 80386 32-bit protected mode.
; if your CPU doesn't support that, then too bad.
			

; END
			jmp		short $

; the GDT goes here
_gdtr_old		dd		0,0
_gdtr			dd		0,0
_gdt			times (32 * 8) db 0

; end
			align		16
