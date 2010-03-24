
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
#include "net-pcnet32.h"

struct pcnet32_rx_head {
	uint32_t		base;
	uint16_t		buf_length;
	volatile uint16_t	status;
	uint32_t		msg_length;
	uint32_t		reserved;
} __attribute__((packed));

struct pcnet32_tx_head {
	uint32_t		base;
	volatile uint16_t	length;
	volatile uint16_t	status;
	uint32_t		msg_length;
	uint32_t		reserved;
} __attribute__((packed));

static struct pci_bar*		pcnet32_io;
static unsigned int		pcnet32_iobase;
static unsigned char		pcnet32_mac_address[6];
static unsigned char		pcnet32_init_data[512];
static struct pcnet32_rx_head	*pcnet32_rx=NULL;
static unsigned char		**pcnet32_rx_packets;
static struct pcnet32_tx_head	*pcnet32_tx=NULL;
static unsigned char		**pcnet32_tx_packets;
static unsigned int		pcnet32_ring_size;
static unsigned int		pcnet32_rx_offset;
static unsigned int		pcnet32_tx_offset;

#define PCNET32_RESET		0x14

static int pcnet32_known_pci_id(struct pci_device *p) {
	if (p == NULL) return 0;

	if (p->vendor_id == 0x1022) {
		if (p->device_id == 0x2000)
			return 1;
	}

	return 0;
}

static int pcnet32_probe(struct pci_device *pdev) {
	if (!pcnet32_known_pci_id(pdev)) return 0;
	return 1;
}

static void pcnet32_rap(unsigned int addr) {
	io_outl(pcnet32_iobase + 0x14,addr); /* DWORD MODE */
}

static inline unsigned int pcnet32_read_csr() {
	return io_inl(pcnet32_iobase + 0x10);
}

static inline void pcnet32_write_csr(unsigned int x) {
	io_outl(pcnet32_iobase + 0x10,x);
}

static inline unsigned int pcnet32_read_bcr() {
	return io_inl(pcnet32_iobase + 0x1C);
}

static inline void pcnet32_write_bcr(unsigned int x) {
	io_outl(pcnet32_iobase + 0x1C,x);
}

static int pcnet32_init(struct pci_device *pdev) {
	unsigned int i,iobar=0;

	if (!pcnet32_known_pci_id(pdev)) return 0;

	pcnet32_io = NULL;
	/* scan for resources */
	for (i=0;i < 6;i++) {
		struct pci_bar *b = &(pdev->BAR[i]);
		if (!b->flags.present) continue;

		if (b->flags.io && b->end >= (b->start+0x1F)) {
			if (pcnet32_io == NULL) {
				pcnet32_io = b;
				iobar = i;
			}
		}
	}

	if (pcnet32_io == NULL) {
		vga_write("Cannot locate I/O resource\r\n");
		return 0;
	}

	/* TODO: don't just assume memory and I/O is enabled, turn it on */

	/* enable I/O */
	pci_config_write_imm(pdev->bus,pdev->device,pdev->function,4,0x29F);

	align_alloc(16);
	if (pcnet32_rx == NULL) pcnet32_rx = do_alloc(16*16);
	if (pcnet32_tx == NULL) pcnet32_tx = do_alloc(16*16);
	if (pcnet32_rx_packets == NULL) pcnet32_rx_packets = do_alloc(16*sizeof(unsigned char*));
	if (pcnet32_tx_packets == NULL) pcnet32_tx_packets = do_alloc(16*sizeof(unsigned char*));

	pcnet32_iobase = (unsigned int)(pcnet32_io->start);

	/* turn on dword mode */
	io_outl(pcnet32_iobase + 0x10,0x00000000);

	/* obtain the MAC address */
	for (i=0;i < 6;i++)
		pcnet32_mac_address[i] = io_inb(pcnet32_iobase + i);

	/* set SSIZE32 */
	pcnet32_rap(20);
	pcnet32_write_bcr(2);

	/* start INIT */
	{
		pcnet32_ring_size = 16;
		uint32_t *it = (uint32_t*)pcnet32_init_data;
		it[0] = (4 << 28) | (4 << 20) | 0x8000 | (3 << 7);		/* 1 << 4 = 16 read/transmit descriptors, promisc. mode, select MII */
		memcpy((char*)it + 4,pcnet32_mac_address,6);			/* MAC address */
		memset((char*)it + 10,0,2);
		memset((char*)it + 12,0,8);					/* no logical addrs */
		it[5] = (uint32_t)pcnet32_rx;
		it[6] = (uint32_t)pcnet32_tx;
	}

	/* set up ring buffer */
	memset(pcnet32_rx,0,16*16);
	memset(pcnet32_tx,0,16*16);

	memset(pcnet32_rx_packets,0,16*sizeof(unsigned char*));
	memset(pcnet32_tx_packets,0,16*sizeof(unsigned char*));

	/* prepare rx for receiving data */
	align_alloc(32);
	for (i=0;i < 16;i++)
		pcnet32_rx_packets[i] = do_alloc(1600);
	for (i=0;i < 16;i++)
		pcnet32_tx_packets[i] = do_alloc(1600);

	for (i=0;i < 16;i++) {
		pcnet32_rx[i].base = (uint32_t)pcnet32_rx_packets[i];
		pcnet32_rx[i].buf_length = -1600;
		pcnet32_rx[i].msg_length = 0;
		pcnet32_rx[i].reserved = 0;
		pcnet32_rx[i].status = 0x8000; /* give the ownership to the chipset */
	}

	for (i=0;i < 16;i++) {
		pcnet32_tx[i].base = (uint32_t)pcnet32_tx_packets[i];
		pcnet32_tx[i].length = -1;
		pcnet32_tx[i].status = 0;
		pcnet32_tx[i].msg_length = 0;
		pcnet32_tx[i].status = 0x0000;
	}

	uint32_t init_addr = (uint32_t)pcnet32_init_data;
	pcnet32_rap(2); pcnet32_write_csr(init_addr>>16);
	pcnet32_rap(1); pcnet32_write_csr(init_addr&0xFFFF);
	pcnet32_rap(0);	pcnet32_write_csr(3);	/* set INIT and START */

	pcnet32_rx_offset = 0;
	pcnet32_tx_offset = 0;

	memcpy(my_eth_mac,pcnet32_mac_address,6);
	return 1;
}

