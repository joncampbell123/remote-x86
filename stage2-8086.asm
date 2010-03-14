
IN_BUF_SIZE	equ		256

; stage1 executes us within our own segment, 16-bit real mode
; when linked together, this must be FIRST!
;
; we're jumped to from stage 1 to 0x0000:0x8000. basing segments
; from zero allows us to reference everything within the first
; 64K absolutely, and also for the other parts like the 32-bit
; protected mode part, to do absolute references to memory.

; known issues:
;
;    Bochs 2.4.x's UART emulation is terrible. It seems to get stuck,
;        causing this code to loop infinitely waiting for the comport
;        to say it's safe to send another byte. Use VirtualBox instead.
bits 16
use16

SECTION		.text
global		_start

BAUD_DIVISOR	equ		(115200/12)	; 9600

; code starts here
_start:		push		cs
		pop		ds
		push		cs
		pop		es

		cmp		byte [inited],0			; other modules may have jumped back to this location. if so, don't re-init everything
		jz		_start_first_time
		jmp		_main_loop

_start_first_time:
		inc		byte [inited]

		mov		si,hello_msg
		call		puts

; ask which comport
		mov		si,ask_comport
		call		puts
ask_loop:
		xor		ax,ax
		int		16h
		cmp		al,'1'
		jl		ask_loop
		cmp		al,'4'
		ja		ask_loop
		sub		al,'1'
		xor		ah,ah
		mov		bx,ax
		shl		bx,1
		mov		ax,[comports + bx]
		mov		word [comport],ax

; announce which comport
		mov		si,announcing_comport
		call		puts
		mov		ax,[comport]
		call		puthex
		mov		si,crlf
		call		puts

; setup the comport
		mov		dx,[comport]
		add		dx,3			; line control register
		mov		al,0x80			; set DLAB
		out		dx,al

		mov		dx,[comport]		; DLAB low
		mov		ax,BAUD_DIVISOR
		out		dx,al
		inc		dx
		mov		cl,8			; remember this might run on an 8086 which doesn't have shr reg,imm
		shr		ax,cl			; >>= 8
		out		dx,al			; DLAB high

		mov		dx,[comport]
		add		dx,3			; line control register
		mov		al,(1 << 3) | 3;	; odd parity 8 bits
		out		dx,al

		mov		dx,[comport]
		inc		dx			; interrupt enable
		xor		al,al			; set all bits to zero
		out		dx,al

		mov		dx,[comport]
		add		dx,2			; FIFO control
		xor		al,al			; set all bits to zero
		out		dx,al

		mov		dx,[comport]
		add		dx,4			; modem control
		xor		al,al			; set all bits to zero
		out		dx,al

		jmp		_main_loop

; main loop
_main_loop:	cli					; keep interrupts disabled

		push		cs
		pop		es

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
		call		_main_loop_command_x64		; ..."x64"
		call		_main_loop_command_386_32	; ..."386-32"
		call		_main_loop_command_386_16	; ..."386-16"
		call		_main_loop_command_286		; ..."286"
		call		_main_loop_command_8086		; ..."8086"

; we didn't understand the message
		stc
		ret

; x64
_main_loop_command_x64:
		cmp		word [si],'x6'
		jnz		.fail
		cmp		byte [si+2],'4'
		jnz		.fail
		cmp		byte [si+3],0
		jnz		.fail
		; TODO
.fail:		ret

; 386-32
_main_loop_command_386_32:
		cmp		word [si],'38'
		jnz		.fail
		cmp		word [si+2],'6-'
		jnz		.fail
		cmp		word [si+4],'32'
		jnz		.fail
		cmp		byte [si+6],0
		jnz		.fail
		jmp		_jmp_386_32
.fail:		ret

; 386-16
_main_loop_command_386_16:
		cmp		word [si],'38'
		jnz		.fail
		cmp		word [si+2],'6-'
		jnz		.fail
		cmp		word [si+4],'16'
		jnz		.fail
		cmp		byte [si+6],0
		jnz		.fail
		jmp		_jmp_386_16
.fail:		ret

; 286
_main_loop_command_286:
		cmp		word [si],'28'
		jnz		.fail
		cmp		byte [si+2],'6'
		jnz		.fail
		cmp		byte [si+3],0
		jnz		.fail
		jmp		_jmp_286
.fail:		ret

; 8086
_main_loop_command_8086:
		cmp		word [si],'80'
		jnz		.fail
		cmp		word [si+2],'86'
		jnz		.fail
		cmp		byte [si+4],0
		jnz		.fail
		jmp		_jmp_8086
