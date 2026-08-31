#ifndef EB_PCI_H
#define EB_PCI_H

#include <eb/types.h>

/* PCI configuration space, through the two ports at 0xCF8 and 0xCFC.
 *
 * The memory-mapped route is faster and reaches the extended registers,
 * but it needs the address to be read out of an ACPI table first. The
 * port pair works on every machine that has PCI at all and needs
 * nothing parsed beforehand, which is the right trade while there is no
 * ACPI parser. */

typedef struct {
    u8  bus, device, function;
    u16 vendor, device_id;
    u8  class_code, subclass, prog_if;
    u8  header_type;
} pci_device;

void pci_scan(void);
u32  pci_device_count(void);
const pci_device *pci_get(u32 index);

/* Finds the first device matching a class, subclass and programming
 * interface. Returns NULL if there is none. */
const pci_device *pci_find(u8 class_code, u8 subclass, u8 prog_if);

u32  pci_read32(const pci_device *d, u8 offset);
void pci_write32(const pci_device *d, u8 offset, u32 value);
u16  pci_read16(const pci_device *d, u8 offset);
void pci_write16(const pci_device *d, u8 offset, u16 value);

/* Base address register, already stripped of its flag bits. */
u64  pci_bar(const pci_device *d, u32 index);

const char *pci_class_name(u8 class_code, u8 subclass);

#endif /* EB_PCI_H */
