use64

%define MAIN_64
%include "global.inc"

;IN_BUF_SIZE	equ		256

PML2_ENTRIES	equ		16384
PML4_LOCATION	equ		0x10000							; where we keep the PML4 page
PML3_LOCATION	equ		PML4_LOCATION + 0x1000					; PML3 where we keep 4 entries for memory ranges 0-4GB
PML2_LOCATION	equ		PML3_LOCATION + 0x1000					; PML3 is one page long so we store the actual 2MB entries from here on out
											; PML3 is arranged so that this part is just one big long array with 1:1
											; correllation between memory and entry. 4 * 512 * 8 = 16384 bytes to cover
											; 16384/8 * 2MB = 2048 * 2MB = 4GB
											; UPDATE: We can up that to 256KB to cover the entire 64GB range on current hardware :)
FINAL_LOW	equ		PML2_LOCATION + (256 * 1024)

; just enough to cover 4GB. We use 2MB large pages for extra laziness.
; PML4              PML3               PML2
;   [0] ---------->  [0] 1GB ---------> [0] 2MB -------------> actual page
;  N/A               [1] 2GB            [1] 2MB
;  N/A               [2] 3GB            [2] 2MB
;  N/A               [3] 4GB            [3] 2MB

;CODE_SELECTOR	equ		0x08
;DATA_SELECTOR	equ		0x10

; this code is jumped to from the 8086 portion.
; you'd better be running this on a 386 or higher because this makes no
; attempt to detect whether the CPU actually supports the mode.
;
; the 8086 portion took care of setting up the UART, we just continue to use it

SECTION		.text

; main loop
_main_loop:	cli					; keep interrupts disabled

		mov		rbx,in_buf
		xor		rsi,rsi			; BX+SI is used to index into in_buf
		
_main_loop_read:call		com_char_in

		cmp		al,10			; the message ends at the newline '\n'
		jz		short _main_loop_process
		cmp		al,13			; the message ends at the newline '\n' (damn you PuTTY)
		jz		short _main_loop_process
		cmp		al,27			; ESC + binary value desired?
		jnz		short _main_loop_read_al
		call		com_char_in		; read in the binary, it won't be taken as newline

_main_loop_read_al:
		cmp		rsi,IN_BUF_SIZE-1	; don't overrun our buffer
		jae		short _main_loop_read

		mov		[rbx+rsi],al		; store the char
		inc		rsi
		jmp		_main_loop_read

_main_loop_process:
		lea		rdi,[rbx+rsi]		; store the end of the string
		mov		rsi,rbx			; SI = in_buf

; clear the rest of the buffer
		push		rdi
		cld
		xor		al,al
_main_loop_process_clr:
		stosb
		cmp		rdi,IN_BUF_SIZE + in_buf
		jb		_main_loop_process_clr
		pop		rdi

		cmp		rsi,rdi			; empty strings are ignored
		jz		short _main_loop

		call		_main_loop_command
		jnc		short _main_loop	; command function will return with CF=0 if it understood it

		mov		rsi,err_head
		call		com_str_out
		mov		rsi,unknown_command_msg
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
		cmp		word [rsi],'LO'
		jnz		.fail
		cmp		byte [rsi+2],'W'
		jnz		.fail
		cmp		byte [rsi+3],0
		jnz		.fail
		pop		rax
		mov		rsi,ok_head
		call		com_str_out
		mov		rax,FINAL_LOW
		call		com_hex_out
		mov		rsi,crlf
		call		com_str_out
.fail:		ret

; 8086
_main_loop_command_8086:
		cmp		dword [rsi],'8086'
		jnz		.fail
		cmp		byte [rsi+4],0
		jnz		.fail
		; UGH it seems we first have to thunk down to 16-bit protected mode, or else Bochs will continue
		; executing 32-bit real mode code heee
		mov		byte [_gdt_table + CODE_SELECTOR + 6],0x8F		; poof, we're 16-bit
		mov		byte [_gdt_table + DATA_SELECTOR + 6],0x8F		; poof, our data segment is 16-bit
		mov		ax,DATA_SELECTOR		; now force the CPU to update by that
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
		jmp far	qword	[rel .jmpto]
.jmpto:		dq		.thunk16
		dw		CODE_SELECTOR
.thunk16:
use16		; switch the CPU back to real mode
		lgdt		[cs:_gdtr_old]
		lidt		[cs:_idtr_realmode]		; on a 386, we then load a sane IDT for real mode
		xor		eax,eax				; turn off protected mode
		mov		cr0,eax
		jmp		0x0000:word .realmode
		; real mode, reload sectors
.realmode:	xor		ax,ax
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
		jmp		0x0000:_jmp_8086
