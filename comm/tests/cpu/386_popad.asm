
; 386 POPAD bug test
;
; 32-bit protected mode.
; must be loaded at fixed address 0x40000.
; this creates and loads it's own because the debugger might
; be the RS-232 version which doesn't load IDTR. Or, it might
; be the ethernet version which does.
		use32
		org		0x40000

; TODO: Find an ancient 386 that exhibits this bug

has:		dd		0

; code begins here at offset 0x0004
entry:		cli
		mov		dword [has],0
		mov		byte [ival],32
		mov		eax,0x12345678
.loop:		mov		ebx,eax
		xor		edi,edi
		mov		edx,edi
		pushad
		popad
		mov		ecx,[eax+(ecx*4)]	; uses EAX as base along with another index register immediately after POPAD
		cmp		eax,ebx
		jnz		bug_oh_noes
		rol		eax,1
		sub		byte [ival],1
		jnz		.loop
	
; exit
		ret

bug_oh_noes:	mov		dword [has],1
		ret

ival:		db		0

