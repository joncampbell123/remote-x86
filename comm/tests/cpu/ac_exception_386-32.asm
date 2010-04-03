
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
TSS_SEL		EQU		(9*8)
TSS2_SEL	EQU		(10*8)

TSS_OFFSET	EQU		0x3C000
TSS2_OFFSET	EQU		0x3E000

DATA_SELECTOR	EQU		0x10
CODE_SELECTOR	EQU		0x08

has:		dd		0

; code begins here at offset 0x0004
entry:		cli
		sidt		[idtr]			; save debugger's
		lidt		[idtr_new]		; load our own
		sgdt		[gdtr]			; we need the GDT
		mov		[saved_esp],esp
		and		esp,~3			; align stack

		; shut off AC
		pushfd
		pop		eax
		and		eax,~(1 << 18)
		push		eax
		popfd

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

		mov		esi,[gdtr+2]
		add		esi,TSS_SEL		;
		mov		word [esi+0],104	; limit 0...15
		mov		word [esi+2],TSS_OFFSET	; base 0...15
		mov		byte [esi+4],(TSS_OFFSET >> 16) ; base 16...23
		mov		byte [esi+5],0x89|(3<<5) ; present, DPL=3, TSS, B=0
		mov		byte [esi+6],0x80	; G=1, limit=0
		mov		byte [esi+7],(TSS_OFFSET >> 24)	; base 24...31

		mov		esi,[gdtr+2]
		add		esi,TSS2_SEL		;
		mov		word [esi+0],104	; limit 0...15
		mov		word [esi+2],TSS2_OFFSET ; base 0...15
		mov		byte [esi+4],(TSS2_OFFSET >> 16) ; base 16...23
		mov		byte [esi+5],0x89|(3<<5) ; present, DPL=3, TSS, B=0
		mov		byte [esi+6],0x80	; G=1, limit=0
		mov		byte [esi+7],(TSS2_OFFSET >> 24) ; base 24...31

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
		; do NOT set AC in the EFLAGS register yet, there is one
		; instruction we run that does a non-aligned read from the GDTR

		; make up some bullshit TSS to satisfy the
		; CPU in our attempt to jump to Ring 3
		mov		esi,TSS_OFFSET
		mov		ax,ss
		movzx		eax,ax
		mov		dword [esi+0x00],0	; back link
		mov		dword [esi+0x04],esp	; PL0 ESP
		mov		dword [esi+0x08],eax	; PL0 SS
		mov		dword [esi+0x0C],esp	; PL1 ESP
		mov		dword [esi+0x10],eax	; PL1 SS
		mov		dword [esi+0x14],esp	; PL2 ESP
		mov		dword [esi+0x18],eax	; PL2 SS
		mov		eax,cr3
		mov		dword [esi+0x1C],eax	; CR3
		mov		dword [esi+0x20],ring0_entry	; EIP
		pushfd
		pop		eax
		mov		dword [esi+0x24],eax	; EFLAGS
		mov		eax,0x31313131
		mov		dword [esi+0x28],eax	; EAX
		mov		dword [esi+0x2C],eax	; ECX
		mov		dword [esi+0x30],eax	; EDX
		mov		dword [esi+0x34],eax	; EBX
		mov		dword [esi+0x38],esp	; ESP
		mov		dword [esi+0x3C],ebp	; EBP
		mov		dword [esi+0x40],eax	; ESI
		mov		dword [esi+0x44],eax	; EDI
		mov		eax,DPL3_DATA|3
		mov		dword [esi+0x48],eax	; ES
		mov		ebx,DPL3_CODE|3
		mov		dword [esi+0x4C],ebx	; CS
		mov		dword [esi+0x50],eax	; SS
		mov		dword [esi+0x54],eax	; DS
		mov		dword [esi+0x58],eax	; FS
		mov		dword [esi+0x5C],eax	; GS
		mov		dword [esi+0x60],0	; LDT
		mov		dword [esi+0x64],0	; I/O bitmap T=0

		; and for coming back...
		mov		esi,TSS2_OFFSET
		mov		eax,DATA_SELECTOR
		mov		dword [esi+0x00],0	; back link
		mov		dword [esi+0x04],esp	; PL0 ESP
		mov		dword [esi+0x08],eax	; PL0 SS
		mov		dword [esi+0x0C],esp	; PL1 ESP
		mov		dword [esi+0x10],eax	; PL1 SS
		mov		dword [esi+0x14],esp	; PL2 ESP
		mov		dword [esi+0x18],eax	; PL2 SS
		mov		eax,cr3
		mov		dword [esi+0x1C],eax	; CR3
		mov		dword [esi+0x20],ring0_return	; EIP
		pushfd
		pop		eax
		mov		dword [esi+0x24],eax	; EFLAGS
		mov		eax,0x31313131
		mov		dword [esi+0x28],eax	; EAX
		mov		dword [esi+0x2C],eax	; ECX
		mov		dword [esi+0x30],eax	; EDX
		mov		dword [esi+0x34],eax	; EBX
		mov		dword [esi+0x38],esp	; ESP
		mov		dword [esi+0x3C],ebp	; EBP
		mov		dword [esi+0x40],eax	; ESI
		mov		dword [esi+0x44],eax	; EDI
		mov		eax,DATA_SELECTOR
		mov		dword [esi+0x48],eax	; ES
		mov		ebx,CODE_SELECTOR
		mov		dword [esi+0x4C],ebx	; CS
		mov		dword [esi+0x50],eax	; SS
		mov		dword [esi+0x54],eax	; DS
		mov		dword [esi+0x58],eax	; FS
		mov		dword [esi+0x5C],eax	; GS
		mov		dword [esi+0x60],0	; LDT
		mov		dword [esi+0x64],0	; I/O bitmap T=0

		; initialize the task register to the 2nd TSS
		; so that when we jump to the first, the CPU
		; will place it's state in a known location.
		; if we don't... my testing reveals that the
		; CPU doesn't complain, it just pukes the state
		; all over the real-mode interrupt vector table
		; at 0x00000000 which then eventually causes a
		; crash when we return to 8086 real mode.
		; Eugh, even Bochs emulates that bug perfectly ;p
		mov		ax,TSS2_SEL
		ltr		ax

		; become ring-3
		jmp		TSS_SEL:0		; through the task gate
ring0_entry:

		; clear the Busy bit so we can switch again
		mov		esi,[gdtr+2]
		add		esi,TSS_SEL		;
		mov		byte [esi+5],0x89|(3<<5) ; present, DPL=3, TSS, B=0

		; NOW it is safe to enable AC
		pushfd
		pop		eax
		or		eax,(1 << 18)
		push		eax
		popfd

		; cause an exception
		mov		ebx,[0x5555]

		; return to ring-0
		jmp		TSS2_SEL:0		; through the task gate
ring0_return:

		; remove AC bit from EFLAGS and AM mask from CR0
		pushfd
		pop		eax
		and		eax,~(1 << 18)
		push		eax
		popfd
		mov		eax,cr0
		and		eax,~(1 << 18)		; alignment mask
		mov		cr0,eax

		; done
		lidt		[idtr]			; restore debugger's IDTR
		mov		esp,[saved_esp]		; debugger's stack
		ret

		align		4
saved_esp:	dd		0
gdtr:		dw		0,0,0,0
idtr:		dw		0,0,0,0
idtr_new:	dw		0x7FF
		dd		idt_table

; ring-0 exception handler, reload registers and come back
ac_exception:	mov		ax,DATA_SELECTOR
		mov		ds,ax
		mov		es,ax
		mov		dword [has],0x12345678
		jmp		ring0_return

		align		16
idt_table:

