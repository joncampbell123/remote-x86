; FIXME: Update this code to use 386 instructions.
;        So far this is an updated version of your 286 code. You can use 32-bit instructions in 16-bit segments!

%define MAIN_16_BIT
%define MAIN_386
%include "global.inc"

; this code is jumped to from the 8086 portion.
; you'd better be running this on a 386 or higher because this makes no
; attempt to detect whether the CPU actually supports the mode.
;
; the 8086 portion took care of setting up the UART, we just continue to use it

; known issues:
;
;    Bochs 2.4.x's UART emulation is terrible. It seems to get stuck,
;        causing this code to loop infinitely waiting for the comport
;        to say it's safe to send another byte. Use VirtualBox instead.
bits 16
use16

SECTION		.text

; main loop
_main_loop:	cli					; keep interrupts disabled

		mov		ax,DATA_SELECTOR
		mov		es,ax

		mov		bx,in_buf
		xor		si,si			; BX+SI is used to index into in_buf
		
_main_loop_read:call		com_char_in

		cmp		al,10			; the message ends at the newline '\n'
		jz		short _main_loop_process
		cmp		al,13			; the message ends at the newline '\n' (damn you PuTTY)
		jz		short _main_loop_process
		cmp		al,27			; ESC + binary value desired?
		jnz		short _main_loop_read_al
		call		com_char_in		; read in the binary, it won't be taken as newline

_main_loop_read_al:
		cmp		si,IN_BUF_SIZE-1	; don't overrun our buffer
		jae		short _main_loop_read

		mov		[bx+si],al		; store the char
		inc		si
		jmp		_main_loop_read

_main_loop_process:
		lea		di,[bx+si]		; store the end of the string
		mov		si,bx			; SI = in_buf

; clear the rest of the buffer
		push		di
		cld
		xor		al,al
_main_loop_process_clr:
		stosb
		cmp		di,IN_BUF_SIZE + in_buf
		jb		_main_loop_process_clr
		pop		di

		cmp		si,di			; empty strings are ignored
		jz		short _main_loop

		call		_main_loop_command
		jnc		short _main_loop	; command function will return with CF=0 if it understood it

		mov		si,err_head
		call		com_str_out
		mov		si,unknown_command_msg
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
		call		_main_loop_command_low		; ..."LOW"

; we didn't understand the message
		stc
		ret

; low
_main_loop_command_low:
		cmp		word [si],'LO'
		jnz		.fail
		cmp		byte [si+2],'W'
		jnz		.fail
		cmp		byte [si+3],0
		jnz		.fail
		pop		ax
		mov		si,ok_head
		call		com_str_out
		mov		ax,last_byte
		call		com_hex_out
		mov		si,crlf
		call		com_str_out
.fail:		ret

; 8086
_main_loop_command_8086:
		cmp		word [si],'80'
		jnz		.fail
		cmp		word [si+2],'86'
		jnz		.fail
		cmp		byte [si+4],0
		jnz		.fail
		; switch the CPU back to real mode
		lgdt		[cs:_gdtr_old]
		lidt		[cs:_idtr_realmode]		; on a 386, we then load a sane IDT for real mode
		mov		eax,cr0				; <- this will crash and fault a 386, which is intended
		and		al,0xFE				; turn off protected mode
		mov		cr0,eax
		jmp		0x0000:.realmode
		; real mode, reload sectors
.realmode:	xor		ax,ax
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
		jmp		0x0000:_jmp_8086
.fail:		ret

; EXEC command
; EXEC <seg>:<off>
;  in this case, the "segment" value is a byte offset where the code segment starts
_main_loop_command_exec:
		cmp		word [si],'EX'
		jnz		.fail
		cmp		word [si+2],'EC'
		jnz		.fail
		cmp		byte [si+4],' '
		jnz		.fail
		add		si,5
		pop		ax
		call		strtohex_32			; DX:AX = seg
		call		dx_ax_phys_to_386seg_code
		mov		es,dx
		call		str_skip_whitespace
		call		strtohex
		mov		bx,ax				; ES:BX = pointer to function

