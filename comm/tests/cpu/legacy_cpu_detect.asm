
; standard 8086/286/386/486 EFLAGS test

; real mode
		use16
		org		0

cpu_rev:	db		0xFF
cpu_id:		db		0xFF
		db		0

; code begins here at offset 0x0004
entry:		cli				; make sure interrupts are off
		mov		ax,sp
		mov		[cs:stk_save],ax
		and		sp,0xFFFC	; stack align

		; assume 8086
		mov		byte [cs:cpu_id],0
		mov		byte [cs:cpu_rev],0
		; is it more than one?
		; bits 12-15 in EFLAGS are always set, and cannot be reset
		pushf
		pop		ax
		mov		cx,ax
		and		ax,0xFFF	; clear EFLAGS[12:15]
		push		ax
		popf
		pushf
		pop		ax
		and		ax,0xF000	; is EFLAGS[12:15] = 1111b?
		cmp		ax,0xF000
		jz		done		; if so, it's an 8086

		; is it more than a 286?
		mov		byte [cs:cpu_rev],2
		; bits 12-15 will always be clear, cannot be set
		or		cx,0xF000
		push		cx
		popf
		pushf
		pop		ax
		and		ax,0xF000	; is EFLAGS[12:15] = 0000b?
		jz		done		; if so, it's a 286

		; is it more than a 386?
		mov		byte [cs:cpu_rev],3
		; the 386 does not have the AC bit, and it cannot be changed
		pushfd
		pop		eax
		mov		ecx,eax
		xor		eax,0x40000
		push		eax
		popfd
		pushfd
		pop		eax
		xor		eax,ecx		; did we change it?
		jz		done		; if not, it's a 386
		push		ecx
		popfd				; restore AC bit

		; is it more than a 486?
		mov		byte [cs:cpu_rev],4
		; late 486s and Pentiums have an ID bit that we can twiddle
		mov		eax,ecx
		xor		eax,0x200000
		push		eax
		popfd
		pushfd
		pop		eax		; did we change the ID bit?
		xor		eax,ecx		; if not, it's a 486
		jz		done
		mov		byte [cs:cpu_id],1 ; else, it's a 486 or higher with CPUID

; clean up and return
done:		mov		ax,[cs:stk_save]
		mov		sp,ax
		retf

stk_save:	dw		0

