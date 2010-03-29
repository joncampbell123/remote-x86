
; UD exception test

; test whether an invalid opcode generates an exception.
; on an 8086, this would not cause anything

; real mode
		use16
		org		0

had_ud:		db		0xFF
had_86skip:	db		0xFF
		db		0,0

; code begins here at offset 0x0004
entry:		push		ds		; save DS, set DS=CS
		mov		ax,cs
		mov		ds,ax

		push		es		; save ES, set ES=0
		xor		ax,ax
		mov		es,ax

		cli				; make sure interrupts are off

; real mode: write over INT 06h to catch UD#
		mov		di,6*4
		mov		word [es:di],ud_vector
		mov		word [es:di+2],cs

; OK do an illegal instruction
		mov		ax,sp
		mov		byte [cs:had_ud],0
		mov		byte [cs:had_86skip],0
		push		cs
		db		0x0F,0x0B	; the official UD2 instruction
		nop
		db		0x00,0x00
		nop
		mov		byte [cs:had_86skip],1
; an 8086 would execute that as:
;	POP	CS
;	OR	DX,[BX+SI+0]
; while later ones would do:
;       UD2
;       ADD	[BX+SI],AL
; either way the CPU safely continues executing
ud_exit:	mov		sp,ax

; clean up and return
		pop		es
		pop		ds
		retf

ud_vector:	mov		byte [cs:had_ud],1
		add		sp,6		; throw away interrupt frame
		jmp		short ud_exit

