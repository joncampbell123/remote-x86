
#include "stage2eth-base.h"
#include "datatypes.h"
#include "ioport.h"

typedef uint16_t		vga_char_t;

extern vga_char_t*		VGA_alpha;
extern unsigned int		VGA_alpha_rows,VGA_alpha_columns;
extern unsigned int		VGA_alpha_x,VGA_alpha_y;
extern unsigned int		VGA_iobase;
extern unsigned char		VGA_color;

vga_char_t *vga_alpha_char_ptr(unsigned int x,unsigned int y);
void vga_writehex(unsigned int val,unsigned int digits);
void vga_scrollup();
void vga_crtc_write(unsigned char index,unsigned char val);
void vga_update_cursor();
void vga_cursor_down();
void vga_cursor_right();
void vga_writechar(char c);
void vga_write(const char *s);
void vga_init();

#define vga_write_hex(x)	vga_writehex(x,sizeof(x)*2)