; run the code (far call)
		mov		word [_command_exec_far_ptr+0],bx
		mov		word [_command_exec_far_ptr+2],es
		call far	word [_command_exec_far_ptr]

; done
		mov		si,exec_complete_msg
		jmp		_main_loop_command_response_output

.fail:		ret

; WRITE command
_main_loop_command_write:
		cmp		word [si],'WR'
		jnz		.fail
		cmp		word [si+2],'IT'
		jnz		.fail
		cmp		word [si+4],'E '
		jnz		.fail
		add		si,6				; accpeded, skip to addr
		pop		ax				; rip away return
		call		strtohex_32			; addr -> DX:AX
		call		str_skip_whitespace
		call		dx_ax_phys_to_386seg
		mov		es,dx
		mov		bx,ax

; bytes to write are read from the command
.writeloop:	call		str_skip_whitespace
		cmp		si,di
		jae		.writeloopend
		
		call		strtohex
		call		str_skip_whitespace
		mov		[es:bx],al

		inc		bx
		jnc		.writeloop
		call		acc386_64k_bump
		jmp		.writeloop
.writeloopend:
		mov		si,write_complete_msg
		jmp		_main_loop_command_response_output

.fail:		ret		

; WRITEB command
_main_loop_command_writeb:
		cmp		word [si],'WR'
		jnz		.fail
		cmp		word [si+2],'IT'
		jnz		.fail
		cmp		word [si+4],'EB'
		jnz		.fail
		cmp		byte [si+6],' '
		jnz		.fail
		add		si,7				; accpeded, skip to addr
		pop		ax				; rip away return
		call		strtohex_32			; addr -> DX:AX
		call		str_skip_whitespace
		call		dx_ax_phys_to_386seg
		mov		es,dx
		mov		bx,ax

		; how many bytes?
		call		strtohex
		mov		cx,ax

		; binary data following it determines what we write.
		; the host is expected to write EXACTLY the byte count it said
.writeloop:	or		cx,cx
		jz		.writeloopend
		dec		cx

		call		com_char_in
		mov		[es:bx],al

		inc		bx
		jnc		.writeloop
		call		acc386_64k_bump
		jmp		.writeloop
.writeloopend:
		mov		si,write_complete_msg
		jmp		_main_loop_command_response_output

.fail:		ret		

; READ command
; READ <phys memaddress> <number of bytes>
_main_loop_command_read:
		cmp		word [si],'RE'
		jnz		.fail
		cmp		word [si+2],'AD'
		jnz		.fail
		cmp		byte [si+4],' '
		jnz		.fail
		add		si,5				; accepted, now parse mem address
		pop		ax				; rip away return to _main_loop_command so we fall back to main loop
		call		strtohex_32			; parse string into binary value (string hex digits to binary) -> DX:AX
		call		str_skip_whitespace
		call		dx_ax_phys_to_386seg		; convert DX:AX into real-mode pointer
		mov		es,dx
		mov		bx,ax				; we'll use ES:BX to read

		; how many bytes?
		call		strtohex
		mov		cx,ax

		; start of message
		mov		si,ok_head
		call		com_str_out

.readloop:	or		cx,cx
		jz		.readloopend
		dec		cx

		mov		al,[es:bx]
		call		com_hex8_out
		mov		al,' '
		call		com_char_out
		inc		bx
		jnc		.readloop
		call		acc386_64k_bump
		jmp		.readloop
.readloopend:

		; end of message
		mov		si,crlf
		call		com_str_out
		clc
.fail:		ret

; READB command
; READB <phys memaddress> <number of bytes>
_main_loop_command_readb:
		cmp		word [si],'RE'
		jnz		.fail
		cmp		word [si+2],'AD'
		jnz		.fail
		cmp		word [si+4],'B '
		jnz		.fail
		add		si,6				; accepted, now parse mem address
		pop		ax				; rip away return to _main_loop_command so we fall back to main loop
		call		strtohex_32			; parse string into binary value (string hex digits to binary) -> DX:AX
		call		str_skip_whitespace
		call		dx_ax_phys_to_386seg		; convert DX:AX into real-mode pointer
		mov		es,dx
		mov		bx,ax				; we'll use ES:BX to read

		; how many bytes?
		call		strtohex
		mov		cx,ax

		; start of message
		mov		si,ok_head
		call		com_str_out

