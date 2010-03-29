
; announce86
; subroutine to print a message on-screen using INT 10h.
; the debugger host indicates where by overwriting the
; first 2 words of this code with the segment:offset
; value

; real mode
		use16
		org		0

str_ofs		dw		0
str_seg		dw		0

; code begins here at offset 0x0004
entry:		push		ds

		mov		si,[cs:str_ofs]
		mov		ds,[cs:str_seg]
		cld

lop:		lodsb
		or		al,al
		jz		lope
		mov		ah,0Eh
		int		10h
		jmp		lop
lope:		

		cli
		pop		ds
		retf

