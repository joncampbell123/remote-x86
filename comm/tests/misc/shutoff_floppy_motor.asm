
; real mode
		use16
		org		0

; start here at +0
		mov		dx,0x3F2		; floppy Digital Output Register (motor control)
		in		al,dx
		and		al,0xF			; clear bits 4-7 turning off all motors
		out		dx,al

; return
		retf

