#include <sys/types.h>
#include <sys/io.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

/* Linux compat */
#define io_outb(p,v)	outb(v,p)
#define io_outw(p,v)	outw(v,p)
#define io_outl(p,v)	outl(v,p)
#define io_inb(p)	inb(p)
#define io_inw(p)	inw(p)
#define io_inl(p)	inl(p)

#define DEBUG(x)	printf("%s\n",x);

static unsigned int rtl_base_io = 0x3000;
static unsigned char rtl_mac_address[6];

int main() {
	unsigned int i;

	if (iopl(3)) {
		fprintf(stderr,"Cannot enable I/O\n");
		return 1;
	}

	io_outw(rtl_base_io+0x3C,0x0000);	/* clear interrupt mask register */

	/* reset */
	DEBUG("Resetting NIC");
	io_outb(rtl_base_io+0x37,0x10);		/* reset */

	{
		unsigned int patience = 200;
		while (io_inb(rtl_base_io+0x37) & 0x10) {
			if (--patience != 0)
				return 1;	/* reset fail */
		}
	}

	io_outw(rtl_base_io+0x3E,0xFFFF);	/* reset ISR */
	io_outl(rtl_base_io+0x40,0x0000 | (3 << 24));	/* transmit configuration: standard gap, no loopback, CRC, max DMA=16 */
	io_outl(rtl_base_io+0x44,0x008F);	/* no-wrap, accept bcast+mcast+physmatch+physaddr */

	for (i=0;i < 6;i++)
		rtl_mac_address[i] = io_inb(rtl_base_io+0x00+i);

	io_outl(rtl_base_io+0x08,0xFFFFFFFF);	/* multicast */
	io_outl(rtl_base_io+0x0C,0xFFFFFFFF);

	io_outw(rtl_base_io+0x62,0x1200);	/* auto-negotiate, re-start auto-negotiate */

	io_outb(rtl_base_io+0x37,0x0C);		/* enable RX TX */

	return 0;
}