use64
.fail:		ret

; EXEC command
; EXEC <off>
;  in this case, the "segment" value is a byte offset where the code segment starts
_main_loop_command_exec:
		cmp		dword [rsi],'EXEC'
		jnz		.fail
		cmp		byte [rsi+4],' '
		jnz		.fail
		add		rsi,5
		pop		rax
		call		strtohex
		mov		rbx,rax				; ES:BX = pointer to function

; run the code (near call)
		call		rbx

; done
		mov		rsi,exec_complete_msg
		jmp		_main_loop_command_response_output

.fail:		ret

; WRITE command
_main_loop_command_write:
		cmp		dword [rsi],'WRIT'
		jnz		.fail
		cmp		word [rsi+4],'E '
		jnz		.fail
		add		rsi,6				; accpeded, skip to addr
		pop		rax				; rip away return
		call		strtohex			; addr -> DX:AX
		call		str_skip_whitespace
		mov		rbx,rax

; bytes to write are read from the command
.writeloop:	call		str_skip_whitespace
		cmp		rsi,rdi
		jae		.writeloopend
		
		call		strtohex
		call		str_skip_whitespace
		mov		[rbx],al

		inc		rbx
		jmp		.writeloop
.writeloopend:
		mov		rsi,write_complete_msg
		jmp		_main_loop_command_response_output

.fail:		ret		

; WRITEB command
_main_loop_command_writeb:
		cmp		dword [rsi],'WRIT'
		jnz		.fail
		cmp		word [rsi+4],'EB'
		jnz		.fail
		cmp		byte [rsi+6],' '
		jnz		.fail
		add		rsi,7				; accpeded, skip to addr
		pop		rax				; rip away return
		call		strtohex			; addr -> DX:AX
		call		str_skip_whitespace
		mov		rbx,rax

		; how many bytes?
		call		strtohex
		mov		rcx,rax

		; binary data following it determines what we write.
		; the host is expected to write EXACTLY the byte count it said
.writeloop:	or		rcx,rcx
		jz		.writeloopend
		dec		rcx

		call		com_char_in
		mov		[rbx],al

		inc		rbx
		jnc		.writeloop
.writeloopend:
		mov		rsi,write_complete_msg
		jmp		_main_loop_command_response_output

.fail:		ret		

; READ command
; READ <phys memaddress> <number of bytes>
_main_loop_command_read:
		cmp		dword [rsi],'READ'
		jnz		.fail
		cmp		byte [rsi+4],' '
		jnz		.fail
		add		rsi,5				; accepted, now parse mem address
		pop		rax				; rip away return to _main_loop_command so we fall back to main loop
		call		strtohex			; parse string into binary value (string hex digits to binary) -> DX:AX
		call		str_skip_whitespace
		mov		rbx,rax				; we'll use ES:BX to read

		; how many bytes?
		call		strtohex
		mov		rcx,rax

		; start of message
		mov		rsi,ok_head
		call		com_str_out

.readloop:	or		rcx,rcx
		jz		.readloopend
		dec		rcx

		mov		al,[rbx]
		call		com_hex8_out
		mov		al,' '
		call		com_char_out
		inc		rbx
		jmp		.readloop
.readloopend:

		; end of message
		mov		rsi,crlf
		call		com_str_out
		clc
.fail:		ret

; READB command
; READB <phys memaddress> <number of bytes>
_main_loop_command_readb:
		cmp		dword [rsi],'READ'
		jnz		.fail
		cmp		word [rsi+4],'B '
		jnz		.fail
		add		rsi,6				; accepted, now parse mem address
		pop		rax				; rip away return to _main_loop_command so we fall back to main loop
		call		strtohex			; parse string into binary value (string hex digits to binary) -> DX:AX
		call		str_skip_whitespace
		mov		rbx,rax				; we'll use ES:BX to read

		; how many bytes?
		call		strtohex
		mov		rcx,rax

		; start of message
		mov		rsi,ok_head
		call		com_str_out

.readloop:	or		rcx,rcx
		jz		.readloopend
		dec		rcx

		mov		al,[rbx]
		call		com_char_out
		inc		rbx
		jmp		.readloop
.readloopend:

		; end of message
		mov		rsi,crlf
		call		com_str_out
		clc
.fail:		ret

; TEST command
_main_loop_command_test:
		cmp		dword [rsi],'TEST'
		jnz		.fail
		cmp		byte [rsi+4],0
		jnz		.fail
; OK-----it is TEST
		pop		rax				; remove stack frame back to _main_loop_command so we head straight back to the caller of that function
		mov		rsi,test_response
		jmp		_main_loop_command_response_output
		call		com_str_out
