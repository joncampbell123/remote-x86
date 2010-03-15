
GDT_ENTRIES		equ		32
IN_BUF_SIZE		equ		256
BAUD_DIVISOR		equ		(115200/12)	; 9600

USER_CODE_SELECTOR	equ		0x20			; used by EXEC to run user code in it's own segment
ACCESS_SELECTOR		equ		0x18			; used by READ/WRITE commands because 286 segments are limited to 64KB
DATA_SELECTOR		equ		0x10
CODE_SELECTOR		equ		0x08

%ifndef MAIN_SRC
extern address_invalid_msg
extern unknown_command_msg
extern write_complete_msg
extern exec_complete_msg
extern test_response
extern err_head
extern ok_head
extern crlf
extern hexdigits
extern comport
extern in_buf
extern _jmp_8086
%endif

%ifndef MAIN_286
extern _gdtr
extern _gdtr_old
extern _gdt_table
extern _idtr_brainfuck
extern _idtr_realmode
%endif

