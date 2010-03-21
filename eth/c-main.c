
#include "stage2eth-base.h"
#include "interrupt.h"
#include "datatypes.h"
#include "keyb8042.h"
#include "pic-8259.h"
#include "pit-8253.h"
#include "ioport.h"
#include "bios.h"
#include "vga.h"
#include "pci.h"

extern char		last_byte;
unsigned char		*alloc;

void *do_alloc(size_t n) {
	void *p = (void*)alloc;
//	vga_write_hex((uint32_t)p);
	alloc += n;
	return p;
}

void init_alloc() {
	alloc = (unsigned char*)((&last_byte) + 0x10000); /* <- FIXME why is this bias needed? */
}

void align_alloc(unsigned int sz) {
	size_t t = ((size_t)alloc + sz - 1) & ~(sz - 1);;
	alloc = (unsigned char*)t;
}

void timer_init() {
	pit_8253_init();
}

void keyboard_init() {
	keyb8042_init();
}

void interrupt_init() {
	idt_init();
	pic_8259_init();
}

char keyb8042_map[0x80] = {
	 -1, 27,'1','2', '3','4','5','6', '7','8','9','0', '-','=',  8,  9,
	'q','w','e','r', 't','y','u','i', 'o','p','[',']',  13, -1,'a','s',
	'd','f','g','h', 'j','k','l',';','\'','`', -1,'\\','z','x','c','v',
	'b','n','m',',', '.','/','*',-1,  ' ', -1, -1, -1,  -1, -1, -1, -1,
	 -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1,
	 -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1,
	 -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1,
	 -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1
};

int keyb8042_to_ascii(int r) {
	if (r < 0 || r >= 0x80) return -1;
	return (int)keyb8042_map[r];
}

void vga_clear() {
	unsigned int x,xmax = VGA_alpha_rows * VGA_alpha_columns;
	for (x=0;x < xmax;x++) VGA_alpha[x] = 0x0720;
}

void vga_home() {
	VGA_alpha_x = VGA_alpha_y = 0;
}

void vga_color(unsigned char c) {
	VGA_color = c;
}

void vga_normal() {
	VGA_color = 0x7;
}

void pci_bus_menu() {
	unsigned int y,idx,brk=0,redraw=1,select=0,scroll=0,x,lines = VGA_alpha_rows - 8;
	unsigned char tmp8;
	uint16_t tmp16;
	uint32_t tmp32;
	int c;

	vga_clear();
	vga_home();
	vga_color(0x0E);
	vga_write("PCI devices present\r\n");
	vga_normal();

	while (!brk) {
		if (redraw) {
			redraw = 0;
			for (y=0;y < lines;y++) {
				VGA_alpha_x = 0;
				VGA_alpha_y = y+1;
				idx = y+scroll;
				if (idx == select) vga_color(0x70);
				else vga_normal();

				if (idx >= MAX_PCI_DEVICE || pci_device[idx].device_id == 0) {
					for (x=0;x < 64;x++) vga_writechar(' ');
				}
				else {
					struct pci_device *dev = &pci_device[idx];
					tmp8 = dev->bus;         vga_write_hex(tmp8);   vga_writechar(':');
					tmp8 = dev->device;      vga_write_hex(tmp8);   vga_writechar('.');
					tmp8 = dev->function;    vga_write_hex(tmp8);   vga_write("  ");

					tmp16 = dev->device_id;  vga_write_hex(tmp16);  vga_write(",");
					tmp16 = dev->vendor_id;  vga_write_hex(tmp16);  vga_write(",");
					tmp8 = dev->class_code >> 16; vga_write_hex(tmp8);
					tmp16 = dev->class_code; vga_write_hex(tmp16);  vga_write(" ");
				}
			}
		}

		int r = keyb8042_read_buffer();
		if (r == 1) brk=1;
		int a = keyb8042_to_ascii(r);

		if (r == 0x48) {
			if (select > 0) {
				if (--select < scroll) scroll = select;
				redraw = 1;
			}
		}
		else if (r == 0x50) {
			if (++select > (scroll+lines-1)) scroll++;
			redraw = 1;
		}
		else if (a == 13) {
			vga_clear();
			vga_home();
			vga_normal();

			if (select >= MAX_PCI_DEVICE || pci_device[select].device_id == 0) {
				for (x=0;x < 64;x++) vga_writechar(' ');
			}
			else {
				struct pci_device *dev = &pci_device[select];

				vga_write("BARs\r\n");

				if (dev->expansion_rom != 0) {
					vga_write("Expansion ROM: ");

					tmp32 = (uint32_t)(dev->expansion_rom);          vga_write_hex(tmp32);

					vga_write("\r\n");
				}

				for (y=0;y < 6;y++) {
					struct pci_bar *bar = &(dev->BAR[y]);
					if (!bar->flags.present) continue;

					tmp8 = y;
					vga_write_hex(tmp8);
					vga_write(": ");

					if (bar->flags.io)	vga_write("I/O ");
					else			vga_write("MEM ");

					tmp32 = (uint32_t)(bar->start >> 32ULL); vga_write_hex(tmp32);
					tmp32 = (uint32_t)(bar->start);          vga_write_hex(tmp32);
					vga_writechar('-');

					tmp32 = (uint32_t)(bar->end >> 32ULL); vga_write_hex(tmp32);
					tmp32 = (uint32_t)(bar->end);          vga_write_hex(tmp32);

					vga_write("\r\n");
				}
			}

			do {
				c = keyb8042_read_buffer();
			} while (!(c == 1 || c == 0x1C));
	
			vga_clear();
			redraw = 1;
		}
	}
}

struct ethernet_frame_header {
	unsigned char		dst_mac[6];
	unsigned char		src_mac[6];
	uint16_t		type;
} __attribute__((packed));

struct network_driver {
	const char		*name;
	int			(*probe)(struct pci_device *);
	int			(*init)(struct pci_device *);
	int			(*uninit)(struct pci_device *);
	unsigned char*		(*get_packet)(int *sz,struct ethernet_frame_header *hdr);
	unsigned char*		(*prepare_send_packet)(int sz,struct ethernet_frame_header *hdr);
	void			(*send_packet)();
	void			(*put_packet)(unsigned char *p);
};

unsigned char			my_ipv4_address[4] = {192,168,1,254};
unsigned char			my_eth_mac[6];

struct pci_device		*chosen_net_dev = NULL;
struct network_driver		*chosen_net_drv = NULL;
unsigned char			chosen_net_drv_open = 0;




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
	if (p->vendor_id == 0x1022) {
		if (p->device_id == 0x2000)
			return 1;
	}

	return 0;
}

int pcnet32_probe(struct pci_device *pdev) {
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

int pcnet32_init(struct pci_device *pdev) {
	unsigned int i;

	if (!pcnet32_known_pci_id(pdev)) return 0;

	pcnet32_io = NULL;
	/* scan for resources */
	for (i=0;i < 6;i++) {
		struct pci_bar *b = &(pdev->BAR[i]);
		if (!b->flags.present) continue;

		if (b->flags.io && b->end >= (b->start+0x1F)) {
			if (pcnet32_io == NULL)
				pcnet32_io = b;
		}
	}

	if (pcnet32_io == NULL) {
		vga_write("Cannot locate I/O resource\r\n");
		return 0;
	}

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

int pcnet32_uninit(struct pci_device *pdev) {
	return 1;
}

static void pcnet32_rx_reinit(struct pcnet32_rx_head *rx) {
	rx->buf_length = -1600;
	rx->msg_length = 0;
	rx->reserved = 0;
	rx->status = 0x8000; /* give the ownership to the chipset */
}

void pcnet32_adv_packet() {
	if (++pcnet32_rx_offset >= pcnet32_ring_size)
		pcnet32_rx_offset = 0;
}

unsigned char* pcnet32_prepare_send_packet(int sz,struct ethernet_frame_header *hdr) {
	if (sz > (1600-14)) return NULL; /* for simplicity's sake we don't bother with packets larger than the MTU */

	struct pcnet32_tx_head *tx = pcnet32_tx + pcnet32_tx_offset;
	while ((tx->status&0x8000) != 0); /* wait until we own it */

	tx->status = 0;
	tx->length = -(sz + 14);
	unsigned char *p = pcnet32_tx_packets[pcnet32_tx_offset];
	*((struct ethernet_frame_header*)(p)) = *hdr;
	return p + 14;
}

void pcnet32_send_packet() {
	struct pcnet32_tx_head *tx = pcnet32_tx + pcnet32_tx_offset;
	tx->status = 0x8000 | (1<<(24-16)) | (1<<(25-16));	/* OWN, STP, ENP one whole packet */

	/* for sync operation wait until it's sent */
	while (tx->status&0x8000);

	if (++pcnet32_tx_offset >= pcnet32_ring_size)
		pcnet32_tx_offset = 0;
}

unsigned char* pcnet32_get_packet(int *sz,struct ethernet_frame_header *hdr) {
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

void pcnet32_put_packet(unsigned char *p) {
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




struct network_driver *net_drivers[] = {
	&pcnet32,
	NULL
};


void autoprobe_network_card() {
	struct pci_device *pdev;
	unsigned int index,drvidx;

	chosen_net_drv=NULL;
	chosen_net_dev=NULL;
	for (index=0;index < MAX_PCI_DEVICE && chosen_net_drv == NULL;index++) {
		pdev = &pci_device[index];
		if (pdev->device_id == 0) continue;

		/* must be ethernet class device */
		if ((pdev->class_code>>16) != 2) continue;

		/* okay, see how our various drivers like it */
		for (drvidx=0;net_drivers[drvidx] != NULL && chosen_net_drv == NULL;drvidx++) {
			if (net_drivers[drvidx]->probe(pdev)) {
				/* found one */
				chosen_net_dev = pdev;
				chosen_net_drv = net_drivers[drvidx];
			}
		}
	}
}

int memcmp(const unsigned char *a,const unsigned char *b,size_t c) {
	while (c-- != 0) {
		int d = (int)(*a++) - (int)(*b++);
		if (d != 0) return d;
	}

	return 0;
}

static inline uint16_t ntoh16(uint16_t x) {
	return (x << 8) | (x >> 8);
}

static inline uint32_t ntoh32(uint32_t x) {
	x = (x >> 16) | (x << 16);
	x = ((x & 0xFF00FF00) >> 8) | ((x & 0x00FF00FF) << 8);
	return x;
}

#define hton16 ntoh16
#define hton32 ntoh32

#define ETH_TYPE_ARP		0x0806

struct arp_packet { /* WARNING: ARP fields are big-endian (network byte order) */
	uint16_t		hw_type;				/* +0 */
	uint16_t		proto_type;				/* +2 */
	uint8_t			hw_addr_len,proto_addr_len;		/* +4, +5 */
	uint16_t		operation;				/* +6 */
} __attribute__((packed));

#define ARP_TYPE_ETH		1
#define ARP_REQUEST		1
#define ARP_REPLY		2
#define ARP_TYPE_IPv4		0x0800
#define ETH_TYPE_IPv4		0x0800

/* handle ARP requests */
void net_on_arp_IPv4(struct ethernet_frame_header *eth,struct arp_packet *arp,unsigned char *pkt,int sz) {
	unsigned int i;

	if (arp->proto_addr_len != 4)
		return;

	/* we only want to answer ARP requests, most likely for clients asking "who is IP address so-so?" */
	if (ntoh16(arp->operation) != ARP_REQUEST)
		return;

	unsigned char *sender_mac = pkt + 8;
	unsigned char *sender_ip = pkt + 14;
	unsigned char *lookin_for_ip = pkt + 24;

	/* only respond to requests for MY ip address */
	if (memcmp(lookin_for_ip,my_ipv4_address,4))
		return;

	struct ethernet_frame_header ret_eth;
	ret_eth.type = hton16(ETH_TYPE_ARP);
	memcpy(ret_eth.dst_mac,eth->src_mac,6);
	memcpy(ret_eth.src_mac,my_eth_mac,6);

	unsigned char *ret_pkt = chosen_net_drv->prepare_send_packet(28,&ret_eth);
	if (ret_pkt == NULL) {
		vga_write("ARP: cannot construct packet to answer\r\n");
		return;
	}
	struct arp_packet *ret_arp = (struct arp_packet*)ret_pkt;
	ret_arp->hw_type = hton16(ARP_TYPE_ETH);
	ret_arp->proto_type = hton16(ARP_TYPE_IPv4);
	ret_arp->hw_addr_len = 6;
	ret_arp->proto_addr_len = 4;
	ret_arp->operation = hton16(ARP_REPLY);
	memcpy(ret_pkt+8,my_eth_mac,6);
	memcpy(ret_pkt+14,my_ipv4_address,4);
	memcpy(ret_pkt+18,sender_mac,6);
	memcpy(ret_pkt+24,sender_ip,4);
	/* +28 */
	chosen_net_drv->send_packet();
}

void net_on_arp(struct ethernet_frame_header *eth,unsigned char *pkt,int sz) {
	struct arp_packet *arp = (struct arp_packet*)pkt;

	/* ethernet only, please */
	if (ntoh16(arp->hw_type) != ARP_TYPE_ETH || arp->hw_addr_len != 6)
		return;

	if (ntoh16(arp->proto_type) == ARP_TYPE_IPv4)
		net_on_arp_IPv4(eth,arp,pkt,sz);
}

void net_on_ipv4(struct ethernet_frame_header *eth,unsigned char *pkt,int sz) {
	unsigned int i;
	if (sz < 20) return;
	if ((pkt[0]>>4) != 4) return;	/* make sure version == 4 */
	unsigned int hdr_len = (pkt[0]&0xF) * 4;
	if (hdr_len < 20) return;

	uint16_t chksum = 0;
	uint16_t *pkt16 = (uint16_t*)pkt;
	for (i=0;i < (hdr_len>>1);i++) {
		if (i != 5) {
			uint16_t wd = ntoh16(pkt16[i]);
			chksum += wd + (wd>>15);
		}
	}
	chksum = ~chksum;
	uint16_t hdr_chksum = ntoh16(pkt16[5]);

	unsigned int total_len = pkt16[1];

	unsigned char flags = pkt[6] >> 5;
	uint16_t frag_offset = (((uint16_t)pkt[6]) & 0x1F) | (((uint16_t)pkt[7]) << 5);
	unsigned char proto = pkt[9];
	unsigned char *src_ip = pkt+12;
	unsigned char *dst_ip = pkt+16;

	if (memcmp(dst_ip,my_ipv4_address,4))
		return;

	if (flags & 1)	/* MF = more fragments. we don't do fragmented packets */
		return;	
	if (frag_offset != 0)	/* fragment offset implies fragmented packet */
		return;

	vga_write_hex(chksum);
	vga_writechar('-');
	vga_write_hex(hdr_chksum);
	vga_writechar(' ');
	vga_write_hex(flags);
	vga_writechar(' ');

	vga_write_hex(proto);
	vga_write("\r\n");
}

void net_idle() {
	if (!chosen_net_drv_open)
			return;

	int sz = 0;
	struct ethernet_frame_header eth;
	unsigned char *p = chosen_net_drv->get_packet(&sz,&eth);
	if (p != NULL) {
		/* respond only to packets going to all, or to me (driver may have put the card into promisc. mode) */
		if (!memcmp(eth.dst_mac,"\xFF\xFF\xFF\xFF\xFF\xFF",6) || !memcmp(eth.dst_mac,my_eth_mac,6)) {
			/* handle ARP */
			if (ntoh16(eth.type) == ETH_TYPE_ARP) {
				net_on_arp(&eth,p,sz);
			}
			/* handle IPv4 */
			else if (ntoh16(eth.type) == ETH_TYPE_IPv4) {
				net_on_ipv4(&eth,p,sz);
			}
			else {
				vga_writechar('>');
				vga_write_hex(eth.dst_mac[0]);
				vga_write_hex(eth.dst_mac[1]);
				vga_write_hex(eth.dst_mac[2]);
				vga_write_hex(eth.dst_mac[3]);
				vga_write_hex(eth.dst_mac[4]);
				vga_write_hex(eth.dst_mac[5]);
				vga_writechar(':');

				unsigned char *src = (unsigned char*)(p+0);
				unsigned int i;

				for (i=0;i < 24 && i < sz;i++) vga_write_hex(*src++);
				vga_write("\r\n");
			}
		}

		chosen_net_drv->put_packet(p);
	}
}

void main_menu() {
	int brk=0,redraw=1;

	while (!brk) {
		if (redraw) {
//			vga_clear();
//			vga_home();
			if (chosen_net_dev) {
				uint8_t tmp8;
				tmp8 = chosen_net_dev->bus; vga_write_hex(tmp8); vga_writechar(':');
				tmp8 = chosen_net_dev->device; vga_write_hex(tmp8); vga_writechar(':');
				tmp8 = chosen_net_dev->function; vga_write_hex(tmp8);
				vga_write("\r\n");
			}
			if (chosen_net_drv) {
				vga_write(chosen_net_drv->name);
				if (chosen_net_drv_open) vga_write(" [ACTIVE]");
				vga_write("\r\n");
			}
			vga_write("\r\n");
			vga_write("Main menu:\r\n");
			vga_write(" P. Show PCI devices\r\n");
			vga_write(" x. Auto-probe network card\r\n");
			if (chosen_net_drv_open)	vga_write(" c. Close driver\r\n");
			else				vga_write(" o. Open driver\r\n");
			redraw=0;
		}

		int r = keyb8042_read_buffer_imm();
		int a = keyb8042_to_ascii(r);

		if (a == 'p') {
			if (!pci_bus_present) {
				vga_write(" ! PCI bus not present\r\n");
			}
			else {
				pci_bus_menu();
				redraw=1;
			}
		}
		else if (a == 'o') {
			if (!chosen_net_drv_open && chosen_net_drv != NULL && chosen_net_dev != NULL) {
				if (chosen_net_drv->init(chosen_net_dev)) {
					chosen_net_drv_open = 1;
					redraw = 1;
//					while (keyb8042_read_buffer() != 0x1C);
				}
				else {
					vga_write("Open failed\r\n");
					keyb8042_read_buffer();
				}
			}
		}
		else if (a == 'c') {
			if (chosen_net_drv_open && chosen_net_drv != NULL && chosen_net_dev != NULL) {
				chosen_net_drv->uninit(chosen_net_dev);
				chosen_net_drv_open = 0;
				redraw = 1;
			}
		}
		else if (a == 'x') {
			autoprobe_network_card();
			redraw = 1;
		}

		net_idle();
	}
}

void c_start() {
	init_alloc();
	vga_init();
	vga_write("Setting up interrupts\r\n");	interrupt_init();
	vga_write("Setting up timer\r\n");	timer_init();
	vga_write("Setting up keyboard\r\n");	keyboard_init();
	pci_init();
	sti();

	vga_write("Remote-x86 (C) 2009-2010 Jonathan Campbell\r\n");
	vga_write("THIS VERSION IS FOR DEBUGGING OVER ETHERNET. It is intended for use on machines\r\n");
	vga_write("where there is ethernet but no working RS-232 serial port (like most laptops).\r\n");
	vga_write("\r\n");

	main_menu();

	hang();
}

