; ----------------32-bit code here-------------------
use32

IN_BUF_SIZE	equ			256
GDT_ENTRIES	equ			32

DATA_SELECTOR		equ		0x10
CODE_SELECTOR		equ		0x08

; this code is jumped to from the 8086 portion.
; you'd better be running this on a 386 or higher because this makes no
; attempt to detect whether the CPU actually supports the mode.
;
; the 8086 portion took care of setting up the UART, we just continue to use it
extern _jmp_8086

; known issues:
;
;    Bochs 2.4.x's UART emulation is terrible. It seems to get stuck,
;        causing this code to loop infinitely waiting for the comport
;        to say it's safe to send another byte. Use VirtualBox instead.

SECTION		.text

; main loop
_main_loop:	cli					; keep interrupts disabled

		mov		ax,DATA_SELECTOR
		mov		es,ax

		mov		ebx,in_buf
		xor		esi,esi			; BX+SI is used to index into in_buf
		
_main_loop_read:call		com_char_in

		cmp		al,10			; the message ends at the newline '\n'
		jz		short _main_loop_process
		cmp		al,13			; the message ends at the newline '\n' (damn you PuTTY)
		jz		short _main_loop_process
		cmp		al,27			; ESC + binary value desired?
		jnz		short _main_loop_read_al
		call		com_char_in		; read in the binary, it won't be taken as newline

_main_loop_read_al:
		cmp		esi,IN_BUF_SIZE-1	; don't overrun our buffer
		jae		short _main_loop_read

		mov		[ebx+esi],al		; store the char
		inc		esi
		jmp		_main_loop_read

_main_loop_process:
		lea		edi,[ebx+esi]		; store the end of the string
		mov		esi,ebx			; SI = in_buf

; clear the rest of the buffer
		push		edi
		cld
		xor		al,al
_main_loop_process_clr:
		stosb
		cmp		edi,IN_BUF_SIZE + in_buf
		jb		_main_loop_process_clr
		pop		edi

		cmp		esi,edi			; empty strings are ignored
		jz		short _main_loop

		call		_main_loop_command
		jnc		short _main_loop	; command function will return with CF=0 if it understood it

		mov		esi,err_head
		call		com_str_out
		mov		esi,unknown_command_msg
		call		com_str_out
		jmp		short _main_loop

; command processing.
; we are expected to set CF=1 if we didn't understand the message
_main_loop_command:
		call		_main_loop_command_test		; function won't return if it is indeed "TEST"
		call		_main_loop_command_read		; ..."READ"
		call		_main_loop_command_readb	; ..."READB"
		call		_main_loop_command_write	; ..."WRITE"
		call		_main_loop_command_writeb	; ..."WRITEB"
		call		_main_loop_command_exec		; ..."EXEC"
		call		_main_loop_command_8086		; ..."8086"

; we didn't understand the message
		stc
		ret

; 8086
_main_loop_command_8086:
		cmp		dword [esi],'8086'
		jnz		.fail
		cmp		byte [esi+4],0
		jnz		.fail
		; UGH it seems we first have to thunk down to 16-bit protected mode, or else Bochs will continue
		; executing 32-bit real mode code heee
		mov		byte [_gdt_table + CODE_SELECTOR + 6],0x8F		; poof, we're 16-bit
		mov		byte [_gdt_table + DATA_SELECTOR + 6],0x8F		; poof, our data segment is 16-bit
		mov		ax,DATA_SELECTOR		; now force the CPU to update by that
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
		jmp		CODE_SELECTOR:word .thunk16
.thunk16:
use16		; switch the CPU back to real mode
		lgdt		[cs:_gdtr_old]
		lidt		[cs:_idtr_realmode]		; on a 386, we then load a sane IDT for real mode
		mov		eax,cr0				; <- this will crash and fault a 386, which is intended
		and		al,0xFE				; turn off protected mode
		mov		cr0,eax
		jmp		0x0000:word .realmode
		; real mode, reload sectors
.realmode:	xor		ax,ax
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
		jmp		0x0000:_jmp_8086
use32
.fail:		ret

