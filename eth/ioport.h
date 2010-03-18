
#ifndef IOPORT_H
#define IOPORT_H

static inline void io_outb(unsigned int port,unsigned char b) {
	__asm__ __volatile__ ("outb	%%al,%%dx"
		: /* out */
		: "d" (port), "a" (b) /* in */ );
}

static inline void io_outbi(unsigned char port,unsigned char b) {
	__asm__ __volatile__ ("outb	%%al,%0"
		: /* out */
		: "i" (port), "a" (b) /* in */ );
}

static inline unsigned char io_inb(unsigned int port) {
	register unsigned char d;
	__asm__ __volatile__ ("inb	%%dx,%%al"
		: "=a" (d) /* out */
		: "d" (port) /* in */ );
	return d;
}

static inline unsigned char io_inbi(unsigned int port) {
	register unsigned char d;
	__asm__ __volatile__ ("inb	%1,%%al"
		: "=a" (d) /* out */
		: "i" (port) /* in */ );
	return d;
}

static inline void io_delay() {
	io_inbi(0x80);
	io_inbi(0x80);
	io_inbi(0x80);
	io_inbi(0x80);
}

static inline void io_outb_d(unsigned int port,unsigned char b) {
	io_outb(port,b);
	io_delay();
}

static inline void io_outbi_d(unsigned char port,unsigned char b) {
	io_outbi(port,b);
	io_delay();
}

static inline unsigned char io_inb_d(unsigned int port) {
	register unsigned char r = io_inb(port);
	io_delay();
	return r;
}

static inline unsigned char io_inbi_d(unsigned char port) {
	register unsigned char r = io_inbi(port);
	io_delay();
	return r;
}

static inline void cli() { __asm__ __volatile__ ("cli"); }
static inline void sti() { __asm__ __volatile__ ("sti"); }

#endif

