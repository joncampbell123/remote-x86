
; the 8086 portion took care of setting up the UART, we just continue to use it
extern _jmp_8086

bits 16
use16

SECTION		.text

; jump here
global _jmp_x64
align		16
_jmp_x64:	jmp		_jmp_8086