; EXEC command
; EXEC <off>
;  in this case, the "segment" value is a byte offset where the code segment starts
_main_loop_command_exec:
		cmp		dword [esi],'EXEC'
		jnz		.fail
		cmp		byte [esi+4],' '
		jnz		.fail
		add		esi,5
		pop		eax
		call		strtohex
		mov		ebx,eax				; ES:BX = pointer to function

; run the code (near call)
		call		ebx

; done
		mov		esi,exec_complete_msg
		jmp		_main_loop_command_response_output

.fail:		ret

; WRITE command
_main_loop_command_write:
		cmp		dword [esi],'WRIT'
		jnz		.fail
		cmp		word [esi+4],'E '
		jnz		.fail
		add		esi,6				; accpeded, skip to addr
		pop		eax				; rip away return
		call		strtohex			; addr -> DX:AX
		call		str_skip_whitespace
		mov		ebx,eax

; bytes to write are read from the command
.writeloop:	call		str_skip_whitespace
		cmp		esi,edi
		jae		.writeloopend
		
		call		strtohex
		call		str_skip_whitespace
		mov		[ebx],al

		inc		ebx
		jmp		.writeloop
.writeloopend:
		mov		esi,write_complete_msg
		jmp		_main_loop_command_response_output

.fail:		ret		

; WRITEB command
_main_loop_command_writeb:
		cmp		dword [esi],'WRIT'
		jnz		.fail
		cmp		word [esi+4],'EB'
		jnz		.fail
		cmp		byte [esi+6],' '
		jnz		.fail
		add		esi,7				; accpeded, skip to addr
		pop		eax				; rip away return
		call		strtohex			; addr -> DX:AX
		call		str_skip_whitespace
		mov		ebx,eax

		; how many bytes?
		call		strtohex
		mov		ecx,eax

		; binary data following it determines what we write.
		; the host is expected to write EXACTLY the byte count it said
.writeloop:	or		ecx,ecx
		jz		.writeloopend
		dec		ecx

		call		com_char_in
		mov		[ebx],al

		inc		ebx
		jnc		.writeloop
.writeloopend:
		mov		esi,write_complete_msg
		jmp		_main_loop_command_response_output

.fail:		ret		

; READ command
; READ <phys memaddress> <number of bytes>
_main_loop_command_read:
		cmp		dword [esi],'READ'
		jnz		.fail
		cmp		byte [esi+4],' '
		jnz		.fail
		add		esi,5				; accepted, now parse mem address
		pop		eax				; rip away return to _main_loop_command so we fall back to main loop
		call		strtohex			; parse string into binary value (string hex digits to binary) -> DX:AX
		call		str_skip_whitespace
		mov		ebx,eax				; we'll use ES:BX to read

		; how many bytes?
		call		strtohex
		mov		ecx,eax

		; start of message
		mov		esi,ok_head
		call		com_str_out

.readloop:	or		ecx,ecx
		jz		.readloopend
		dec		ecx

		mov		al,[ebx]
		call		com_hex8_out
		mov		al,' '
		call		com_char_out
		inc		ebx
		jmp		.readloop
.readloopend:

		; end of message
		mov		esi,crlf
		call		com_str_out
		clc
.fail:		ret

; READB command
; READB <phys memaddress> <number of bytes>
_main_loop_command_readb:
		cmp		dword [esi],'READ'
		jnz		.fail
		cmp		word [esi+4],'B '
		jnz		.fail
		add		esi,6				; accepted, now parse mem address
		pop		eax				; rip away return to _main_loop_command so we fall back to main loop
		call		strtohex			; parse string into binary value (string hex digits to binary) -> DX:AX
		call		str_skip_whitespace
		mov		ebx,eax				; we'll use ES:BX to read

		; how many bytes?
		call		strtohex
		mov		ecx,eax

		; start of message
		mov		esi,ok_head
		call		com_str_out

.readloop:	or		ecx,ecx
		jz		.readloopend
		dec		ecx

		mov		al,[ebx]
		call		com_char_out
		inc		ebx
		jmp		.readloop
.readloopend:

		; end of message
		mov		esi,crlf
		call		com_str_out
		clc
.fail:		ret

; TEST command
_main_loop_command_test:
		cmp		dword [esi],'TEST'
		jnz		.fail
		cmp		byte [esi+4],0
		jnz		.fail
