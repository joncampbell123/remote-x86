use16
org		0x7800

		cli
		xor		ax,ax
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
		mov		sp,0x77FF

; the BIOS should have loaded the rest for us
		jmp		0x0000:0x8000

		times		2048-($-$$) db 0

