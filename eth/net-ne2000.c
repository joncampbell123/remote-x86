
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
#include "net-ne2000.h"

static unsigned int		ne2000_iobase;
static unsigned char		ne2000_mac_address[6];

static unsigned int		ne2000_probe_base=0;

static unsigned char*		ne2000_last_packet;
static unsigned char*		ne2000_xmit_packet;
static unsigned int		ne2000_last_packet_size;
static unsigned int		ne2000_xmit_packet_size;

static int ne2000_isa_probe(unsigned int base) {
	unsigned char x;

	if (io_inb(base) == 0xFF)
		return 0;

	/* stop NIC */
	io_outb(base,0x21);	/* STOP and set register page 0 */

	x = io_inb(base);
	if ((x&0x3) != 1) return 0;
	if ((x&0xC0) != 0) return 0;

	/* OK look at the ISR. wait for reset to complete. */
	{
		unsigned int patience=0xC00;
		while (patience-- != 0 && (io_inb(base+7)&0x80) == 0);
		if ((io_inb(base+7)&0x80) == 0) return 0;
	}

	/* now START the NIC */
	io_outb(base,0x02);	/* START and set register page 0 */

	x = io_inb(base);
	if ((x&0x3) != 2) return 0;
	if ((x&0xC0) != 0) return 0;

	{
		unsigned int patience=0xC00;
		while (patience-- != 0 && (io_inb(base+7)&0x80) == 0x80);
		if ((io_inb(base+7)&0x80) == 0x80) return 0;
	}

	io_outb(base,0x21);
	vga_write("NE-2000 compatible at ");
	vga_write_hex((uint16_t)base);
	vga_write("\r\n");

	ne2000_probe_base = base;
	return 1;
}

static int ne2000_probe(struct pci_device *pdev) {
	if (pdev != NULL) {
		/* currently this code is written for the ISA variety */
		return 0;
	}

	if (ne2000_isa_probe(0x240))
		return 1;
	if (ne2000_isa_probe(0x260))
		return 1;
	if (ne2000_isa_probe(0x300))
		return 1;

	return 0;
}

/* in our implementation we reserve the lowest 2K of local NIC memory
 * for transmitting packets */
#define TX_PAGE			0x40
#define RX_PAGE_START		0x46
#define RX_PAGE_END		0x70

static int ne2000_init(struct pci_device *pdev) {
	unsigned char tmp[64];
	unsigned int i;

	if (ne2000_probe_base == 0 || pdev != NULL)
		return 0;

	ne2000_last_packet = do_alloc(2048);
	ne2000_xmit_packet = do_alloc(2049);

	io_outb(ne2000_probe_base+0x0,0x21);		/* abort transmit, STOP, page zero */
	io_outb(ne2000_probe_base+0xE,0xC5);		/* little endian, word-size, dual 16-bit mode for long words, FIFO triggers at 6 words */
	io_outb(ne2000_probe_base+0xA,0x00);		/* clear remote byte count register */
	io_outb(ne2000_probe_base+0xB,0x00);
	io_outb(ne2000_probe_base+0xC,0x1C);		/* receive broadcast, multicast, promisc. mode */
	io_outb(ne2000_probe_base+0xD,0x02);		/* loopback */

	io_outb(ne2000_probe_base+0x3,RX_PAGE_START);	/* boundary pointer */
	io_outb(ne2000_probe_base+0x1,RX_PAGE_START);	/* page start */
	io_outb(ne2000_probe_base+0x2,RX_PAGE_END);	/* page stop */
	io_outb(ne2000_probe_base+0x7,0xFF);		/* clear ISR */
	io_outb(ne2000_probe_base+0xF,0x00);		/* IMR: NO interrupts */

	/* This is not documented where I can find it, but the ne2.c driver in Linux implies that I read the MAC address from the bottom-most page? */
	io_outb(ne2000_probe_base+0xA,32);		/* read 32 bytes */
	io_outb(ne2000_probe_base+0xB,0);
	io_outb(ne2000_probe_base+0x8,0);		/* from 0x0000 */
	io_outb(ne2000_probe_base+0x9,0);

	for (i=0;i < 16;i++)
		tmp[i] = io_inb(ne2000_probe_base+0x10);
	for (i=0;i < 6;i++)
		ne2000_mac_address[i] = my_eth_mac[i] = tmp[i];

	io_outb(ne2000_probe_base+0x0,0x21|0x40);	/* STOP, page 1 */
	for (i=0;i < 6;i++)
		io_outb(ne2000_probe_base+0x1+i,ne2000_mac_address[i]);	/* your physical address (PAR0...PAR5) */
	for (i=0;i < 6;i++)
		io_outb(ne2000_probe_base+0x8+i,0xFF);	/* multicast address filter */
	io_outb(ne2000_probe_base+0x7,RX_PAGE_START);	/* current page */

	/* okay. start */
	io_outb(ne2000_probe_base+0x0,0x22);

	/* remove loopback mode */
	io_outb(ne2000_probe_base+0xD,0x00);

	return 1;
}

static int ne2000_uninit(struct pci_device *pdev) {
	return 1;
}

static void ne2000_adv_packet() {
}

static unsigned char* ne2000_prepare_send_packet(int sz,struct ethernet_frame_header *hdr) {
	/* wait for transmitter ready */
	io_outb(ne2000_probe_base+0,0x22);
	while (io_inb(ne2000_probe_base+0x0) & 4);

	if (sz >= 1520)
		return NULL;

	*((struct ethernet_frame_header*)ne2000_xmit_packet) = *hdr;
	ne2000_xmit_packet_size = sz + 14;
	return ne2000_xmit_packet + 14;
}