; OK-----it is TEST
		pop		eax				; remove stack frame back to _main_loop_command so we head straight back to the caller of that function
		mov		esi,test_response
		jmp		_main_loop_command_response_output
		call		com_str_out
.fail:		ret						; on failure, we go back to _main_loop_command. on success, we go straight back to _main_loop_process_clr

; SI = message to return with 'OK' header. caller should JUMP to us not CALL us. well actually it doesn't matter, but that's the intent.
_main_loop_command_response_output:
		push		esi
		mov		esi,ok_head
		call		com_str_out
		pop		esi
		call		com_str_out
		mov		esi,crlf
		call		com_str_out
		clc
		ret

; SI = message to return with 'ERR' header. caller should JUMP to us not CALL us. well actually it doesn't matter, but that's the intent.
_main_loop_command_response_output_err:
		push		esi
		mov		esi,err_head
		call		com_str_out
		pop		esi
		call		com_str_out
		mov		esi,crlf
		call		com_str_out
		clc
		ret

_main_loop_command_understood_generic:
		mov		esi,ok_head
		call		com_str_out
		mov		esi,crlf
		call		com_str_out
		clc
		ret

; strtohex_getdigit
; from DS:SI read one char and convert to hex. if not hex digit, then CF=1
strtohex_getdigit:
		push		eax
		lodsb
		cmp		al,'0'
		jb		.err			; '0' to '9' are 0x30-0x39 so anything below that is invalid
		cmp		al,'9'
		jbe		.numeral		; <= '9' then is definitely a number
		or		al,0x20			; quick conversion to lowercase
		cmp		al,'a'
		jb		.err			; 'A' binary code is less than 'a'
		cmp		al,'f'
		ja		.err			; 'f' is above 'F'
		sub		al,'a' - 10		; equiv: al -= 'a', al += 10
		jmp		.ok

.numeral:	sub		al,'0'
		; fall to .ok below'

.ok:		clc
		mov		bl,al
		pop		eax
		ret

.err:		stc
		pop		eax
		ret

; scan string from DS:SI returning in AX.
; SI is returned adjusted forward to the first non-digit char
strtohex:	push		ebx
		xor		eax,eax
.loop:		call		strtohex_getdigit
		jc		.end			; function will set CF=1 if it isn't a hex digit. else the converted digit will be in BL

		shl		eax,4
		or		al,bl			; AX = (AX << 4) | BL
		jmp		.loop

.end:		pop		ebx
		ret

; hello
hello_msg:	db		'Stage 2 active.',13,10,0
ask_comport:	db		'Choose COM port: [1] 3F8   [2] 2F8   [3] 3E8   [4] 2E8',13,10,0
address_invalid_msg: db		'Invalid address',0
unknown_command_msg: db		'Unknown command',13,10,0
announcing_comport: db		'Using com port at ',0
write_complete_msg: db		'Accepted',0
exec_complete_msg: db		'Function complete',0
test_response:	db		'Test successful',0
err_head:	db		'ERR ',0
ok_head:	db		'OK ',0
crlf:		db		13,10,0

; scan DS:SI forward to skip whitespace
str_skip_whitespace:
		cmp		byte [si],' '
		jz		.skip
.end		ret
.skip:		inc		si
		jmp		str_skip_whitespace

; print a hexadecimal digit in AL
com_puthex_digit:
		push		ax
		push		bx
		and		ax,0Fh
		mov		bx,ax
		mov		al,[hexdigits + bx]
		call		com_char_out
		pop		bx
		pop		ax
		ret

; print a hex number in AX
com_hex8_out:	push		ax				; AX = 1234
		rol		al,4				; AX = 2341
		call		com_puthex_digit
		rol		al,4				; AX = 3412
		call		com_puthex_digit
		pop		ax
		ret

; read one byte from the comport
com_char_in:	push		dx
		mov		dx,[comport]
		add		dx,5				; DX = Line status register
com_char_in_wait:
		in		al,dx
		test		al,1
		jz		com_char_in_wait
		sub		dx,5				; DX = recieve buffer
		in		al,dx
		pop		dx
		ret

; write one byte to the comport from AL
com_char_out:	push		edx
		push		eax
		push		ecx
		mov		dx,[comport]
		add		dx,5
		xor		ecx,ecx