static int pcnet32_uninit(struct pci_device *pdev) {
	return 1;
}

static void pcnet32_rx_reinit(struct pcnet32_rx_head *rx) {
	rx->buf_length = -1600;
	rx->msg_length = 0;
	rx->reserved = 0;
	rx->status = 0x8000; /* give the ownership to the chipset */
}

static void pcnet32_adv_packet() {
	if (++pcnet32_rx_offset >= pcnet32_ring_size)
		pcnet32_rx_offset = 0;
}

static unsigned char* pcnet32_prepare_send_packet(int sz,struct ethernet_frame_header *hdr) {
	if (sz > (1600-14)) return NULL; /* for simplicity's sake we don't bother with packets larger than the MTU */

	struct pcnet32_tx_head *tx = pcnet32_tx + pcnet32_tx_offset;
	while ((tx->status&0x8000) != 0); /* wait until we own it */

	tx->status = 0;
	tx->length = -(sz + 14);
	unsigned char *p = pcnet32_tx_packets[pcnet32_tx_offset];
	*((struct ethernet_frame_header*)(p)) = *hdr;
	return p + 14;
}

static void pcnet32_send_packet() {
	struct pcnet32_tx_head *tx = pcnet32_tx + pcnet32_tx_offset;
	tx->status = 0x8000 | (1<<(24-16)) | (1<<(25-16));	/* OWN, STP, ENP one whole packet */

	/* for sync operation wait until it's sent */
//	while (tx->status&0x8000);

	if (++pcnet32_tx_offset >= pcnet32_ring_size)
		pcnet32_tx_offset = 0;
}

static unsigned char* pcnet32_get_packet(int *sz,struct ethernet_frame_header *hdr) {
	struct pcnet32_rx_head *rx = pcnet32_rx + pcnet32_rx_offset;
	if ((rx->status&0x8000) == 0) { /* do we own it now? */
		/* discard descriptor on any error condition */
		if ((rx->status&0x7C80) != 0) { /* ERR+FRAM+OFLO+CRC+BUFF+BPE */
			pcnet32_rx_reinit(rx);
			pcnet32_adv_packet();
			return NULL;
		}

		if ((rx->status&(1<<(25-16))) && (rx->status&(1<<(24-16)))) { /* STP and ENP I.e. a complete packet */
			if (rx->msg_length >= 14) {
				unsigned char *p = pcnet32_rx_packets[pcnet32_rx_offset];
				*sz = rx->msg_length - 14;
				*hdr = *((struct ethernet_frame_header*)p);
				return p+14;
			}
			else {
				return NULL;
			}
		}
		else if (!(rx->status&(1<<(25-16)))) { /* if this isn't the start of a packet then discard and move on */
			pcnet32_rx_reinit(rx);
			pcnet32_adv_packet();
			return NULL;
		}

		/* neither of the above are true, so STP and !ENP */
		/* for simplicity's sake, we just ignore packets that are too large for one descriptor. it's a lot
		 * simpler that way */
		pcnet32_rx_reinit(rx);
		pcnet32_adv_packet();
	}

	return NULL;
}

static void pcnet32_put_packet(unsigned char *p) {
	struct pcnet32_rx_head *rx = pcnet32_rx + pcnet32_rx_offset;
	pcnet32_rx_reinit(rx);
	pcnet32_adv_packet();
}

struct network_driver pcnet32 = {
	.name =			"PCnet-FAST",
	.probe =		pcnet32_probe,
	.init =			pcnet32_init,
	.uninit =		pcnet32_uninit,
	.get_packet =		pcnet32_get_packet,
	.prepare_send_packet =	pcnet32_prepare_send_packet,
	.send_packet =		pcnet32_send_packet,
	.put_packet =		pcnet32_put_packet
};

