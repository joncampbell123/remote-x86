
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

struct pci_device			pci_device[MAX_PCI_DEVICE];
unsigned char				pci_bus_present=0;

#define PCI_CONFIG_ADDRESS		0xCF8
#define PCI_CONFIG_DATA			0xCFC

struct pci_device *pci_find(uint16_t device_id,uint16_t vendor_id,uint16_t class_code) {
	unsigned int idx=0;

	if (!pci_bus_present) return NULL;

	while (idx < MAX_PCI_DEVICE) {
		struct pci_device *dev = &pci_device[idx++];
		if (dev->device_id == 0) continue;
		if (device_id != PCI_ANY && device_id != dev->device_id) continue;
		if (vendor_id != PCI_ANY && vendor_id != dev->vendor_id) continue;
		if (class_code != PCI_ANY) {
			if ((class_code & 0xFF0000) != (dev->class_code & 0xFF0000)) continue;
			if ((class_code & 0x00FF00) != (dev->class_code & 0x00FF00)) continue;
			if ((class_code & 0x0000FF) != (dev->class_code & 0x0000FF)) continue;
		}
		return dev;
	}

	return NULL;
}

void pci_addr(unsigned char bus,unsigned char dev,unsigned char func,unsigned char reg_dw) {
	reg_dw &= 0xFC,func &= 7,dev &= 0x1F,bus &= 0xFF;
	io_outl(PCI_CONFIG_ADDRESS,
		0x80000000 |
		( ((unsigned int)bus) << 16 ) |
		( ((unsigned int)dev) << 11 ) |
		( ((unsigned int)func) << 8 ) |
		( ((unsigned int)reg_dw)    ));
}

uint32_t pci_config_read_imm(unsigned char bus,unsigned char dev,unsigned char func,unsigned char reg_dw) {
	pci_addr(bus,dev,func,reg_dw);
	return io_inl(PCI_CONFIG_DATA);
}

void pci_config_write_imm(unsigned char bus,unsigned char dev,unsigned char func,unsigned char reg_dw,uint32_t data) {
	pci_addr(bus,dev,func,reg_dw);
	io_outl(PCI_CONFIG_DATA,data);
}

/* probe around. but first see if and where the bus value wraps around
 * to avoid duplicate phantom PCI devices */
unsigned int pci_max_bus() {
	unsigned int bus,bus_max;
	uint32_t match_id;

	match_id = pci_config_read_imm(0,0,0,0);
	for (bus=1,bus_max=1;bus <= 0xFF;bus++) {
		uint32_t tmp = pci_config_read_imm(bus,0,0,0);
		if (tmp == match_id) break;
		bus_max = bus+1;
	}

	return bus_max;
}

void pci_init() {
	unsigned int x,bus,bus_max,index,dev,func;

	/* see if anything exists at the configuration I/O port */
	io_outl(PCI_CONFIG_ADDRESS,0x80000000);
	if ((io_inl(PCI_CONFIG_ADDRESS) & 0x80FFFFFC) != 0x80000000)
		return;

	io_outl(PCI_CONFIG_ADDRESS,0x80FFFFFC);
	if ((io_inl(PCI_CONFIG_ADDRESS) & 0x80FFFFFC) != 0x80FFFFFC)
		return;

	memset(pci_device,0,sizeof(struct pci_device) * MAX_PCI_DEVICE);
	vga_write("PCI bus detected\r\n");
	bus_max = pci_max_bus();
	pci_bus_present = 1;

	index=0;
	for (bus=0;bus < bus_max && index < MAX_PCI_DEVICE;bus++) {
		for (dev=0;dev < 0x20 && index < MAX_PCI_DEVICE;dev++) {
			for (func=0;func < 8 && index < MAX_PCI_DEVICE;func++) {
				uint32_t id = pci_config_read_imm(bus,dev,func,0);
				if (id == 0xFFFFFFFF) break;

				struct pci_device *pdev = &pci_device[index++];
				memset(pdev,0,sizeof(*pdev));
				pdev->bus = bus;
				pdev->device = dev;
				pdev->function = func;
				pdev->device_id = id >> 16;
				pdev->vendor_id = id & 0xFFFF;

				uint32_t x = pci_config_read_imm(bus,dev,func,8);
				pdev->class_code = x >> 8;
				pdev->revision_id = x & 0xFF;

				x = pci_config_read_imm(bus,dev,func,0x2C);
				pdev->subsystem_id = x >> 16;
				pdev->subsystem_vendor_id = x & 0xFFFF;

				x = pci_config_read_imm(bus,dev,func,0x30);
				pdev->expansion_rom = x & (~0x1FFFFF);

				unsigned int bar;
				for (bar=0;bar < 6;) {
					struct pci_bar *pbar = &(pdev->BAR[bar]);
					unsigned int reg = 0x10 + (bar * 4);
					uint32_t orig = pci_config_read_imm(bus,dev,func,reg);
					pci_config_write_imm(bus,dev,func,reg,0x00000000);
					uint32_t min = pci_config_read_imm(bus,dev,func,reg);
					pci_config_write_imm(bus,dev,func,reg,0xFFFFFFFF);
					uint32_t max = pci_config_read_imm(bus,dev,func,reg);
					pci_config_write_imm(bus,dev,func,reg,orig);
					bar++;

					if (orig == min && min == max && (orig == 0 || orig == ~0))
						continue;

					pbar->flags.present = 1;

					if (max & 1) {
						orig &= ~0x3,max &= ~0x3;
						pbar->flags.io = 1;
					}
					else {
						pbar->flags.prefetchable = (max >> 3) & 1;
						pbar->flags.type = (max >> 1) & 3;
						orig &= ~0xF,max &= ~0xF;
					}

					pbar->mask = 0xFFFFFFFF;
					pbar->start = orig;
					if (pbar->flags.type == PCI_BAR_TYPE_64BIT && bar < 6) {
						uint32_t upper = pci_config_read_imm(bus,dev,func,reg+4);
						pbar->start |= ((uint64_t)upper) << 32ULL;
						bar++;
					}

					pbar->end = orig | (~max);
				}
			}
		}
	}
}