.fail:		ret						; on failure, we go back to _main_loop_command. on success, we go straight back to _main_loop_process_clr

; SI = message to return with 'OK' header. caller should JUMP to us not CALL us. well actually it doesn't matter, but that's the intent.
_main_loop_command_response_output:
		push		rsi
		mov		rsi,ok_head
		call		com_str_out
		pop		rsi
		call		com_str_out
		mov		rsi,crlf
		call		com_str_out
		clc
		ret

; SI = message to return with 'ERR' header. caller should JUMP to us not CALL us. well actually it doesn't matter, but that's the intent.
_main_loop_command_response_output_err:
		push		rsi
		mov		rsi,err_head
		call		com_str_out
		pop		rsi
		call		com_str_out
		mov		rsi,crlf
		call		com_str_out
		clc
		ret

_main_loop_command_understood_generic:
		mov		rsi,ok_head
		call		com_str_out
		mov		rsi,crlf
		call		com_str_out
		clc
		ret

; strtohex_getdigit
; from DS:SI read one char and convert to hex. if not hex digit, then CF=1
strtohex_getdigit:
		push		rax
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
		pop		rax
		ret

.err:		stc
		pop		rax
		ret

; scan string from DS:SI returning in AX.
; SI is returned adjusted forward to the first non-digit char
strtohex:	push		rbx
		xor		rax,rax
.loop:		call		strtohex_getdigit
		jc		.end			; function will set CF=1 if it isn't a hex digit. else the converted digit will be in BL

		shl		rax,4
		or		al,bl			; AX = (AX << 4) | BL
		jmp		.loop

.end:		pop		rbx
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

; scan DS:SI forward to skip whitespace
str_skip_whitespace:
		cmp		byte [rsi],' '
		jz		.skip
.end		ret
.skip:		inc		rsi
		jmp		str_skip_whitespace

; print a hexadecimal digit in AL
com_puthex_digit:
		push		rax
		push		rbx
		and		rax,0Fh
		mov		rbx,rax
		mov		al,[hexdigits + rbx]
		call		com_char_out
		pop		rbx
		pop		rax
		ret

; print a hex number in AX
com_hex8_out:	push		rax				; AX = 1234
		rol		al,4				; AX = 2341
		call		com_puthex_digit
		rol		al,4				; AX = 3412
		call		com_puthex_digit
		pop		rax
		ret

; print a hex number in AX
com_hex_out:	push		rax				; AX = 1234
		rol		rax,4				; AX = 2341
		call		com_puthex_digit
		rol		rax,4				; AX = 3412
		call		com_puthex_digit
		rol		rax,4				; AX = 4123
		call		com_puthex_digit
		rol		rax,4				; AX = 1234
		call		com_puthex_digit
		rol		rax,4				; AX = 2341
		call		com_puthex_digit
		rol		rax,4				; AX = 3412
		call		com_puthex_digit
		rol		rax,4				; AX = 4123
		call		com_puthex_digit
		rol		rax,4				; AX = 1234
		call		com_puthex_digit
		rol		rax,4				; AX = 2341
		call		com_puthex_digit
		rol		rax,4				; AX = 3412
		call		com_puthex_digit
		rol		rax,4				; AX = 4123
		call		com_puthex_digit
		rol		rax,4				; AX = 1234
		call		com_puthex_digit
		rol		rax,4				; AX = 2341
		call		com_puthex_digit
		rol		rax,4				; AX = 3412
		call		com_puthex_digit
		rol		rax,4				; AX = 4123
		call		com_puthex_digit
		rol		rax,4				; AX = 1234
		call		com_puthex_digit
		pop		rax
		ret

; read one byte from the comport
com_char_in:	push		rdx
		mov		dx,[comport]
		add		dx,5				; DX = Line status register
com_char_in_wait:
		in		al,dx
		test		al,1
		jz		com_char_in_wait
		sub		dx,5				; DX = recieve buffer
		in		al,dx
		pop		rdx
		ret

; write one byte to the comport from AL
com_char_out:	push		rdx
		push		rax
		push		rcx
		mov		dx,[comport]
		add		dx,5
		xor		cx,cx
com_char_out_wait:
		in		al,dx
		test		al,0x60				; is transmit buffer empty?
		jz		com_char_out_wait		; if not, loop again
		mov		dx,[comport]
		pop		rcx
		pop		rax
		out		dx,al
		pop		rdx
		ret

com_str_out:	push		rsi
		push		rax
com_str_outl:	lodsb
		or		al,al
		jz		com_str_oute
		call		com_char_out
		jmp		com_str_outl
com_str_oute:	pop		rax
		pop		rsi
		ret

