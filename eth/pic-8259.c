
#include "stage2eth-base.h"
#include "interrupt.h"
#include "datatypes.h"
#include "keyb8042.h"
#include "pit-8253.h"
#include "ioport.h"
#include "bios.h"
#include "vga.h"

#define PIC_8259_PIC1		0x20
#define PIC_8259_PIC2		0xA0

DECLARE_INTERRUPT_FUNCTION(default_irq_pic1_0); void default_irq_pic1_0() { do_irq(0); }
DECLARE_INTERRUPT_FUNCTION(default_irq_pic1_1); void default_irq_pic1_1() { do_irq(1); }
DECLARE_INTERRUPT_FUNCTION(default_irq_pic1_2); void default_irq_pic1_2() { do_irq(2); }
DECLARE_INTERRUPT_FUNCTION(default_irq_pic1_3); void default_irq_pic1_3() { do_irq(3); }
DECLARE_INTERRUPT_FUNCTION(default_irq_pic1_4); void default_irq_pic1_4() { do_irq(4); }
DECLARE_INTERRUPT_FUNCTION(default_irq_pic1_5); void default_irq_pic1_5() { do_irq(5); }
DECLARE_INTERRUPT_FUNCTION(default_irq_pic1_6); void default_irq_pic1_6() { do_irq(6); }
DECLARE_INTERRUPT_FUNCTION(default_irq_pic1_7); void default_irq_pic1_7() { do_irq(7); }

DECLARE_INTERRUPT_FUNCTION(default_irq_pic2_8); void default_irq_pic2_8() { do_irq(8); }
DECLARE_INTERRUPT_FUNCTION(default_irq_pic2_9); void default_irq_pic2_9() { do_irq(9); }
DECLARE_INTERRUPT_FUNCTION(default_irq_pic2_10); void default_irq_pic2_10() { do_irq(10); }
DECLARE_INTERRUPT_FUNCTION(default_irq_pic2_11); void default_irq_pic2_11() { do_irq(11); }
DECLARE_INTERRUPT_FUNCTION(default_irq_pic2_12); void default_irq_pic2_12() { do_irq(12); }
DECLARE_INTERRUPT_FUNCTION(default_irq_pic2_13); void default_irq_pic2_13() { do_irq(13); }
DECLARE_INTERRUPT_FUNCTION(default_irq_pic2_14); void default_irq_pic2_14() { do_irq(14); }
DECLARE_INTERRUPT_FUNCTION(default_irq_pic2_15); void default_irq_pic2_15() { do_irq(15); }

void pic_8259_init() {
	unsigned int i;

	if (io_inbi_d(PIC_8259_PIC1) == 0xFF && io_inbi_d(PIC_8259_PIC2) == 0xFF) {
		vga_write("8259 PIC is missing\r\n");
		return;
	}

	/* primary */
	io_outbi_d(PIC_8259_PIC1,  0x11);		/* ICW1 (0x10) edge triggered (0x08) Call address interval 4 (0x04) ICW4 needed (0x01) */
	io_outbi_d(PIC_8259_PIC1+1,0x20);		/* ICW2 IRQ0-7 moved to interrupt 0x20 */
	io_outbi_d(PIC_8259_PIC1+1,0x04);		/* ICW3 IRQ2 has a slave */
	io_outbi_d(PIC_8259_PIC1+1,0x03);		/* ICW4 bufferd master (0x0C) 8086 mode (0x01) */
	io_outbi_d(PIC_8259_PIC1+1,0x00);		/* mask all interrupts */

	/* secondary */
	io_outbi_d(PIC_8259_PIC2,  0x11);		/* ICW1 (0x10) edge triggered (0x08) call address interval 4 (0x04) ICW4 needed (0x01) */
	io_outbi_d(PIC_8259_PIC2+1,0x28);		/* ICW2 IRQ8-15 moved to interrupt 0x28 */
	io_outbi_d(PIC_8259_PIC2+1,0x02);		/* ICW3 slave on IRQ 2 */
	io_outbi_d(PIC_8259_PIC2+1,0x03);		/* ICW4 bufferd slave (0x08) 8086 mode (0x01) */
	io_outbi_d(PIC_8259_PIC2+1,0x00);		/* mask all interrupts */

	/* now "forgive" all pending interrupts */
	for (i=0;i < 8;i++) io_outbi_d(PIC_8259_PIC1,0x60+i);	/* specific EOF all interrupts */
	for (i=0;i < 8;i++) io_outbi_d(PIC_8259_PIC2,0x60+i);	/* 2nd one too */

	/* set up IDTs */
	set_idt(0x20,INTERRUPT_FUNCTION(default_irq_pic1_0));
	set_idt(0x21,INTERRUPT_FUNCTION(default_irq_pic1_1));
	set_idt(0x22,INTERRUPT_FUNCTION(default_irq_pic1_2));
	set_idt(0x23,INTERRUPT_FUNCTION(default_irq_pic1_3));
	set_idt(0x24,INTERRUPT_FUNCTION(default_irq_pic1_4));
	set_idt(0x25,INTERRUPT_FUNCTION(default_irq_pic1_5));
	set_idt(0x26,INTERRUPT_FUNCTION(default_irq_pic1_6));
	set_idt(0x27,INTERRUPT_FUNCTION(default_irq_pic1_7));

	set_idt(0x28,INTERRUPT_FUNCTION(default_irq_pic2_8));
	set_idt(0x29,INTERRUPT_FUNCTION(default_irq_pic2_9));
	set_idt(0x2A,INTERRUPT_FUNCTION(default_irq_pic2_10));
	set_idt(0x2B,INTERRUPT_FUNCTION(default_irq_pic2_11));
	set_idt(0x2C,INTERRUPT_FUNCTION(default_irq_pic2_12));
	set_idt(0x2D,INTERRUPT_FUNCTION(default_irq_pic2_13));
	set_idt(0x2E,INTERRUPT_FUNCTION(default_irq_pic2_14));
	set_idt(0x2F,INTERRUPT_FUNCTION(default_irq_pic2_15));
}

void pic_8259_seoi(unsigned int i) {
	if (i >= 0 && i <= 7)
		io_outbi_d(PIC_8259_PIC1,0x60+i);
	else if (i >= 8 && i <= 15)
		io_outbi_d(PIC_8259_PIC2,0x60+i-8);
}

unsigned int pending_interrupts() {
	unsigned char irr1,irr2;

	io_outbi_d(PIC_8259_PIC1,0x08 | 0x02);		/* OCW3 read IRR */
	irr1 = io_inbi_d(PIC_8259_PIC1);

	io_outbi_d(PIC_8259_PIC2,0x08 | 0x02);		/* OCW3 read IRR */
	irr2 = io_inbi_d(PIC_8259_PIC2);

	return ((unsigned int)irr1) | (((unsigned int)irr2) << 8);
}

unsigned int in_service_interrupts() {
	unsigned char irr1,irr2;

	io_outbi_d(PIC_8259_PIC1,0x08 | 0x03);		/* OCW3 read ISR */
	irr1 = io_inbi_d(PIC_8259_PIC1);

	io_outbi_d(PIC_8259_PIC2,0x08 | 0x03);		/* OCW3 read ISR */
	irr2 = io_inbi_d(PIC_8259_PIC2);

	return ((unsigned int)irr1) | (((unsigned int)irr2) << 8);
}

