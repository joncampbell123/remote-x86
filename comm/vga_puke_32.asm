
	org		0
	use32

_start:	xor		edx,edx

l1:	mov		ecx,80*25
	mov		eax,edx
	mov		edi,0xB8000

l2:	stosw
	inc		eax
	loop		l2

	inc		edx
	cmp		edx,0x400
	jb		l1

	ret

