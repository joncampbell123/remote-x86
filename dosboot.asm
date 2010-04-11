; real-mode
; 
; Prompt user, then kick MS-DOS out of memory and run our payload,
; the serial debugger

use16

		org	0x100		; this is a .COM executable

start:		mov	ax,cs
		mov	ds,ax

		mov	dx,message
		mov	ah,9
		int	21h

; user's response?
promptl:	mov	ah,1
		int	21h
		cmp	al,'y'
		jz	doit
		cmp	al,'Y'
		jz	doit
		jmp	dos_exit

doit:		mov	dx,okay
		mov	ah,9
		int	21h

; execute it.
; Most of the payload is written to execute from 0x0000:0x8000
; but the initial part that prompts the user, by design, will
; safely run from any segment. The reason we have to do that
; has to do with the DOS kernel or some TSR who may have hooked
; INT 16h or INT 10h. If we just stomp on DOS and then try to
; call INT 16h, we'll crash!
;
; Instead, after prompting, the code will autodetect that it's
; running from the wrong place and move itself into the right
; place.
		mov	ax,cs
		mov	bx,payload
		mov	cl,4
		shr	bx,cl
		add	ax,bx
		sub	ax,0x800
		push	ax		; segment
		mov	ax,0x8000
		push	ax		; offset
		retf			; go there

dos_exit:	mov	ax,4C00h
		int	21h

message:	db	'Remote-x86 serial debugger launcher (C) 2009-2010 Jonathan Campbell',13,10
		db	'WARNING: When this program becomes active, MS-DOS and the surrounding OS',13,10
		db	'         will be removed from memory and inactive until next reboot.',13,10
		db	'         The computer will remain under complete remote control in that time.',13,10
		db	'         Press Y if thats what you really want to do.',13,10
		db	'$'

okay:		db	13,10
		db	'Alrighty, here we go...',13,10
		db	'$'

		align	16
payload:
incbin		"stage2.bin"		; assumption: any machine that needs DOS as a bootloader probably can't do x86-64
payload_end:

