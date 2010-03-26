/* APPARENTLY THIS DRIVER DOESN'T WORK WITH THE ACTUAL HARDWARE */

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

#include "network.h"
#include "net-rtl8139.h"

#if 0
static struct pci_bar*		rtl8139_io;
static unsigned int		rtl8139_iobase;
static unsigned char		rtl8139_mac_address[6];
static uint32_t*		rtl8139_rx=NULL;
static unsigned int		rtl8139_ring_size;
static unsigned int		rtl8139_rx_offset;
static unsigned int		rtl8139_send_sz;

#define PCNET32_RESET		0x14
#endif

static int rtl8139_known_pci_id(struct pci_device *p) {
#if 0
	if (p == NULL) return 0;

	if (p->vendor_id == 0x10EC) {
		if (p->device_id == 0x8139)
			return 1;
	}
#endif
	return 0;
}

static int rtl8139_probe(struct pci_device *pdev) {
	if (!rtl8139_known_pci_id(pdev)) return 0;
	return 1;
}

static int rtl8139_init(struct pci_device *pdev) {
#if 0
	unsigned int i,iobar=0;

	if (!rtl8139_known_pci_id(pdev)) return 0;

	rtl8139_io = NULL;
	/* scan for resources */
	for (i=0;i < 6;i++) {
		struct pci_bar *b = &(pdev->BAR[i]);
		if (!b->flags.present) continue;

		if (b->flags.io && b->end >= (b->start+0xFF)) {
			if (rtl8139_io == NULL) {
				rtl8139_io = b;
				iobar = i;
			}
		}
	}

	if (rtl8139_io == NULL) {
		vga_write("Cannot locate I/O resource\r\n");
		return 0;
	}

	/* TODO: don't just assume memory and I/O is enabled, turn it on */

	/* enable I/O */
	pci_config_write_imm(pdev->bus,pdev->device,pdev->function,4,0x29F);

	rtl8139_iobase = (unsigned int)(rtl8139_io->start);

	io_outb(rtl8139_iobase+0x5B,'R');	/* 8139too.c in Linux does this, why? */

	if (io_inl(rtl8139_iobase+0x40) == 0xFFFFFFFF) {
		vga_write("RTL8139 is not responding\r\n");
		return 0;
	}

	io_outw(rtl8139_iobase+0x3C,0x0000);	/* clear interrupt mask register */

	io_outw(rtl8139_iobase+0xE0,0);		/* disable C+ mode */
	io_outw(rtl8139_iobase+0x50,0);

	/* reset */
	io_outb(rtl8139_iobase+0x37,0x10);		/* reset */

	{
		unsigned int patience = 200;
		while (io_inb(rtl8139_iobase+0x37) & 0x10) {
			if (--patience != 0)
				return 0;	/* reset fail */
		}
	}

	io_outw(rtl8139_iobase+0x3E,0xFFFF);	/* reset ISR */
	io_outl(rtl8139_iobase+0x40,0x0000 | (3 << 24) | (6 << 8));	/* transmit configuration: standard gap, no loopback, CRC, max DMA=16 */
	io_outl(rtl8139_iobase+0x44,0x028F);	/* accept bcast+mcast+physmatch+physaddr ring size=8k no-wrap */

	for (i=0;i < 6;i++)
		rtl8139_mac_address[i] = io_inb(rtl8139_iobase+0x00+i);

	io_outl(rtl8139_iobase+0x08,0xFFFFFFFF);	/* multicast */
	io_outl(rtl8139_iobase+0x0C,0xFFFFFFFF);

	io_outw(rtl8139_iobase+0x62,0x1200);	/* auto-negotiate, re-start auto-negotiate */

	rtl8139_ring_size = 8192 + 16;
	align_alloc(4);
	if (rtl8139_rx == NULL) rtl8139_rx = do_alloc(rtl8139_ring_size + 1700);

	/* set up ring buffer */
	memset(rtl8139_rx,0,rtl8139_ring_size);

	/* FIXME: so let me get this straight: in "C" mode we follow a DWORD + packet around?
	 *        with little to no protection against, say, part of a packet? No need to set the "OWN" bit?
	 *        Ick... good thing they changed that on later chipsets */
	rtl8139_rx_offset = 0;

	memcpy(my_eth_mac,rtl8139_mac_address,6);

	/* tell the NIC where the RX and TX rings are */
	io_outl(rtl8139_iobase+0x30,(uint32_t)rtl8139_rx);
	io_outw(rtl8139_iobase+0x38,0xFFF0);
	io_outw(rtl8139_iobase+0x3A,0x0000);

	io_outw(rtl8139_iobase+0x3C,0x0000);
	io_outw(rtl8139_iobase+0x3E,0xFFFF);

	io_outb(rtl8139_iobase+0x37,0x0C);		/* enable RX TX */

	io_outl(rtl8139_iobase+0x4C,0);			

	io_outb(rtl8139_iobase+0x37,0x0C);		/* enable RX TX */

//	io_outl(rtl8139_iobase+0x20,(uint32_t)rtl8139_tx);

//	io_outl(rtl8139_iobase+0xEC,(1600+127)/128);	/* max packet size */
	return 1;
#else
	return 0;
#endif
}