.fail:		ret

; EXEC command
; EXEC <seg>:<off>
_main_loop_command_exec:
		cmp		word [si],'EX'
		jnz		.fail
		cmp		word [si+2],'EC'
		jnz		.fail
		cmp		byte [si+4],' '
		jnz		.fail
		add		si,5
		pop		ax
		call		strtohex			; AX = seg
		mov		es,ax
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

_command_exec_far_ptr:
		dw		0,0

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
		cmp		dx,0x10
		jb		.addrok
		mov		si,address_invalid_msg
		jmp		_main_loop_command_response_output_err
.addrok:	call		str_skip_whitespace
		call		dx_ax_phys_to_real
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
		mov		ax,es
		add		ax,0x1000
		mov		es,ax
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
		cmp		dx,0x10
		jb		.addrok
		mov		si,address_invalid_msg
		jmp		_main_loop_command_response_output_err
.addrok:	call		str_skip_whitespace
		call		dx_ax_phys_to_real
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
		mov		ax,es
		add		ax,0x1000
		mov		es,ax
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
		cmp		dx,0x10				; in real mode on an 8086 we cannot address above the 1MB limit, >= 0x100000
		jb		.addrok
		mov		si,address_invalid_msg
		jmp		_main_loop_command_response_output_err
.addrok:	call		str_skip_whitespace
		call		dx_ax_phys_to_real		; convert DX:AX into real-mode pointer
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
		mov		ax,es
		add		ax,0x1000
		mov		es,ax
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
		cmp		dx,0x10				; in real mode on an 8086 we cannot address above the 1MB limit, >= 0x100000
		jb		.addrok
		mov		si,address_invalid_msg
		jmp		_main_loop_command_response_output_err
.addrok:	call		str_skip_whitespace
		call		dx_ax_phys_to_real		; convert DX:AX into real-mode pointer
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
		mov		ax,es
		add		ax,0x1000
		mov		es,ax
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

; convert physical addr DX:AX to real-mode address DX:AX
;
; A0000 = A000:0000
; 12345 = 1000:2345
dx_ax_phys_to_real:
		mov		cl,12
		shl		dx,cl				; DX <<= 12
		push		ax
		mov		cl,4
		shr		ax,cl
		or		dx,ax				; DX |= AX >> 4
		pop		ax
		and		ax,0Fh
		ret

; scan DS:SI forward to skip whitespace
str_skip_whitespace:
		cmp		byte [si],' '
		jz		.skip
.end		ret
.skip:		inc		si
		jmp		str_skip_whitespace

; print a string from DS:SI. use the BIOS.
puts:		push		si
		push		ax
putsl:		lodsb
		or		al,al
		jz		putse
		mov		ah,0Eh
		int		10h
		jmp		putsl
putse:		pop		ax
		pop		si
		ret

; print one char in AL
putc:		push		ax
		mov		ah,0Eh
		int		10h
		pop		ax
		ret

; print a hexadecimal digit in AL
puthex_digit:	push		ax
		push		bx
		and		ax,0Fh
		mov		bx,ax
		mov		al,[hexdigits + bx]
		call		putc
		pop		bx
		pop		ax
		ret

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
puthex:		push		ax				; AX = 1234
		rol		ax,4				; AX = 2341
		call		puthex_digit
		rol		ax,4				; AX = 3412
		call		puthex_digit
		rol		ax,4				; AX = 4123
		call		puthex_digit
		rol		ax,4				; AX = 1234
		call		puthex_digit
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

; strings
global hexdigits
hexdigits:	db		'0123456789ABCDEF'
comports:	dw		0x3F8,0x2F8,0x3E8,0x2E8
note_8086:	db		'8086',0

; variables
inited		db		0
global comport
comport		dw		0

; input buffer
in_buf		times IN_BUF_SIZE db 0

; jump here
global _jmp_8086
align		16
_jmp_8086:	cli
		mov		ax,cs
		mov		ds,ax
		mov		ss,ax
		mov		sp,7BFCh
		sti
		mov		si,ok_head
		call		com_str_out
		mov		si,note_8086
		call		com_str_out
		mov		si,crlf
		call		com_str_out
		jmp		_main_loop

; this must finish on a paragraph.
; when this and other images are catencated together this ensures each one can safely
; run from it's own 16-bit segment.
align		16

; other modules
extern _jmp_286
extern _jmp_386_16
extern _jmp_386_32
