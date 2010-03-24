
	org		0
	use16

_start:	mov		ax,0xB800
	mov		es,ax
	xor		dx,dx

l1:	mov		cx,80*25
	mov		ax,dx
	xor		di,di

l2:	stosw
	inc		ax
	loop		l2

	inc		dx
	cmp		dx,0x400
	jb		l1

	retf