com_char_out_wait:
		in		al,dx
		test		al,0x60				; is transmit buffer empty?
		jz		com_char_out_wait		; if not, loop again
		mov		dx,[comport]
		pop		ecx
		pop		eax
		out		dx,al
		pop		edx
		ret

com_str_out:	push		esi
		push		eax
com_str_outl:	lodsb
		or		al,al
		jz		com_str_oute
		call		com_char_out
		jmp		com_str_outl
com_str_oute:	pop		eax
		pop		esi
		ret

; strings
extern hexdigits
note_386_32:	db		'386-32',0

; variables
inited		db		0
extern comport
; comport	dw

; input buffer
in_buf		times IN_BUF_SIZE db 0

; ----------------16-bit code here-------------------
use16

; generate 386 GDT
gen_gdt_386:	mov		di,_gdt_table
		cld
		xor		ax,ax
; #0 NULL entry
		stosw
		stosw
		stosw
		stosw
; #1 code
	; 0:15
		mov		cl,4
		mov		ax,0xFFFF		; limit
		stosw
	; 16:31
		xor		ax,ax			; base
		stosw
	; 32:47
		xor		al,al
		mov		ah,0x9A			; access = present, ring 0, executable, readable
		stosw
	; 48:63
		mov		ax,0x00CF		; base 24:31 granular 4KB pages, 32-bit selector
		stosw
; #2 data
	; 0:15
		mov		cl,4
		mov		ax,0xFFFF		; limit
		stosw
	; 16:31
		xor		ax,ax			; base
		stosw
	; 32:47
		xor		al,al
		mov		ah,0x92			; access = present, ring 0, readable/writeable, data
		stosw
	; 48:63
		mov		ax,0x00CF		; base 24:31 granular 4KB pages, 32-bit selector
		stosw
; #3 ref data
	; 0:15
		mov		cl,4
		mov		ax,0xFFFF		; limit
		stosw
	; 16:31
		xor		ax,ax			; base
		stosw
	; 32:47
		xor		al,al
		mov		ah,0x92			; access = present, ring 0, readable/writeable, data
		stosw
	; 48:63
		mov		ax,0x00CF		; base 24:31 granular 4KB pages, 32-bit selector
		stosw
; #4 ref code
	; 0:15
		mov		cl,4
		mov		ax,0xFFFF		; limit
		stosw
	; 16:31
		xor		ax,ax			; base
		stosw
	; 32:47
		xor		al,al
		mov		ah,0x9A			; access = present, ring 0, executable, readable
		stosw
	; 48:63
		mov		ax,0x00CF		; base 24:31 granular 4KB pages, 32-bit selector
		stosw

; fill in the GDTR too
		mov		di,_gdtr
		mov		ax,(GDT_ENTRIES * 8) - 1
		stosw					; limit
		mov		ax,_gdt_table
		stosw					; base 0:15
		xor		ax,ax
		stosw					; base 16:31
		xor		ax,ax
		stosw

; done
		ret

; jump here
global _jmp_386_32
align		16

_jmp_386_32:	cli					; NO INTERRUPTS! We're not prepared to handle them
		mov		ax,cs
		mov		ds,ax
		mov		ss,ax
		mov		[_orig_cs],ax
		mov		sp,7BFCh
		call		gen_gdt_386
		sgdt		[cs:_gdtr_old]
		lgdt		[cs:_gdtr]
		mov		eax,cr0
		or		al,1			; switch on protected mode, 386 style
		mov		cr0,eax
		jmp		dword CODE_SELECTOR:.cacheflush
use32
.cacheflush:	mov		ax,DATA_SELECTOR
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
; out the message
		mov		esi,ok_head
		call		com_str_out
		mov		esi,note_386_32
		call		com_str_out
		mov		esi,crlf
		call		com_str_out
		jmp		_main_loop

; GDT table needed for protected mode
align		16
_orig_cs	dw		0
_gdt_table:	times (8 * GDT_ENTRIES) db 0
_gdtr_old	dd		0,0
_gdtr		dd		0,0
_idtr_brainfuck:dd		0,0
_idtr_realmode:	dd		0x400-1,0

; this must finish on a paragraph.
; when this and other images are catencated together this ensures each one can safely
; run from it's own 16-bit segment.
align		16

