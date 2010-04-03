
; real mode, or 16-bit protected mode
		use16
		org		0

; go
start:		in		al,0x92
		test		al,2
		jnz		.already
		or		al,2
		out		0x92,al
.already:

; return
		retf