.readloop:	or		cx,cx
		jz		.readloopend
		dec		cx

		mov		al,[es:bx]
		call		com_char_out
		inc		bx
		jnc		.readloop
		call		acc386_64k_bump
		jmp		.readloop
.readloopend:

		; end of message
		mov		si,crlf
		call		com_str_out
		clc
.fail:		ret

; TEST command
_main_loop_command_test:
		cmp		word [si],'TE'
		jnz		.fail
		cmp		word [si+2],'ST'
		jnz		.fail
		cmp		byte [si+4],0
		jnz		.fail
; OK-----it is TEST
		pop		ax				; remove stack frame back to _main_loop_command so we head straight back to the caller of that function
		mov		si,test_response
		jmp		_main_loop_command_response_output
		call		com_str_out
.fail:		ret						; on failure, we go back to _main_loop_command. on success, we go straight back to _main_loop_process_clr

; SI = message to return with 'OK' header. caller should JUMP to us not CALL us. well actually it doesn't matter, but that's the intent.
_main_loop_command_response_output:
		push		si
		mov		si,ok_head
		call		com_str_out
		pop		si
		call		com_str_out
		mov		si,crlf
		call		com_str_out
		clc
		ret

; SI = message to return with 'ERR' header. caller should JUMP to us not CALL us. well actually it doesn't matter, but that's the intent.
_main_loop_command_response_output_err:
		push		si
		mov		si,err_head
		call		com_str_out
		pop		si
		call		com_str_out
		mov		si,crlf
		call		com_str_out
		clc
		ret

_main_loop_command_understood_generic:
		mov		si,ok_head
		call		com_str_out
		mov		si,crlf
		call		com_str_out
		clc
		ret

; strtohex_getdigit
; from DS:SI read one char and convert to hex. if not hex digit, then CF=1
strtohex_getdigit:
		push		ax
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
		pop		ax
		ret

.err:		stc
		pop		ax
		ret

; scan string from DS:SI returning in AX.
; SI is returned adjusted forward to the first non-digit char
strtohex:	push		bx
		push		cx
		xor		ax,ax
.loop:		call		strtohex_getdigit
		jc		.end			; function will set CF=1 if it isn't a hex digit. else the converted digit will be in BL

		mov		cl,4
		shl		ax,cl
		or		al,bl			; AX = (AX << 4) | BL
		jmp		.loop

.end:		pop		cx
		pop		bx
		ret

; scan string from DS:SI returning in DX:AX.
; SI is returned adjusted forward to the first non-digit char
strtohex_32:	push		bx
		push		cx
		xor		ax,ax
		mov		dx,ax
.loop:		call		strtohex_getdigit
		jc		.end			; function will set CF=1 if it isn't a hex digit. else the converted digit will be in BL

		push		bx
		mov		cl,12
		mov		bx,ax			; BX = AX >> 12
		shr		bx,cl
		mov		cl,4
		shl		dx,cl
		or		dl,bl			; DX = (DX << 4) | [(AX >> 12)]
		pop		bx
		shl		ax,cl
		or		al,bl			; AX = (AX << 4) | BL
		jmp		.loop

.end:		pop		cx
		pop		bx
		ret

; hello
;hello_msg:	db		'Stage 2 active.',13,10,0
;ask_comport:	db		'Choose COM port: [1] 3F8   [2] 2F8   [3] 3E8   [4] 2E8',13,10,0
;address_invalid_msg: db		'Invalid address',0
;unknown_command_msg: db		'Unknown command',13,10,0
;announcing_comport: db		'Using com port at ',0
;write_complete_msg: db		'Accepted',0
;exec_complete_msg: db		'Function complete',0
;test_response:	db		'Test successful',0
;err_head:	db		'ERR ',0
;ok_head:	db		'OK ',0
;crlf:		db		13,10,0

