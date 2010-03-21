
#ifndef PCI_H
#define PCI_H

struct pci_bar {
	struct {
		uint32_t	io:1;
		uint32_t	prefetchable:1;
		uint32_t	type:2;
		uint32_t	present:1;
	} flags;
	uint64_t		start;
	uint64_t		end;
	uint64_t		mask;
};

enum {
	PCI_BAR_TYPE_32BIT=0,
	PCI_BAR_TYPE_20BIT,
	PCI_BAR_TYPE_64BIT
};

struct pci_device {
	unsigned char		bus,device,function;
	uint16_t		device_id,vendor_id;
	uint32_t		class_code;
	uint8_t			revision_id;
	uint16_t		subsystem_id,subsystem_vendor_id;
	uint32_t		expansion_rom;
	struct pci_bar		BAR[6];
};

#define MAX_PCI_DEVICE			64

#define PCI_ANY				(~0)

struct pci_device *pci_find(uint16_t device_id,uint16_t vendor_id,uint16_t class_code);
void pci_addr(unsigned char bus,unsigned char dev,unsigned char func,unsigned char reg_dw);
uint32_t pci_config_read_imm(unsigned char bus,unsigned char dev,unsigned char func,unsigned char reg_dw);
void pci_config_write_imm(unsigned char bus,unsigned char dev,unsigned char func,unsigned char reg_dw,uint32_t data);
unsigned int pci_max_bus();
void pci_init();

extern struct pci_device			pci_device[MAX_PCI_DEVICE];
extern unsigned char				pci_bus_present;

#endif /* PCI_H */