static int rtl8139_uninit(struct pci_device *pdev) {
#if 0
	return 1;
#else
	return 0;
#endif
}

static unsigned char* rtl8139_prepare_send_packet(int sz,struct ethernet_frame_header *hdr) {
#if 0
	if (sz > (1600-14)) return NULL; /* for simplicity's sake we don't bother with packets larger than the MTU */
	struct rtl8139_tx_head *tx = rtl8139_tx + rtl8139_tx_offset;
	rtl8139_send_sz = (unsigned int)sz;

	{
		unsigned int patience = 3000000;
		while ((tx->d1&0x80000000) != 0) {
			if (--patience == 0) {
				vga_write("Timeout waiting for transmit ring to free up fragment\r\n");
				break;
			}
		}
	}

	unsigned char *p = rtl8139_tx_packets[rtl8139_tx_offset];
	*((struct ethernet_frame_header*)(p)) = *hdr;
	return p + 14;
#endif
	return NULL;
}

static void rtl8139_send_packet() {
#if 0
	struct rtl8139_tx_head *tx = rtl8139_tx + rtl8139_tx_offset;
	uint32_t d1 = 0x30000000 | 0x80000000 | (rtl8139_send_sz + 14);	/* first+last owned */

	if (rtl8139_tx_offset == (rtl8139_ring_size-1) || 1)
		d1 |= 0x40000000;			/* end of ring */

	tx->d2 = 0;
	tx->addr = (uint64_t)((size_t)rtl8139_tx_packets[rtl8139_tx_offset]);
	tx->d1 = d1;

	/* hey! packet ready! */
	io_outb(rtl8139_iobase+0x38,0x40);	/* NPQ=1 */

	{
		unsigned int patience = 300000000;
		while ((tx->d1&0x80000000) != 0) {
			if (--patience == 0) {
				vga_write("Timeout waiting for transmit ring to actually send packet\r\n");
				break;
			}
		}
	}

	if (++rtl8139_tx_offset >= rtl8139_ring_size || 1)
		rtl8139_tx_offset = 0;
#endif
}

static unsigned char* rtl8139_get_packet(int *sz,struct ethernet_frame_header *hdr) {
#if 0
	/* if it says buffer is not empty... then... */
	if (io_inb(rtl8139_iobase+0x37) & 1)
		return NULL;

	uint32_t status = *((uint32_t*)(rtl8139_rx + rtl8139_rx_offset));
	vga_write_hex(status);
	vga_write("\r\n");

#if 0
	struct rtl8139_rx_head *rx = rtl8139_rx + rtl8139_rx_offset;
	if ((rx->d1&0x80000000) == 0) { /* do we own it now? */
		/* discard descriptor on any error condition */
		if ((rx->d1&(1<<21)) != 0) { /* error? */
			rtl8139_rx_reinit(rx,rtl8139_rx_offset);
			rtl8139_adv_packet();
			return NULL;
		}

		if ((rx->d1&(1<<29)) && (rx->d1&(1<<28))) { /* First + Last descriptor (a complete packet) */
			unsigned int frame_length = rx->d1 & 0x3FFF;

			if (frame_length >= 14) {
				unsigned char *p = rtl8139_rx_packets[rtl8139_rx_offset];
				*sz = frame_length - 14;
				*hdr = *((struct ethernet_frame_header*)p);
				return p+14;
			}
			else {
				return NULL;
			}
		}
		else if (!(rx->d1&(1<<29))) { /* if this isn't the start of a packet then discard and move on */
			rtl8139_rx_reinit(rx,rtl8139_rx_offset);
			rtl8139_adv_packet();
			return NULL;
		}

		/* neither of the above are true, so STP and !ENP */
		/* for simplicity's sake, we just ignore packets that are too large for one descriptor. it's a lot
		 * simpler that way */
		rtl8139_rx_reinit(rx,rtl8139_rx_offset);
		rtl8139_adv_packet();
	}
#endif
#endif
	return NULL;
}

static void rtl8139_put_packet(unsigned char *p) {
}

struct network_driver rtl8139 = {
	.name =			"RealTek 8139",
	.probe =		rtl8139_probe,
	.init =			rtl8139_init,
	.uninit =		rtl8139_uninit,
	.get_packet =		rtl8139_get_packet,
	.prepare_send_packet =	rtl8139_prepare_send_packet,
	.send_packet =		rtl8139_send_packet,
	.put_packet =		rtl8139_put_packet
};

