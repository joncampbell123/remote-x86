
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
#include "net-pcnet32.h"
#include "net-rtl8139.h"

unsigned char			my_ipv4_address[4] = {192,168,250,254};
unsigned char			my_eth_mac[6];

struct pci_device		*chosen_net_dev = NULL;
struct network_driver		*chosen_net_drv = NULL;
unsigned char			chosen_net_drv_open = 0;

struct network_driver *net_drivers[] = {
	&pcnet32,				/* AMD PCnet Fast III network card */
	&rtl8139,				/* RealTek 8139/8101E */
	&ne2000,				/* your typical mid-to-late 1990's NE-2000 compatible */
	NULL
};

void autoprobe_network_card_isa() {
	unsigned int index,drvidx;

	chosen_net_drv=NULL;
	chosen_net_dev=NULL;

	/* okay, see how our various drivers act when asked to probe a non-PCI device */
	for (drvidx=0;net_drivers[drvidx] != NULL && chosen_net_drv == NULL;drvidx++) {
		if (net_drivers[drvidx]->probe(NULL)) {
			/* found one */
			chosen_net_drv = net_drivers[drvidx];
		}
	}
}

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

	if (chosen_net_dev == NULL) {
		vga_write("No PCI cards were automatically identified. Would you like me to probe for\r\n");
		vga_write("ISA network cards? \r\n");

		while (1) {
			int c = keyb8042_to_ascii(keyb8042_read_buffer());
			if (c == 'y') {
				autoprobe_network_card_isa();
				break;
			}
			else if (c > 0) {
				break;
			}
		}
	}
}

