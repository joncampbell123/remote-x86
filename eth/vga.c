
#include "stage2eth-base.h"
#include "datatypes.h"
#include "ioport.h"
#include "bios.h"
#include "vga.h"

/* TODO: move elsewhere */
const char*			hexes = "0123456789ABCDEF";

vga_char_t*			VGA_alpha=NULL;
unsigned int			VGA_alpha_rows,VGA_alpha_columns;
unsigned int			VGA_alpha_x,VGA_alpha_y;
unsigned int			VGA_iobase;

vga_char_t *vga_alpha_char_ptr(unsigned int x,unsigned int y) {
	return VGA_alpha + (y * VGA_alpha_columns) + x;
}

void vga_scrollup() {
	memcpy(VGA_alpha,VGA_alpha+VGA_alpha_columns,(VGA_alpha_rows-1)*VGA_alpha_columns*sizeof(vga_char_t));
	memset(VGA_alpha+((VGA_alpha_rows-1)*VGA_alpha_columns),0,VGA_alpha_columns*sizeof(vga_char_t));
}

void vga_crtc_write(unsigned char index,unsigned char val) {
	io_outb(VGA_iobase+4,index);
	io_outb(VGA_iobase+5,val);
}

void vga_update_cursor() {
	unsigned int cursor_pos = (VGA_alpha_y * VGA_alpha_columns) + VGA_alpha_x;
	vga_crtc_write(0x0E,cursor_pos>>8);
	vga_crtc_write(0x0F,cursor_pos);
}

void vga_cursor_down() {
	if (++VGA_alpha_y >= VGA_alpha_rows) {
		VGA_alpha_y = VGA_alpha_rows-1;
		vga_scrollup();
	}
}

void vga_cursor_right() {
	if (++VGA_alpha_x >= VGA_alpha_columns) {
		VGA_alpha_x = 0;
		vga_cursor_down();
	}
}

void vga_writehex(unsigned int val,unsigned int digits) {
	while (digits-- != 0) {
		unsigned int v = (val >> (digits * 4)) & 0xF;
		vga_writechar(hexes[v]);
	}
}

void vga_writechar(char c) {
	if (c == 13) {	/* CR */
		VGA_alpha_x = 0;
	}
	else if (c == 10) { /* LF */
		vga_cursor_down();
	}
	else {
		*vga_alpha_char_ptr(VGA_alpha_x,VGA_alpha_y) = 0x0700 | ((vga_char_t) ((unsigned char)c));
		vga_cursor_right();
	}

	vga_update_cursor();
}

void vga_write(const char *s) {
	char c;
	while ((c = *s++) != 0)
		vga_writechar(c);
}

void vga_init() {
	/* most modern systems have VGA alphanumeric RAM at 0xB8000 */
	VGA_alpha = (vga_char_t*)(0xB8000);
	VGA_alpha_rows = 80;
	VGA_alpha_columns = 25;
	VGA_iobase = 0x3D0;

	/* but the BIOS may have configured VGA differently, who knows? */
	{
		unsigned char bios_rows = *BIOS_DATA_AREA(uint8_t,0x84) + 1;
		unsigned char bios_cols = *BIOS_DATA_AREA(uint16_t,0x4A);

		if ((bios_cols == 40 || bios_cols == 80 || bios_cols == 32) && (bios_rows >= 10 && bios_rows <= 60)) {
			VGA_alpha_rows = bios_rows;
			VGA_alpha_columns = bios_cols;
		}
	}

	VGA_alpha_x = 0;
	VGA_alpha_y = VGA_alpha_rows-1;

	/* the BIOS knows where the cursor is */
	{
		unsigned char x = *BIOS_DATA_AREA(uint8_t,0x50);
		unsigned char y = *BIOS_DATA_AREA(uint8_t,0x51);
		if (x >= 0 && x < VGA_alpha_columns && y >= 0 && y < VGA_alpha_rows) {
			VGA_alpha_x = x;
			VGA_alpha_y = y;
		}
	}
}





