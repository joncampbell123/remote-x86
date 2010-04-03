
; real mode, or 16-bit protected mode
		use16
		org		0

; enable A20 using the keyboard controller.
; don't verify, the cpu_test control program will remotely read memory afterwards
; and check to see if memory contents alias like A20 is off
		call		empty_8042
		mov		al,0xD1			; command byte write
		out		0x64,al

		call		empty_8042
		mov		al,0xDF			; enable A20, don't reset CPU
		out		0x60,al

; return
		retf

; wait for 8042 to empty (both ends!)
empty_8042:	push		cx
		push		ax
		xor		cx,cx
		dec		cx			; CX = 0xFFFF
.l:		in		al,0x64			; read status
		test		al,1			; any data?
		jz		.nomorer
		in		al,0x60			; suck up data, discard it
		loop		.l			; if (--CX != 0) then look for more
.nomorer:	in		al,0x64			; read status
		test		al,2			; can I write yet? output buffer full?
		jz		.canw			; 
		loop		.nomorer		; if (--CX != 0) then wait for write ready
.canw:		pop		ax
		pop		cx
		ret

