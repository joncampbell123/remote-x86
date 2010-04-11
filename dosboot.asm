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

; make sure that our segment is far enough away from the DOS kernel.
; testing shows that if we're too close, our jump to the code never
; makes it and we crash, or the stage2 overwrites itself at the
; instruction pointer when it moves itself downward. This can happen
; for example if you make a Windows 95 bootdisk with absolutely no
; drivers or TSRs active, segment values as low as 0xA00 are possible.
		mov	ax,cs
		cmp	ax,0x1000
		jae	seg_ok

; we're too low in memory, we need to copy ourself and run from a higher
; memory address. copy ourself 64K higher into memory. again, DOS gives
; COM programs all system memory until further notice, so stomping on
; something else is not a problem unless you are severely restricted
; in available DOS memory
		mov	ds,ax
		add	ax,0x1000
		mov	es,ax
		xor	si,si
		mov	di,si
		mov	cx,payload_end		; damn you NASM I wish I could write payload_end>>1 and REP MOVSW :(
		cld
		rep	movsb
		
		push	es
		mov	ax,seg_ok
		push	ax
		retf				; jump to the new segment

seg_ok:
; make sure the data following the image is zeroed.
; don't worry, DOS allocates all memory to the current program by default,
; this is unlikely to stomp on anything unless you're severely out of memory
		mov	ax,cs
		mov	es,ax
		mov	di,payload_end
		xor	ax,ax
		mov	cx,2048
		rep	stosw

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
incbin		"stage2-recomm.bin"
payload_end:

