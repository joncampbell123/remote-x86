
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
#include "net-rtl8101.h"

struct rtl8101_rx_head {
	volatile uint32_t	d1;		/* OWN EOR buffer size */
	volatile uint32_t	d2;		/* misc */
	volatile uint64_t	addr;
} __attribute__((packed));

struct rtl8101_tx_head {
	volatile uint32_t	d1;		/* OWN EOR misc frame length */
	volatile uint32_t	d2;		/* misc */
	volatile uint64_t	addr;
} __attribute__((packed));

static struct pci_bar*		rtl8101_io;
static unsigned int		rtl8101_iobase;
static unsigned char		rtl8101_mac_address[6];
static struct rtl8101_rx_head	*rtl8101_rx=NULL;
static unsigned char		**rtl8101_rx_packets;
static struct rtl8101_tx_head	*rtl8101_tx=NULL;
static unsigned char		**rtl8101_tx_packets;
static unsigned int		rtl8101_ring_size;
static unsigned int		rtl8101_rx_offset;
static unsigned int		rtl8101_tx_offset;
static unsigned int		rtl8101_send_sz;

#define PCNET32_RESET		0x14

static int rtl8101_known_pci_id(struct pci_device *p) {
	if (p == NULL) return 0;

	if (p->vendor_id == 0x10EC) {
		if (p->device_id == 0x8136)
			return 1;
	}

	return 0;
}

static int rtl8101_probe(struct pci_device *pdev) {
	if (!rtl8101_known_pci_id(pdev)) return 0;
	return 1;
}

static int rtl8101_init(struct pci_device *pdev) {
	unsigned int i,iobar=0;

	if (!rtl8101_known_pci_id(pdev)) return 0;

	rtl8101_io = NULL;
	/* scan for resources */
	for (i=0;i < 6;i++) {
		struct pci_bar *b = &(pdev->BAR[i]);
		if (!b->flags.present) continue;

		if (b->flags.io && b->end >= (b->start+0xFF)) {
			if (rtl8101_io == NULL) {
				rtl8101_io = b;
				iobar = i;
			}
		}
	}

	if (rtl8101_io == NULL) {
		vga_write("Cannot locate I/O resource\r\n");
		return 0;
	}

	/* TODO: don't just assume memory and I/O is enabled, turn it on */

	/* enable I/O */
	pci_config_write_imm(pdev->bus,pdev->device,pdev->function,4,0x29F);

	rtl8101_iobase = (unsigned int)(rtl8101_io->start);

	io_outw(rtl8101_iobase+0x3C,0x0000);	/* clear interrupt mask register */

	/* reset */
	io_outb(rtl8101_iobase+0x37,0x10);		/* reset */

	{
		unsigned int patience = 200;
		while (io_inb(rtl8101_iobase+0x37) & 0x10) {
			if (--patience != 0)
				return 0;	/* reset fail */
		}
	}

	io_outw(rtl8101_iobase+0x3E,0xFFFF);	/* reset ISR */
	io_outl(rtl8101_iobase+0x40,0x0000 | (3 << 24) | (6 << 8));	/* transmit configuration: standard gap, no loopback, CRC, max DMA=16 */
	io_outl(rtl8101_iobase+0x44,0x000F);	/* accept bcast+mcast+physmatch+physaddr */

	for (i=0;i < 6;i++)
		rtl8101_mac_address[i] = io_inb(rtl8101_iobase+0x00+i);

	io_outl(rtl8101_iobase+0x08,0xFFFFFFFF);	/* multicast */
	io_outl(rtl8101_iobase+0x0C,0xFFFFFFFF);

	io_outw(rtl8101_iobase+0x62,0x1200);	/* auto-negotiate, re-start auto-negotiate */

	rtl8101_ring_size = 4;
	align_alloc(256);
	if (rtl8101_rx == NULL) rtl8101_rx = do_alloc(rtl8101_ring_size*16);
	align_alloc(256);
	if (rtl8101_tx == NULL) rtl8101_tx = do_alloc(rtl8101_ring_size*16);
	align_alloc(16);
	if (rtl8101_rx_packets == NULL) rtl8101_rx_packets = do_alloc(4*sizeof(unsigned char*));
	if (rtl8101_tx_packets == NULL) rtl8101_tx_packets = do_alloc(4*sizeof(unsigned char*));

	/* set up ring buffer */
	memset(rtl8101_rx,0,rtl8101_ring_size*16);
	memset(rtl8101_tx,0,rtl8101_ring_size*16);

	memset(rtl8101_rx_packets,0,rtl8101_ring_size*sizeof(unsigned char*));
	memset(rtl8101_tx_packets,0,rtl8101_ring_size*sizeof(unsigned char*));

	/* prepare rx for receiving data */
	align_alloc(32);
	for (i=0;i < rtl8101_ring_size;i++)
		rtl8101_rx_packets[i] = do_alloc(1600);
	for (i=0;i < rtl8101_ring_size;i++)
		rtl8101_tx_packets[i] = do_alloc(1600);

	for (i=0;i < rtl8101_ring_size;i++) {
		rtl8101_rx[i].addr = (uint64_t)((size_t)rtl8101_rx_packets[i]);
		rtl8101_rx[i].d2 = 0x00000000;
		rtl8101_rx[i].d1 = 0x80000000 | (i == (rtl8101_ring_size - 1) ? 0x40000000 : 0) | 1600;	/* set EOR end-of-ring for desc 3, and NIC owns it now */
	}

	for (i=0;i < rtl8101_ring_size;i++) {
		rtl8101_tx[i].addr = (uint64_t)((size_t)rtl8101_tx_packets[i]);
		rtl8101_tx[i].d2 = 0x00000000;
		rtl8101_tx[i].d1 = 0x00000000 | (i == (rtl8101_ring_size - 1) ? 0x40000000 : 0) | 1600;	/* set EOR end-of-ring for desc 3, and I own it */
	}

	rtl8101_rx_offset = 0;
	rtl8101_tx_offset = 0;

	memcpy(my_eth_mac,rtl8101_mac_address,6);

	/* tell the NIC where the RX and TX rings are */
	io_outl(rtl8101_iobase+0xE8,0);
	io_outl(rtl8101_iobase+0xE4,(uint32_t)rtl8101_rx);
	io_outl(rtl8101_iobase+0x20,(uint32_t)rtl8101_tx);
	io_outl(rtl8101_iobase+0x24,0);
	io_outl(rtl8101_iobase+0x28,0);
	io_outl(rtl8101_iobase+0x2C,0);

	io_outl(rtl8101_iobase+0xEC,(1600+127)/128);	/* max packet size */

	io_outb(rtl8101_iobase+0x37,0x0C);		/* enable RX TX */
	return 1;
}

static int rtl8101_uninit(struct pci_device *pdev) {
	return 1;
}

static void rtl8101_rx_reinit(struct rtl8101_rx_head *rx,int entry) {
	rx->d2 = 0x00000000;
	rx->d1 &= 0x3FFFC000;
	rx->addr = (uint64_t)((size_t)rtl8101_rx_packets[entry]);
	/* FIX: is the stupid NIC resetting the "last descriptor" bit?? */
	if (entry == (rtl8101_ring_size-1)) rx->d1 |= 0x40000000;
	rx->d1 |= 0x80000000 | 1600;	/* set EOR end-of-ring for desc 3, and NIC owns it now */
}

static void rtl8101_adv_packet() {
	if (++rtl8101_rx_offset >= rtl8101_ring_size)
		rtl8101_rx_offset = 0;
}

