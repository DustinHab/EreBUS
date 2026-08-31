/*
 * pci.c -- finding out what is plugged in.
 */
#include <eb/pci.h>
#include <eb/io.h>
#include <eb/fmt.h>

#define PCI_ADDRESS 0xCF8
#define PCI_DATA    0xCFC

#define MAX_DEVICES 64

static pci_device devices[MAX_DEVICES];
static u32 device_count;

static u32 config_read(u8 bus, u8 dev, u8 fn, u8 offset)
{
    /* Bit 31 enables the access; the offset must be aligned to four
     * bytes, which is why the bottom two bits are masked away. */
    u32 address = (1u << 31)
                | ((u32)bus << 16)
                | ((u32)(dev & 0x1F) << 11)
                | ((u32)(fn & 0x07) << 8)
                | (offset & 0xFC);

    outl(PCI_ADDRESS, address);
    return inl(PCI_DATA);
}

static void config_write(u8 bus, u8 dev, u8 fn, u8 offset, u32 value)
{
    u32 address = (1u << 31)
                | ((u32)bus << 16)
                | ((u32)(dev & 0x1F) << 11)
                | ((u32)(fn & 0x07) << 8)
                | (offset & 0xFC);

    outl(PCI_ADDRESS, address);
    outl(PCI_DATA, value);
}

u32 pci_read32(const pci_device *d, u8 offset)
{
    return config_read(d->bus, d->device, d->function, offset);
}

void pci_write32(const pci_device *d, u8 offset, u32 value)
{
    config_write(d->bus, d->device, d->function, offset, value);
}

u16 pci_read16(const pci_device *d, u8 offset)
{
    u32 v = pci_read32(d, offset & 0xFC);
    return (u16)((v >> ((offset & 2) * 8)) & 0xFFFF);
}

void pci_write16(const pci_device *d, u8 offset, u16 value)
{
    u32 v = pci_read32(d, offset & 0xFC);
    u32 shift = (offset & 2) * 8;
    v = (v & ~(0xFFFFu << shift)) | ((u32)value << shift);
    pci_write32(d, offset & 0xFC, v);
}

u64 pci_bar(const pci_device *d, u32 index)
{
    u8 offset = (u8)(0x10 + index * 4);
    u32 low = pci_read32(d, offset);

    if (low & 1) return low & ~0x3ULL;          /* an I/O port range */

    /* A memory range says in its own bits whether it is 64 bits wide,
     * in which case the next register holds the top half. */
    if (((low >> 1) & 3) == 2) {
        u64 high = pci_read32(d, (u8)(offset + 4));
        return ((high << 32) | (low & ~0xFULL));
    }
    return low & ~0xFULL;
}

/* ------------------------------------------------------------------ */

static void examine(u8 bus, u8 dev, u8 fn)
{
    u32 ident = config_read(bus, dev, fn, 0x00);
    u16 vendor = (u16)(ident & 0xFFFF);

    /* All ones means nothing answered. */
    if (vendor == 0xFFFF) return;
    if (device_count >= MAX_DEVICES) return;

    u32 classes = config_read(bus, dev, fn, 0x08);
    u32 header  = config_read(bus, dev, fn, 0x0C);

    pci_device *d = &devices[device_count++];
    d->bus = bus;
    d->device = dev;
    d->function = fn;
    d->vendor = vendor;
    d->device_id = (u16)(ident >> 16);
    d->prog_if = (u8)((classes >> 8) & 0xFF);
    d->subclass = (u8)((classes >> 16) & 0xFF);
    d->class_code = (u8)((classes >> 24) & 0xFF);
    d->header_type = (u8)((header >> 16) & 0xFF);
}

void pci_scan(void)
{
    device_count = 0;

    /* A plain sweep of every slot rather than following bridges. It is
     * slower -- 256 buses of 32 slots -- but it needs no assumptions
     * about the topology, and it happens once. */
    for (u32 bus = 0; bus < 256; bus++) {
        for (u32 dev = 0; dev < 32; dev++) {
            u32 ident = config_read((u8)bus, (u8)dev, 0, 0x00);
            if ((ident & 0xFFFF) == 0xFFFF) continue;

            examine((u8)bus, (u8)dev, 0);

            /* Bit 7 of the header type says the slot has more than one
             * function behind it. */
            u32 header = config_read((u8)bus, (u8)dev, 0, 0x0C);
            if (!((header >> 16) & 0x80)) continue;

            for (u32 fn = 1; fn < 8; fn++)
                examine((u8)bus, (u8)dev, (u8)fn);
        }
    }
}

u32 pci_device_count(void) { return device_count; }

const pci_device *pci_get(u32 index)
{
    return index < device_count ? &devices[index] : NULL;
}

const pci_device *pci_find(u8 class_code, u8 subclass, u8 prog_if)
{
    for (u32 i = 0; i < device_count; i++) {
        if (devices[i].class_code == class_code &&
            devices[i].subclass == subclass &&
            devices[i].prog_if == prog_if)
            return &devices[i];
    }
    return NULL;
}

const char *pci_class_name(u8 class_code, u8 subclass)
{
    switch (class_code) {
    case 0x01:
        switch (subclass) {
        case 0x01: return "ide controller";
        case 0x06: return "sata controller";
        case 0x08: return "nvme controller";
        default:   return "storage controller";
        }
    case 0x02: return "network controller";
    case 0x03: return "display controller";
    case 0x04: return "multimedia controller";
    case 0x06:
        switch (subclass) {
        case 0x00: return "host bridge";
        case 0x01: return "isa bridge";
        case 0x04: return "pci bridge";
        default:   return "bridge";
        }
    case 0x0C:
        switch (subclass) {
        case 0x03: return "usb controller";
        default:   return "serial bus controller";
        }
    default: return "device";
    }
}
