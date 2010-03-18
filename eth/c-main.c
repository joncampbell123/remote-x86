
#include "stage2eth-base.h"
#include "interrupt.h"
#include "datatypes.h"
#include "keyb8042.h"
#include "pic-8259.h"
#include "pit-8253.h"
#include "ioport.h"
#include "bios.h"
#include "vga.h"

void timer_init() {
	pit_8253_init();
}

void keyboard_init() {
	keyb8042_init();
}

void interrupt_init() {
	idt_init();
	pic_8259_init();
}

void c_start() {
	vga_init();
	vga_write("Setting up interrupts\r\n");	interrupt_init();
	vga_write("Setting up timer\r\n");	timer_init();
	vga_write("Setting up keyboard\r\n");	keyboard_init();
	vga_write("Main loop\r\n");
	sti();

	hang();
}