static unsigned char* rtl8101_prepare_send_packet(int sz,struct ethernet_frame_header *hdr) {
	if (sz > (1600-14)) return NULL; /* for simplicity's sake we don't bother with packets larger than the MTU */
	struct rtl8101_tx_head *tx = rtl8101_tx + rtl8101_tx_offset;
	rtl8101_send_sz = (unsigned int)sz;

	{
		unsigned int patience = 3000000;
		while ((tx->d1&0x80000000) != 0) {
			if (--patience == 0) {
				vga_write("Timeout waiting for transmit ring to free up fragment\r\n");
				break;
			}
		}
	}

	unsigned char *p = rtl8101_tx_packets[rtl8101_tx_offset];
	*((struct ethernet_frame_header*)(p)) = *hdr;
	return p + 14;
}

static void rtl8101_send_packet() {
	struct rtl8101_tx_head *tx = rtl8101_tx + rtl8101_tx_offset;
	uint32_t d1 = 0x30000000 | 0x80000000 | (rtl8101_send_sz + 14);	/* first+last owned */

	if (rtl8101_tx_offset == (rtl8101_ring_size-1) || 1)
		d1 |= 0x40000000;			/* end of ring */

	tx->d2 = 0;
	tx->addr = (uint64_t)((size_t)rtl8101_tx_packets[rtl8101_tx_offset]);
	tx->d1 = d1;

	/* hey! packet ready! */
	io_outb(rtl8101_iobase+0x38,0x40);	/* NPQ=1 */

	{
		unsigned int patience = 300000000;
		while ((tx->d1&0x80000000) != 0) {
			if (--patience == 0) {
				vga_write("Timeout waiting for transmit ring to actually send packet\r\n");
				break;
			}
		}
	}

	if (++rtl8101_tx_offset >= rtl8101_ring_size || 1)
		rtl8101_tx_offset = 0;
}

static unsigned char* rtl8101_get_packet(int *sz,struct ethernet_frame_header *hdr) {
	struct rtl8101_rx_head *rx = rtl8101_rx + rtl8101_rx_offset;
	if ((rx->d1&0x80000000) == 0) { /* do we own it now? */
		/* discard descriptor on any error condition */
		if ((rx->d1&(1<<21)) != 0) { /* error? */
			rtl8101_rx_reinit(rx,rtl8101_rx_offset);
			rtl8101_adv_packet();
			return NULL;
		}

		if ((rx->d1&(1<<29)) && (rx->d1&(1<<28))) { /* First + Last descriptor (a complete packet) */
			unsigned int frame_length = rx->d1 & 0x3FFF;

			if (frame_length >= 14) {
				unsigned char *p = rtl8101_rx_packets[rtl8101_rx_offset];
				*sz = frame_length - 14;
				*hdr = *((struct ethernet_frame_header*)p);
				return p+14;
			}
			else {
				return NULL;
			}
		}
		else if (!(rx->d1&(1<<29))) { /* if this isn't the start of a packet then discard and move on */
			rtl8101_rx_reinit(rx,rtl8101_rx_offset);
			rtl8101_adv_packet();
			return NULL;
		}

		/* neither of the above are true, so STP and !ENP */
		/* for simplicity's sake, we just ignore packets that are too large for one descriptor. it's a lot
		 * simpler that way */
		rtl8101_rx_reinit(rx,rtl8101_rx_offset);
		rtl8101_adv_packet();
	}

	return NULL;
}

static void rtl8101_put_packet(unsigned char *p) {
	struct rtl8101_rx_head *rx = rtl8101_rx + rtl8101_rx_offset;
	rtl8101_rx_reinit(rx,rtl8101_rx_offset);
	rtl8101_adv_packet();
}

struct network_driver rtl8101 = {
	.name =			"RealTek 8101E",
	.probe =		rtl8101_probe,
	.init =			rtl8101_init,
	.uninit =		rtl8101_uninit,
	.get_packet =		rtl8101_get_packet,
	.prepare_send_packet =	rtl8101_prepare_send_packet,
	.send_packet =		rtl8101_send_packet,
	.put_packet =		rtl8101_put_packet
};

