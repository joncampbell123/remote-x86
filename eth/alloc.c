
#include "stage2eth-base.h"
#include "interrupt.h"
#include "datatypes.h"
#include "keyb8042.h"
#include "pic-8259.h"
#include "pit-8253.h"
#include "ioport.h"
#include "alloc.h"
#include "bios.h"
#include "vga.h"
#include "pci.h"

extern char		last_byte;
unsigned char		*alloc;

void *do_alloc(size_t n) {
	void *p = (void*)alloc;
	alloc += n;
	return p;
}

void init_alloc() {
	alloc = (unsigned char*)((&last_byte) + 0x20000); /* <- FIXME why is this bias needed? */
}

void align_alloc(unsigned int sz) {
	size_t t = ((size_t)alloc + sz - 1) & ~(sz - 1);;
	alloc = (unsigned char*)t;
}

