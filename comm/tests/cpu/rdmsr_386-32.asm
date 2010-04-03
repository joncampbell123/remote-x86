
; RDMSR
;
; 32-bit protected mode.
; must be loaded at fixed address 0x40000.
; this creates and loads it's own because the debugger might
; be the RS-232 version which doesn't load IDTR. Or, it might
; be the ethernet version which does.
;
; This code allows the host to read the client's CPUID values
		use32
		org		0x40000

reg:		dd		0
except:		dd		0
r_eax:		dd		0
r_edx:		dd		0

; code begins here at offset 0x0010
entry:		cli
		sidt		[idtr]			; save debugger's
		lidt		[idtr_new]		; load our own
		mov		[saved_esp],esp

		; clear previous result
		xor		eax,eax
		mov		[except],eax
		mov		[r_eax],eax
		mov		[r_edx],eax

		; modify INT 6, in case RDMSR is unsupported
		mov		esi,[idtr_new+2]	; we want the IDT offset
		add		esi,6*8
		mov		ebx,ud_exception
		mov		word [esi],bx
		shr		ebx,16
		mov		ax,cs
		mov		word [esi+2],ax
		mov		byte [esi+4],0
		mov		byte [esi+5],0x8E
		mov		word [esi+6],bx

		; modify INT 0xD, in case RDMSR causes a GPF
		mov		esi,[idtr_new+2]	; we want the IDT offset
		add		esi,13*8
		mov		ebx,gpf_exception
		mov		word [esi],bx
		shr		ebx,16
		mov		ax,cs
		mov		word [esi+2],ax
		mov		byte [esi+4],0
		mov		byte [esi+5],0x8E
		mov		word [esi+6],bx

		; do it
		mov		ecx,[reg]
		rdmsr
		mov		[r_eax],eax
		mov		[r_edx],edx

		; done
return_path:	lidt		[idtr]			; restore debugger's IDTR
		mov		esp,[saved_esp]
		ret

		align		4
saved_esp:	dd		0
idtr:		dw		0,0,0,0
idtr_new:	dw		0x7FF
		dd		idt_table

		align		4
ud_exception:	or		dword [except],1 << 6
		jmp		return_path

		align		4
gpf_exception:	or		dword [except],1 << 13
		jmp		return_path

		align		16
idt_table:

