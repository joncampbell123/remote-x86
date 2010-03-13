
; we are a boot sector and we run from 0x7C00
use16
org 0x7C00

; well, ARE we running from 0x0000:0x7C00?
	call	s1a
s1a:	pop	bx
	mov	ax,cs			; AX:BX = CS:IP = base of this code + 3
	mov	ds,ax
	or	ax,ax			; CS must be zero
	jnz	s1err
	cmp	bx,0x7C00+3		; BX = IP = our base + 3
	jnz	s1err
	jmp	short s1pass
s1err:	lea	si,[bx + s1errmsg - (0x7C00+3)] ; remember we don't know where the fuck we are
	call	puts			; NASM will make this a relative-address jump
	jmp	short $
s1errmsg: db	'Non-standard boot location',0
s1pass: 				; test OK!

; okay, decide how we're going to load the rest of ourself.
; which floppy drive?
	; TODO. usually, we're on a floppy at 0x00

; what is your geometry?
	mov	ah,8
	mov	dl,[boot_drive]
	int	13h
	jc	geo_err			; CF means error
	or	ah,ah
	jnz	geo_err			; AH != 0 means error
; ok! get the info
	inc	ch
	mov	[boot_c],ch
	mov	[boot_s],cl
	inc	dh
	mov	[boot_h],dh
	jmp	geo_ok
; error
geo_err:
	mov	si,geo_errmsg
	call	puts
	jmp	short $
geo_errmsg: db	'Geometry N/A',0
geo_ok:					; test OK!

; OK start reading.
; load contents to 0x0000:0x8000
	mov	bx,(0x8000 >> 4)	; 0x0800
	mov	es,bx
	xor	bx,bx			; 0x0800:0x0000 or 0x0000:0x8000
load_loop:
	mov	ah,2
	mov	al,1
	mov	ch,[read_c]
	mov	cl,[read_s]
	mov	dh,[read_h]
	mov	dl,[boot_drive]
	int	13h
	jc	read_errmsg
	add	bx,0x200		; advance 512 bytes
	jnc	load_loop2
	mov	bx,es			; if BX overflows, advance ES 64KB further
	add	bx,0x1000
	mov	es,bx
load_loop2:
	dec	word [stage2_size]
	jz	load_done

	mov	al,byte [boot_s]
	inc	byte [read_s]
	cmp	byte [read_s],al	; is read_s > boot_s
	jle	load_loop
	mov	byte [read_s],1

	mov	al,byte [boot_h]
	inc	byte [read_h]
	cmp	byte [read_h],al	; is read_h >= boot_h
	jl	load_loop
	mov	byte [read_h],0

	inc	byte [read_c]
	jmp	load_loop		; fat chance we overrun the number of cylinders

load_done:
	sti
	jmp	0x0000:0x8000

; read error
	mov	si,read_errmsg
	call	puts
	jmp	short $
read_errmsg: db	'Read error',0

; API
puts:	clc
	push	ax
	push	si
puts1:	lodsb
	or	al,al
	jz	putse
	mov	ah,0Eh
	int	10h
	jmp	puts1
putse:	pop	si
	pop	ax
	ret

; variables
boot_drive	db	0		; which drive? (usually, the floppy)
boot_c		db	0
boot_h		db	0
boot_s		db	0
; vars preset to read from C/H/S 0/0/2 onward
read_c		db	0
read_h		db	0
read_s		db	2

; boot signature + byte count filled in by make script
times (510-2) - ($-$$) db 0
stage2_size	dw	40
		dw	0xAA55

