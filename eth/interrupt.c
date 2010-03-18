
#include "stage2eth-base.h"
#include "interrupt.h"
#include "datatypes.h"
#include "keyb8042.h"
#include "pit-8253.h"
#include "ioport.h"
#include "bios.h"
#include "vga.h"

unsigned char __attribute__((aligned(16))) IDT[256*8];
unsigned char __attribute__((aligned(8))) IDTR[8];

void (*irq_function[MAX_IRQ])(int n);

extern void default_int();
__asm__ (	"default_int:	iret");

void idt_init() {
	unsigned int i;

	memset(IDT,0,sizeof(IDT));
	for (i=0;i < 256;i++) {
		unsigned char *b = IDT + (i * 8);
		uint32_t offset = (uint32_t)default_int;

		*((uint16_t*)(b+0)) = (uint16_t)offset;
		*((uint16_t*)(b+2)) = CODE_SELECTOR;
		*((uint16_t*)(b+4)) = 0x8E00;	/* interrupt gate */
		*((uint16_t*)(b+6)) = (uint16_t)(offset >> 16UL);
	}

	*((uint16_t*)(IDTR+0)) = (256 * 8) - 1;
	*((uint32_t*)(IDTR+2)) = (uint32_t)IDT;
	__asm__ __volatile__ ("lidt IDTR");

	for (i=0;i < MAX_IRQ;i++) irq_function[i] = NULL;
}

void set_idt(unsigned char interrupt,void *intfunc) {
	unsigned char *b = IDT + ((unsigned int)interrupt * 8);
	uint32_t offset = (uint32_t)intfunc;
	*((uint16_t*)(b+0)) = (uint16_t)offset;
	*((uint16_t*)(b+6)) = (uint16_t)(offset >> 16UL);
}

void do_irq(unsigned int n) {
	if (n >= MAX_IRQ) return;
	(*((uint16_t*)(0xB8000+(n*2))))++;
	if (irq_function[n] == NULL) return;
	irq_function[n](n);
}