; we are jumped to from the 8086 loader, so this part is real-mode code
global _jmp_x64
use16			; <- Woo! Nasm lets me use 16-bit code in an ELF64 object file!

_jmp_x64:	cli
		mov		ax,cs
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
		mov		sp,0x7BF0

		mov		word [_gdtr],0xFFFF	; limit
		mov		dword [_gdtr+2],_gdt_table ; offset
		call		gen_gdt_32

		sidt		[_idtr_realmode]

		sgdt		[_gdtr_old]
		lgdt		[_gdtr]

		mov		eax,0x00000001
		mov		cr0,eax			; enable protected mode but NOT paging

		jmp		dword CODE_SELECTOR:.protmode32
use32
.protmode32:
		mov		ax,DATA_SELECTOR
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
		mov		fs,ax
		mov		gs,ax

; update the GDT to enable long mode when we jump.
; in the mean time the cached segment registers we're using still reflect 32-bit code+data,
; just DON'T RELOAD THEM!
		mov		edi,_gdt_table
		mov		byte [edi + CODE_SELECTOR + 6],0xAF		; D=0 L=1 64-bit code
		mov		byte [edi + DATA_SELECTOR + 6],0xAF		; D=0 L=1 64-bit data

; switch on A20. anything that supports 64-bit is new enough to support the port 0x92
; trick (fast A20) and is too new to suffer the quirks mentioned on various sites about
; embedded video cards screwing up when we use it.
		in		al,0x92
		or		al,2		; enable A20
		and		al,0xFE		; DONT reset
		out		0x92,al

; build the damn page tables. 4 levels deep even!
; keep it simple by only mapping the first 4GB of memory.
; the host can upload code later that fills out the rest if it cares to
; peek above 4GB.
		cld
		mov		edi,PML4_LOCATION
		mov		ecx,511		; 512 * 64-bit entries = 512 * 8 = 4096
		; make ONE entry for the 0x0000`0000`0000 - 0x007F`FFFF`FFFF range
		mov		eax,PML3_LOCATION | 7 ; Present, Writeable, Supervisor, base 12:31 = page table addr
		stosd
		xor		eax,eax		; EBX=0 and upper 32 bits zero
		stosd
		; and the rest are N/A
.pml4zero:	xor		eax,eax
		stosd
		stosd
		loop		.pml4zero
; okay, and then just enough Page Directory Pointer entries to cover the first 4GB. each entry is indexed from linear addr bits 30:38
; this generates entries to make the lower level one big convenient array 
		xor		edx,edx		; this is used to count bits 30:31 for the loop below
		mov		edi,PML3_LOCATION
.pml3loop:	mov		eax,edx
		shl		eax,12		; EAX = (EDX << 12) | 7
		add		eax,PML2_LOCATION
		or		eax,0x00000007	; Present, Writeable, Supervisor, base 12:31
		stosd
		xor		eax,eax		; EBX=0 and upper 32 bits zero
		stosd
		inc		edx
		cmp		edx,64		; 64GB max
		jb		.pml3loop
		; and fill out the rest as N/A
.pml3zero:	xor		eax,eax
		stosd
		stosd
		inc		edx
		cmp		edx,512
		jb		.pml3zero

; and finally fill out the mappings. use 2MB large pages. the above loop delibrately placed the upper
; level entries to make this lower level one big array, so our job is easy.
		xor		edx,edx
		mov		edi,PML2_LOCATION
.pml2loop:	mov		eax,edx
		shl		eax,21
		or		eax,0x00000087	; Present, Writeable, Supervisor, base, 2MB large
		stosd
		xor		eax,eax
		stosd
		inc		edx
		cmp		edx,512*64	; 64 x 1GB
		jb		.pml2loop

; switch on long mode
		mov		eax,0x000000B0	; enable PGE, PAE, PSE
		mov		cr4,eax

		mov		ecx,0xC0000080	; EFER MSR
		rdmsr
		or		eax,0x00000100	; turn on long mode (LME) and IA-32e mode (LMA)
		wrmsr

		mov		eax,PML4_LOCATION
		mov		cr3,eax		; where page table level 4 is

		mov		eax,cr0
		or		eax,0x80000001	; protected mode + paging
		mov		cr0,eax

		jmp		CODE_SELECTOR:.longmode
use64
.longmode:	mov		rsi,ok_head
		call		com_str_out
		mov		rsi,note_x64
		call		com_str_out
		mov		rsi,crlf
		call		com_str_out
		jmp		_main_loop

; generate 386 GDT
use16
gen_gdt_32:	mov		di,_gdt_table
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
; done
		ret

; strings
note_x64:	db		'x64',0

align		16

