
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

#define IPv4_PROTO_ICMP		0x01

void net_on_ipv4_icmp(struct ethernet_frame_header *eth,unsigned char *src_ip,unsigned char *pkt,int len) {
	unsigned int i;
	uint32_t chksum = 0;
	uint16_t *pkt16 = (uint16_t*)pkt;
	if (len < 8) return;
	for (i=0;i < (len>>1);i++) {
		if (i != 1) {
			uint16_t wd = ntoh16(pkt16[i]);
			chksum += wd;
			chksum += chksum >> 16;
			chksum &= 0xFFFF;
		}
	}
	chksum = (~chksum) & 0xFFFF;
	uint16_t hdr_chksum = ntoh16(pkt16[1]);

	if (chksum != hdr_chksum)
		return;

	unsigned char type = pkt[0];
	unsigned char code = pkt[1];
	uint16_t id = *((uint16_t*)(pkt+4));
	uint16_t seq = *((uint16_t*)(pkt+6));

	if (type == 8 && code == 0) { /* ICMP echo request */
		vga_write("ICMP echo request, sending reply\r\n");

		struct ethernet_frame_header ret_eth;
		ret_eth.type = hton16(ETH_TYPE_IPv4);
		memcpy(ret_eth.dst_mac,eth->src_mac,6);
		memcpy(ret_eth.src_mac,my_eth_mac,6);

		unsigned int ipv4_len = 0x14+len;
		unsigned char *ret_pkt = chosen_net_drv->prepare_send_packet(0x14+len,&ret_eth);
		if (ret_pkt == NULL) {
			vga_write("ARP: cannot construct packet to answer\r\n");
			return;
		}

		memset(ret_pkt,0,0x14+len);
		ret_pkt[0] = 0x45;	/* IPv4 5 words */
		ret_pkt[2] = ipv4_len >> 8;
		ret_pkt[3] = ipv4_len;
		ret_pkt[6] = (2 << 5);	/* DF don't fragment */
		ret_pkt[8] = 0x40;	/* TTL */
		ret_pkt[9] = 0x01;	/* ICMP */
		memcpy(ret_pkt+12,my_ipv4_address,4);
		memcpy(ret_pkt+16,src_ip,4);

		chksum = 0;
		uint16_t *ret_pkt16 = (uint16_t*)ret_pkt;
		for (i=0;i < (20>>1);i++) {
			if (i != 5) {
				uint16_t wd = ntoh16(ret_pkt16[i]);
				chksum += wd;
				chksum += chksum >> 16;
				chksum &= 0xFFFF;
			}
		}
		uint16_t ret_hdr_chksum = ~chksum;
		ret_pkt[10] = ret_hdr_chksum >> 8;
		ret_pkt[11] = ret_hdr_chksum;

		unsigned char *icmp = ret_pkt + 0x14;
		memcpy(icmp,pkt,len);
		icmp[0] = 0x00;	/* echo reply */

		chksum = 0;
		ret_pkt16 = (uint16_t*)(icmp);
		for (i=0;i < (len>>1);i++) {
			if (i != 1) {
				uint16_t wd = ntoh16(ret_pkt16[i]);
				chksum += wd;
				chksum += chksum >> 16;
				chksum &= 0xFFFF;
			}
		}
		ret_hdr_chksum = ~chksum;
		icmp[2] = ret_hdr_chksum >> 8;
		icmp[3] = ret_hdr_chksum;

		chosen_net_drv->send_packet();
	}
}

void net_on_ipv4(struct ethernet_frame_header *eth,unsigned char *pkt,int sz) {
	unsigned int i;
	if (sz < 20) return;
	if ((pkt[0]>>4) != 4) return;	/* make sure version == 4 */
	unsigned int hdr_len = (pkt[0]&0xF) * 4;
	if (hdr_len < 20) return;

	uint32_t chksum = 0;
	uint16_t *pkt16 = (uint16_t*)pkt;
	for (i=0;i < (hdr_len>>1);i++) {
		if (i != 5) {
			uint16_t wd = ntoh16(pkt16[i]);
			chksum += wd;
			chksum += chksum >> 16;
			chksum &= 0xFFFF;
		}
	}
	chksum = (~chksum) & 0xFFFF;
	uint16_t hdr_chksum = ntoh16(pkt16[5]);

	if (chksum != hdr_chksum)
		return;

	unsigned int total_len = ntoh16(pkt16[1]);

	unsigned char flags = pkt[6] >> 5;
	uint16_t frag_offset = (((uint16_t)pkt[6]) & 0x1F) | (((uint16_t)pkt[7]) << 5); /* <- FIXME: is this right? */
	unsigned char proto = pkt[9];
	unsigned char *src_ip = pkt+12;
	unsigned char *dst_ip = pkt+16;

	if (memcmp(dst_ip,my_ipv4_address,4))
		return;

	if (flags & 1)	/* MF = more fragments. we don't do fragmented packets */
		return;	
	if (frag_offset != 0)	/* fragment offset implies fragmented packet */
		return;
	if (hdr_len > sz)
		return;
	if (hdr_len > total_len)
		return;
	if (total_len > sz)
		return;

	unsigned char *data = pkt + hdr_len;
	int data_len = total_len - hdr_len;
	if (data_len < 0)
		return;

	if (proto == IPv4_PROTO_ICMP)
		net_on_ipv4_icmp(eth,src_ip,data,data_len);
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

