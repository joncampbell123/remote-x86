
; stage1 executes us within our own segment, 16-bit real mode
; when linked together, this must be FIRST!
;
; we're jumped to from stage 1 to 0x0000:0x8000. basing segments
; from zero allows us to reference everything within the first
; 64K absolutely, and also for the other parts like the 32-bit
; protected mode part, to do absolute references to memory.

bits 16
use16

CODE_SELECTOR		equ		0x08
DATA_SELECTOR		equ		0x10

SECTION			.text
global			_start

extern			c_start

; code starts here
_start:			push		cs
			pop		ds
			push		cs
			pop		es
			push		cs
			pop		ss
			cli
			cld

			mov		esp,0x7BF8				; prepare stack

; unlike our 8086 counterpart we go straight into 80386 32-bit protected mode.
; if your CPU doesn't support that, then too bad.
			mov		word [_gdtr],((32 * 8) - 1)		; limit
			mov		dword [_gdtr+2],_gdt			; base

			mov		edi,_gdt + CODE_SELECTOR
			mov		word [edi+0],0xFFFF			; limit 0:15
			mov		word [edi+2],0x0000			; base 0:15
			mov		word [edi+4],0x9A00			; base 16:23 and access flags: code, priv=0 readable
			mov		word [edi+6],0x00CF			; base 24:31 and flags (4GB large, default i.e. 32-bit), limit 16:19

			mov		edi,_gdt + DATA_SELECTOR
			mov		word [edi+0],0xFFFF			; limit 0:15
			mov		word [edi+2],0x0000			; base 0:15
			mov		word [edi+4],0x9200			; base 16:23 and access flags: data, priv=0 writable
			mov		word [edi+6],0x00CF			; base 24:31 and flags (4GB large, default i.e. 32-bit), limit 16:19

			sgdt		[_gdtr_old]
			lgdt		[_gdtr]

			mov		eax,0x00000001
			mov		cr0,eax					; switch on protected mode

			jmp		dword CODE_SELECTOR:.prot32		; leap of faith
use32
.prot32:		mov		ax,DATA_SELECTOR
			mov		ds,ax
			mov		es,ax
			mov		ss,ax

; pass control to the C subroutine
			jmp		c_start

; other utility functions
global hang
hang:			cli
			hlt
			jmp		short hang

; the GDT goes here
_gdtr_old		dd		0,0
_gdtr			dd		0,0
_gdt			times (32 * 8) db 0

; end
			align		16
