
#include "stage2eth-base.h"
#include "datatypes.h"
#include "keyb8042.h"
#include "pit-8253.h"
#include "ioport.h"
#include "bios.h"
#include "vga.h"

#define IO_PIT_8253_TIMER0		0x40
#define IO_PIT_8253_TIMER1		0x41
#define IO_PIT_8253_TIMER2		0x42
#define IO_PIT_8253_CONTROL		0x43

/* notice we use the "countdown" mode not the periodic mode.
 * we don't need the ticker running in the background */

void pit_8253_init() {
	pit_8253_start_countdown();
}

void pit_8253_start_countdown() {
	io_outbi(IO_PIT_8253_CONTROL,(0 << 6) | (3 << 4) | (0 << 1));	/* timer 0, low/high, mode 0 */
	io_outbi(IO_PIT_8253_TIMER0,0xFF);
	io_outbi(IO_PIT_8253_TIMER0,0xFF);			/* tick at 18.2Hz */
}

uint16_t pit_8253_time() {
	io_outbi(IO_PIT_8253_CONTROL,(0 << 6) | (0 << 4) | (0 << 1));	/* timer 0, low/high, mode 0 */
	unsigned char lo = io_inbi(IO_PIT_8253_TIMER0);
	unsigned char hi = io_inbi(IO_PIT_8253_TIMER0);
	return ((uint16_t)lo) | (((uint16_t)hi) << 8);
}

