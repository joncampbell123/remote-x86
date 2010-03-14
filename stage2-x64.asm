use64

CODE_SELECTOR	equ		0x08
DATA_SELECTOR	equ		0x10

SECTION		.text

; we are jumped to from the 8086 loader, so this part is real-mode code
global _jmp_x64
use16			; <- Woo! Nasm lets me use 16-bit code in an ELF64 object file!

_jmp_x64:	cli
		mov		ax,cs
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
		mov		sp,0x7BF0

		mov		word [x_gdtr_32],0xFFFF	; limit
		mov		dword [x_gdtr_32+2],_gdt ; offset
		call		gen_gdt_32

		sgdt		[x_gdtr_old]
		lgdt		[x_gdtr_32]
		mov		eax,cr0
		or		al,1
		mov		cr0,eax
		jmp		dword CODE_SELECTOR:.protmode32
use32
.protmode32:

		jmp		short $

; generate 386 GDT
use16
gen_gdt_32:	mov		di,_gdt
		cld
		xor		ax,ax
; #0 NULL entry
		stosw
		stosw
		stosw
		stosw
; #1 code
	; 0:15
		mov		cl,4
		mov		ax,0xFFFF		; limit
		stosw
	; 16:31
		xor		ax,ax			; base
		stosw
	; 32:47
		xor		al,al
		mov		ah,0x9A			; access = present, ring 0, executable, readable
		stosw
	; 48:63
		mov		ax,0x00CF		; base 24:31 granular 4KB pages, 32-bit selector
		stosw
; #2 data
	; 0:15
		mov		cl,4
		mov		ax,0xFFFF		; limit
		stosw
	; 16:31
		xor		ax,ax			; base
		stosw
	; 32:47
		xor		al,al
		mov		ah,0x92			; access = present, ring 0, readable/writeable, data
		stosw
	; 48:63
		mov		ax,0x00CF		; base 24:31 granular 4KB pages, 32-bit selector
		stosw
; done
		ret

x_gdtr_old:	dd		0,0
x_gdtr_32:	dd		0,0
x_gdtr_64:	dd		0,0,0,0,0,0,0,0		; <- how long is it?

align		16
_gdt		times (32 * 8) - ($-$$) db 0

extern _jmp_8086

