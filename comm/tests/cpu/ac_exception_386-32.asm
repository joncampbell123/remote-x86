
; AC exception test
;
; 32-bit protected mode.
; must be loaded at fixed address 0x40000.
; this creates and loads it's own because the debugger might
; be the RS-232 version which doesn't load IDTR. Or, it might
; be the ethernet version which does.
;
; On the 486, this intentionally creates a situation where an
; Alignment Check exception would occur. We catch it, and note
; it where the host program can see the results.
		use32
		org		0x40000

DPL3_DATA	EQU		(6*8)
DPL3_CODE	EQU		(7*8)
RETURN_GATE	EQU		(8*8)

DATA_SELECTOR	EQU		0x10
CODE_SELECTOR	EQU		0x08

has:		dd		0

; code begins here at offset 0x0004
entry:		cli
		sidt		[idtr]			; save debugger's
		lidt		[idtr_new]		; load our own
		sgdt		[gdtr]			; we need the GDT
	
		; AC won't trigger unless we move out of Ring 0 (momentarily become non-kernel)
		mov		esi,[gdtr+2]
		add		esi,DPL3_DATA		; start at 6, the debugger takes the first 5
		mov		word [esi+0],0xFFFF	; limit 0...15
		mov		word [esi+2],0x0000	; base 0...15
		mov		byte [esi+4],0x00	; base 16...23
		mov		byte [esi+5],0x92|(3<<5) ; present, read/write, data, DPL=3
		mov		byte [esi+6],0xCF	; G=1, B=1
		mov		byte [esi+7],0x00	; base 24...31

		mov		esi,[gdtr+2]
		add		esi,DPL3_CODE		; 
		mov		word [esi+0],0xFFFF	; limit 0...15
		mov		word [esi+2],0x0000	; base 0...15
		mov		byte [esi+4],0x00	; base 16...23
		mov		byte [esi+5],0x9A|(3<<5) ; present, read/write, code, DPL=3
		mov		byte [esi+6],0xCF	; G=1, B=1
		mov		byte [esi+7],0x00	; base 24...31

		mov		bx,cs
		mov		eax,ring0_return
		mov		esi,[gdtr+2]
		add		esi,RETURN_GATE		; 
		mov		word [esi+0],ax		; offset 0...15
		shr		eax,16
		mov		word [esi+2],bx		; selector
		mov		byte [esi+4],0x00	; 0 dwords
		mov		byte [esi+5],0x9C|(3<<5) ; present, gate, DPL=3
		mov		word [esi+6],ax		; offset 16...31

		; modify INT 17
		mov		esi,[idtr_new+2]	; we want the IDT offset
		add		esi,17*8
		mov		ebx,ac_exception
		mov		word [esi],bx
		shr		ebx,16
		mov		ax,cs
		mov		word [esi+2],ax
		mov		byte [esi+4],0
		mov		byte [esi+5],0x8E
		mov		word [esi+6],bx

		; enable the AC bit
		mov		eax,cr0
		or		eax,(1 << 18)		; alignment mask
		mov		cr0,eax
		pushfd
		pop		eax
		or		eax,(1 << 18)
		push		eax
		popfd

		; become ring-3
		mov		[saved_esp],esp
		mov		ax,DPL3_DATA
		mov		ss,ax
		mov		ds,ax
		mov		es,ax
		jmp		DPL3_CODE:ring0_entry
ring0_entry:

		; cause an exception
		mov		ebx,[0x5555]

		; return to ring-0
		call		RETURN_GATE:0
ring0_return:	mov		esp,[saved_esp]

		; disable the AC bit
return_path:	pushfd
		pop		eax
		and		eax,~(1 << 18)
		push		eax
		popfd
		mov		eax,cr0
		and		eax,~(1 << 18)		; alignment mask
		mov		cr0,eax

		; done
		lidt		[idtr]			; restore debugger's IDTR
		ret

saved_esp:	dd		0
gdtr:		dw		0,0,0,0
idtr:		dw		0,0,0,0
idtr_new:	dw		0x7FF
		dd		idt_table

; ring-0 exception handler, reload registers and come back
ac_exception:	mov		ax,DATA_SELECTOR
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
		mov		dword [has],0x12345678
		jmp		return_path

		align		16
idt_table:

