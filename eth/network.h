
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

extern struct network_driver		*net_drivers[];

extern unsigned char			my_ipv4_address[4];
extern unsigned char			my_eth_mac[6];

extern struct pci_device		*chosen_net_dev;
extern struct network_driver		*chosen_net_drv;
extern unsigned char			chosen_net_drv_open;

