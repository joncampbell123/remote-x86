/* boot.S - bootstrap the kernel */
/* Copyright (C) 1999, 2001  Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#define ASM 1
#include "multiboot.h"

/* we're going to thunk down to 16-bit real mode so the GDT has to be below 1MB */
#define gdt		0x7000
#define prot16_temp	0x7100

.text

.globl  start, _start
start:
_start:
	jmp		multiboot_entry

/* Align 32 bits boundary. */
	.align		4

/* Multiboot header. */
multiboot_header:
	.long		MULTIBOOT_HEADER_MAGIC /* magic */
	.long		MULTIBOOT_HEADER_FLAGS /* flags */
	.long		-(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS) /* checksum */

multiboot_entry:
	/* Initialize the stack pointer. */
	movl		$(stack + STACK_SIZE), %esp

	/* Reset EFLAGS. */
	pushl		$0
	popf

	/* Push the pointer to the Multiboot information structure. */
	pushl		%ebx
	/* Push the magic value. */
	pushl		%eax

	/* copy our payload down into memory */
	push		%ds
	pop		%es
	mov		$(payload),%esi
	mov		$0x8000,%edi
	mov		$((payload_end-payload)>>2),%ecx
	cld
	rep		movsd

	/* make up some GDT to thunk by */
	mov		$(gdt+8),%edi
	movw		$((8*3)-1),gdtr
	movl		$gdt,gdtr+2
	lgdt		gdtr

	/* code(1) */
	movw		$0xFFFF,0(%edi)			/* limit=full 4GB 32-bit code ring 0 */
	movw		$0x0000,2(%edi)
	movw		$0x9A00,4(%edi)
	movw		$0x00CF,6(%edi)
	addl		$8,%edi

	/* data(2) */
	movw		$0xFFFF,0(%edi)			/* limit=full 4GB 32-bit data ring 0 */
	movw		$0x0000,2(%edi)
	movw		$0x9200,4(%edi)
	movw		$0x00CF,6(%edi)

	/* take effect */
	mov		$(8*2),%eax
	mov		%ax,%ds
	mov		%ax,%es
	mov		%ax,%ss
	jmp		$(8*1),$tramp1
tramp1:	/* so we're now running on the two we just made up */

	/* copy the code down below the 1MB mark */
	mov		$copyme16,%esi
	mov		$prot16_temp,%edi
	mov		$32,%ecx
	cld
	rep		movsl

	/* change the code selector to 16-bit */
	mov		$(gdt+8),%edi
	movb		$0x8F,6(%edi)
	mov		$(gdt+16),%edi
	movb		$0x8F,6(%edi)

	/* reload regs */
	mov		$(8*2),%eax
	mov		%ax,%ds
	mov		%ax,%es
	mov		%ax,%ss
	jmp		$(8*1),$prot16_temp

	/* payload */
	.align		4
payload:
.incbin	"stage2-recomm.bin"
payload_end:

	/* GDT */
	.align		16
gdtr:	.space		8

/* this code is copied down below 1MB and executed */
.code16
copyme16:
	mov		%cr0,%eax
	and		$(~0x80000001),%eax
	mov		%eax,%cr0

	xor		%eax,%eax
	mov		%ax,%ds
	mov		%ax,%es
	mov		%ax,%ss
	jmp		$0x0000,$0x8000
.code32

	/* Our stack area. */
	.comm   stack, STACK_SIZE

