
#include "stage2eth-base.h"
#include "datatypes.h"
#include "ioport.h"
#include "bios.h"
#include "vga.h"

#define IO_KEYBOARD_BUFFER	0x60
#define IO_KEYBOARD_COMMAND	0x64
#define IO_KEYBOARD_STATUS	0x64

void keyb8042_wait_for_output_ready() {
	while (io_inbi_d(IO_KEYBOARD_STATUS) & 2) io_delay();
}

void keyb8042_wait_for_input_ready() {
	while ((io_inbi_d(IO_KEYBOARD_STATUS) & 1) == 0) io_delay();
}

void keyb8042_write_buffer(unsigned char c) {
	keyb8042_wait_for_output_ready();
	io_outbi_d(IO_KEYBOARD_BUFFER,c);
}

int keyb8042_read_buffer_imm() {
	if (io_inbi_d(IO_KEYBOARD_STATUS) & 1)
		return io_inbi_d(IO_KEYBOARD_BUFFER);
	else
		return -1;
}

int keyb8042_read_buffer() {
	int r;
	while ((r=keyb8042_read_buffer_imm()) < 0) io_delay();
	return r;
}

void keyb8042_write_command(unsigned char c) {
	keyb8042_wait_for_output_ready();
	io_outbi_d(IO_KEYBOARD_COMMAND,c);
}

void keyb8042_write_command_byte(unsigned char c) {
	keyb8042_write_command(0x60);
	keyb8042_write_buffer(c);
	keyb8042_wait_for_output_ready();
}

void keyb8042_write_leds(unsigned char c) {
	keyb8042_write_buffer(0xED);
	keyb8042_write_buffer(c);
}

/* TODO: if PS/2 is not present, talk to USB keyboard + controller */
void keyb8042_init() {
	/* probe the I/O ports to see if they are there */
	/* hopefully we're on hardware that has it, or newer hardware that fakes it */
	io_inbi_d(IO_KEYBOARD_BUFFER);
	io_inbi_d(IO_KEYBOARD_BUFFER);
	io_inbi_d(IO_KEYBOARD_STATUS);
	io_inbi_d(IO_KEYBOARD_STATUS);

	/* so, are there I/O ports there? */
	if (io_inbi_d(IO_KEYBOARD_BUFFER) == 0xFF && io_inbi_d(IO_KEYBOARD_STATUS) == 0xFF) {
		vga_write("PS/2 or compatible keyboard controller not present\r\n");
		return;
	}

	/* so kick-start the thing */
	keyb8042_write_command(0xAA);		/* controller self-test */

	int self_test = keyb8042_read_buffer();
	if (self_test != 0x55) {
		vga_write("PS/2 or compatible keyboard controller gave incorrect response to self-test\r\n");
		return;
	}

	keyb8042_write_command_byte(0x33|0x40);	/* XT translation */
	keyb8042_write_command(0xAB);		/* keyboard interface test */
	int keyb8042_test = keyb8042_read_buffer();
	if (keyb8042_test >= 1 && keyb8042_test <= 3)
		vga_write("PS/2 keyboard self-test indicates possible problem\r\n");
	else if (keyb8042_test < 0 || keyb8042_test >= 0x10)
		vga_write("PS/2 keyboard self-test returned garbage results\r\n");

	{
		unsigned char c;
		keyb8042_write_buffer(0xFF);		/* reset */
		c = (unsigned char)keyb8042_read_buffer();
		if (c != 0xFA) c = keyb8042_read_buffer_imm();
		if (c != 0xFA) {
			keyb8042_read_buffer_imm();
			keyb8042_write_buffer(0xFF);		/* reset */
			c = (unsigned char)keyb8042_read_buffer();
			if (c != 0xFA) vga_write("PS/2 keyboard reset does not give acknowledgement\r\n");
		}

		c = (unsigned char)keyb8042_read_buffer();
		if (c != 0xAA) vga_write("PS/2 keyboard reset does not give self-test passed response\r\n");
	}

	keyb8042_write_buffer(0xF5);		/* reset to power on and wait for enable */
	if (keyb8042_read_buffer() != 0xFA) vga_write("PS/2 keyboard 0xF5: no ack\r\n");
	keyb8042_write_leds(0x0);		/* set LEDs */
	if (keyb8042_read_buffer() != 0xFA) vga_write("PS/2 keyboard set LEDs: no ack\r\n");

	/* please use scan code set #1 */
	keyb8042_write_buffer(0xF0);
	if (keyb8042_read_buffer() != 0xFA) vga_write("PS/2 keyboard set scan code #1 (first): no ack\r\n");
	keyb8042_write_buffer(1);
	if (keyb8042_read_buffer() != 0xFA) vga_write("PS/2 keyboard set scan code #1: no ack\r\n");

	keyb8042_write_command(0xAE);		/* enable keyboard */

	keyb8042_write_buffer(0xF4);		/* enable keyboard */
	if (keyb8042_read_buffer() != 0xFA) vga_write("PS/2 keyboard enable: no ack\r\n");
}

int keyb8042_readkey() {
	int b1 = keyb8042_read_buffer_imm();
	if (b1 < 0) return -1;

	/* parse extended keys */
	if (b1 == 0xE0) {
		int b2 = keyb8042_read_buffer();
		if (b2 < 0) return -1;
		b1 = 0xE000 | b2;
	}
	else if (b1 == 0xE1) {
		int b2 = keyb8042_read_buffer();
		if (b2 < 0) return -1;
		b1 = 0xE10000 | (b2 << 8);
		b2 = keyb8042_read_buffer();
		if (b2 < 0) return -1;
		b1 |= b2;
	}

	/* ignore key-up messages */
	if (b1 & 0x80) return -1;

	return b1;
}

int keyb8042_waitkey() {
	int r;

	while ((r = keyb8042_readkey()) == -1);
	return r;
}