static void ne2000_send_packet() {
	unsigned int i;

	if (ne2000_xmit_packet_size != 0) {
		io_outb(ne2000_probe_base+0x4,TX_PAGE);
		io_outb(ne2000_probe_base+0x5,ne2000_xmit_packet_size);
		io_outb(ne2000_probe_base+0x6,ne2000_xmit_packet_size >> 8);
		io_outb(ne2000_probe_base+0x8,0);		/* from 0x0000 */
		io_outb(ne2000_probe_base+0x0,0x02|0x08);
		io_outb(ne2000_probe_base+0xA,ne2000_xmit_packet_size);		/* read 4 bytes */
		io_outb(ne2000_probe_base+0xB,ne2000_xmit_packet_size >> 8);
		io_outb(ne2000_probe_base+0x8,0);		/* from 0x0000 */
		io_outb(ne2000_probe_base+0x9,TX_PAGE);
		io_outb(ne2000_probe_base+0x0,0x02|0x10);	/* now write */
		for (i=0;i < (ne2000_xmit_packet_size>>1);i++)
			io_outw(ne2000_probe_base+0x10,((uint16_t*)ne2000_xmit_packet)[i]);
		if (ne2000_xmit_packet_size&1)
			io_outb(ne2000_probe_base+0x10,ne2000_xmit_packet[ne2000_xmit_packet_size-1]);

		/* now transmit */
		io_outb(ne2000_probe_base+0x0,0x22|0x04);

		/* wait for transmitter ready */
		io_outb(ne2000_probe_base+0,0x22);
		while (io_inb(ne2000_probe_base+0x0) & 4);

		ne2000_xmit_packet_size = 0;
	}
}

static unsigned char* ne2000_get_packet(int *sz,struct ethernet_frame_header *hdr) {
	unsigned char c,tmp[8];
	unsigned int i;
	
	ne2000_last_packet_size = 0;

	/* did we get anything? */
	io_outb(ne2000_probe_base+0x0,0x22);

	c = io_inb(ne2000_probe_base+7);
	if (c&0x40) {
		io_outb(ne2000_probe_base+7,0x40);
		c = io_inb(ne2000_probe_base+7);
	}

#if 0
	if (c) {
		vga_write_hex(c);
		vga_write("\r\n");
	}
#endif

	if (c&1) { /* we got a packet */
		io_outb(ne2000_probe_base+0,0x22|0x40);	/* goto page 1 */
		unsigned char cur_write = io_inb(ne2000_probe_base+7);	/* where the NIC is writing */
		io_outb(ne2000_probe_base+0,0x22); /* back to 0 */
		unsigned char cur_read = io_inb(ne2000_probe_base+3)+1;	/* where we are reading */
		if (cur_read >= RX_PAGE_END) cur_read = RX_PAGE_START;

		if (cur_read == cur_write)
			return NULL;

		/* read packet header */
		io_outb(ne2000_probe_base+0xA,4);		/* read 4 bytes */
		io_outb(ne2000_probe_base+0xB,0);
		io_outb(ne2000_probe_base+0x8,0);		/* from 0x0000 */
		io_outb(ne2000_probe_base+0x9,cur_read);
		for (i=0;i < (4>>1);i++)
			((uint16_t*)tmp)[i] = io_inw(ne2000_probe_base+0x10);

		/* 4 bytes:
		 * status (same as Receive status)
		 * next pointer
		 * byte count (big endian) */
		unsigned char next = tmp[1];
		unsigned char rstatus = tmp[0];
		unsigned int byte_count = ((unsigned int)(tmp[2])) | (((unsigned int)(tmp[3])) << 8);

		if ((rstatus & 1) && ((rstatus & 0x1E) == 0) && byte_count >= (14+4) && byte_count <= (1518+4)) {
			byte_count -= 4;	/* apparently count includes the header? */

			/* FIXME: what about buffer wrap-around? */
			io_outb(ne2000_probe_base+0xA,byte_count);
			io_outb(ne2000_probe_base+0xB,byte_count>>8);
			io_outb(ne2000_probe_base+0x8,4);
			io_outb(ne2000_probe_base+0x9,cur_read);

			for (i=0;i < (byte_count>>1);i++)
				((uint16_t*)ne2000_last_packet)[i] = io_inw(ne2000_probe_base+0x10);
			if (byte_count&1)
				ne2000_last_packet[byte_count-1] = io_inb(ne2000_probe_base+0x10);

			ne2000_last_packet_size = byte_count;
		}

		/* move to next, update boundary */
		io_outb(ne2000_probe_base+3,next-1);

		/* answer */
		io_outb(ne2000_probe_base+7,0x01);
	}

	io_outb(ne2000_probe_base+0x0,0x22);
	c = io_inb(ne2000_probe_base+7);
	if (c) io_outb(ne2000_probe_base+7,c&0xFE);

	if (ne2000_last_packet_size >= 14) {
		*hdr = *((struct ethernet_frame_header*)ne2000_last_packet);
		*sz = ne2000_last_packet_size - 14;
		return ne2000_last_packet + 14;
	}

	return NULL;
}

static void ne2000_put_packet(unsigned char *p) {
}

struct network_driver ne2000 = {
	.name =			"NE-2000 or compatible",
	.probe =		ne2000_probe,
	.init =			ne2000_init,
	.uninit =		ne2000_uninit,
	.get_packet =		ne2000_get_packet,
	.prepare_send_packet =	ne2000_prepare_send_packet,
	.send_packet =		ne2000_send_packet,
	.put_packet =		ne2000_put_packet
};