; pump the access base up 64K
acc386_64k_bump:
		push		di
		push		ax
		mov		di,_gdt_table + ACCESS_SELECTOR
		mov		word [di],0xFFFF
		mov		word [di+2],0
		add		byte [di+4],1
		adc		byte [di+7],1
		mov		ax,ACCESS_SELECTOR
		mov		es,ax
		pop		ax
		pop		di
		ret

; convert physical addr DX:AX to 386-mode address DX:AX
; note we accomplish this not by absolute translation but
; by updating the "reference" selector. so what comes
; back is AX zeroed and DX set to the access selector
;
; A0000 = A000:0000
; 12345 = 1000:2345
dx_ax_phys_to_386seg_code:
		push		di
		mov		di,_gdt_table + USER_CODE_SELECTOR
		mov		word [di],0xFFFF			; limit kept at 0xFFFF
		mov		word [di+2],ax				; base 0:15 = AX
		mov		[di+4],dl				; DL becomes base 16:23
		mov		[di+7],dh				; DH becomes base 24:31
		pop		di
		mov		dx,USER_CODE_SELECTOR
		xor		ax,ax
		ret

; convert physical addr DX:AX to 386-mode address DX:AX
; note we accomplish this not by absolute translation but
; by updating the "reference" selector. so what comes
; back is AX unchanged and DX set to the access selector
;
; A0000 = A000:0000
; 12345 = 1000:2345
;
; NOT!!! Apparently I can't just bump bits 16:23 forward when I need to, it doesn't work!!!!
dx_ax_phys_to_386seg:
		push		di
		mov		di,_gdt_table + ACCESS_SELECTOR
		mov		word [di],0xFFFF			; limit kept at 0xFFFF
		mov		word [di+2],ax				; base 0:15 kept at zero
		mov		[di+4],dl				; DL becomes base 16:23
		mov		[di+7],dh				; DH becomes base 24:31
		pop		di
		mov		dx,ACCESS_SELECTOR
		xor		ax,ax
		ret

; scan DS:SI forward to skip whitespace
str_skip_whitespace:
		cmp		byte [si],' '
		jz		.skip
.end		ret
.skip:		inc		si
		jmp		str_skip_whitespace

%ifdef XXX
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
com_char_out:	push		dx
		push		ax
		push		cx
		mov		dx,[comport]
		add		dx,5
		xor		cx,cx
com_char_out_wait:
		in		al,dx
		test		al,0x60				; is transmit buffer empty?
		jz		com_char_out_wait		; if not, loop again
		mov		dx,[comport]
		pop		cx
		pop		ax
		out		dx,al
		pop		dx
		ret

com_str_out:	push		si
		push		ax
com_str_outl:	lodsb
		or		al,al
		jz		com_str_oute
		call		com_char_out
		jmp		com_str_outl
com_str_oute:	pop		ax
		pop		si
		ret
%endif

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
		mov		ax,0x008F		; base 24:31 granular 4KB pages, 16-bit selector
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
		mov		ax,0x008F		; base 24:31 granular 4KB pages, 16-bit selector
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
		mov		ax,0x008F		; base 24:31 granular 4KB pages, 16-bit selector
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
		mov		ax,0x008F		; base 24:31 granular 4KB pages, 16-bit selector
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
global _jmp_386_16
align		16
_jmp_386_16:	cli					; NO INTERRUPTS! We're not prepared to handle them
		mov		ax,cs
		mov		ds,ax
		mov		ss,ax
		mov		sp,7BFCh
		mov		si,ok_head
		call		com_str_out
		mov		si,note_386_16
		call		com_str_out
; now switch into 386 protected mode
		call		gen_gdt_386
		sgdt		[cs:_gdtr_old]
		lgdt		[cs:_gdtr]
		mov		eax,cr0
		or		al,1			; switch on protected mode, 386 style
		mov		cr0,eax
		jmp		CODE_SELECTOR:.cacheflush
.cacheflush:	mov		ax,DATA_SELECTOR
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
; end the message
		mov		si,crlf
		call		com_str_out
		jmp		_main_loop

; strings
note_386_16:	db		'386-16',0

; this must finish on a paragraph.
; when this and other images are catencated together this ensures each one can safely
; run from it's own 16-bit segment.
align		16

