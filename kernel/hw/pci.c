/*
 * pci.c -- finding out what is plugged in.
 */
#include <eb/pci.h>
#include <eb/io.h>
#include <eb/fmt.h>
#include <eb/trap.h>
#include <eb/apic.h>
#include <eb/vmm.h>
#include <eb/mm.h>

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

/* ------------------------------------------------------------------ */
/* Interrupts                                                          */
/* ------------------------------------------------------------------ */

u8 pci_find_capability(const pci_device *d, u8 id)
{
    u16 status = pci_read16(d, 0x06);
    if (!(status & (1u << 4))) return 0;           /* no list at all */
    u8 off = (u8)(pci_read32(d, 0x34) & 0xFC);
    for (u32 guard = 0; off >= 0x40 && guard < 48; guard++) {
        u32 head = pci_read32(d, off);
        if ((head & 0xFF) == id) return off;
        off = (u8)((head >> 8) & 0xFC);
    }
    return 0;
}

/* The plain message capability: one message, at the vector given. */
static bool msi_enable(const pci_device *d, u8 cap, u8 vector)
{
    u32 head = pci_read32(d, cap);
    u16 ctl = (u16)(head >> 16);
    bool wide = (ctl & (1u << 7)) != 0;            /* 64-bit address */
    bool maskable = (ctl & (1u << 8)) != 0;

    pci_write32(d, (u8)(cap + 4), lapic_msi_address());
    if (wide) {
        pci_write32(d, (u8)(cap + 8), 0);
        pci_write16(d, (u8)(cap + 12), (u16)lapic_msi_data(vector));
        if (maskable) pci_write32(d, (u8)(cap + 16), 0);
    } else {
        pci_write16(d, (u8)(cap + 8), (u16)lapic_msi_data(vector));
        if (maskable) pci_write32(d, (u8)(cap + 12), 0);
    }
    /* one message (multiple message enable = 0), and on */
    ctl = (u16)((ctl & ~(7u << 4)) | 1u);
    pci_write16(d, (u8)(cap + 2), ctl);
    return true;
}

/* The table form: entry zero of the table in one of the device's
 * windows, everything else left masked as it came up. */
static bool msix_enable(const pci_device *d, u8 cap, u8 vector)
{
    u32 where = pci_read32(d, (u8)(cap + 4));
    u32 bir = where & 7;
    u64 offset = where & ~7u;
    u64 base = pci_bar(d, bir);
    if (!base || bir > 5) return false;
    phys_addr pa = base + offset;
    phys_addr page = pa & ~(PAGE_SIZE - 1);
    if (!vmm_map(vmm_kernel_pml4(), (virt_addr)phys_to_virt(page), page, 2 * PAGE_SIZE, PAGE_KERNEL_MMIO))
        return false;
    volatile u32 *entry = (volatile u32 *)phys_to_virt(pa);
    entry[0] = lapic_msi_address();
    entry[1] = 0;
    entry[2] = lapic_msi_data(vector);
    entry[3] = 0;                                   /* unmasked */
    u16 ctl = pci_read16(d, (u8)(cap + 2));
    ctl = (u16)((ctl | (1u << 15)) & ~(1u << 14));  /* enabled, function not masked */
    pci_write16(d, (u8)(cap + 2), ctl);
    return true;
}

static void set_intx(const pci_device *d, bool disabled)
{
    u16 cmd = pci_read16(d, 0x04);
    if (disabled) cmd |= (1u << 10); else cmd &= (u16)~(1u << 10);
    pci_write16(d, 0x04, cmd);
}

pci_irq pci_attach_irq(const pci_device *d, irq_handler fn, bool msix_ok)
{
    pci_irq r = { PCI_IRQ_NONE, 0 };
    if (!d || !fn) return r;

    if (lapic_present()) {
        u8 msi = pci_find_capability(d, 0x05);
        u8 msix = msix_ok ? pci_find_capability(d, 0x11) : 0;
        if (msi || msix) {
            i32 v = irq_alloc_vector(fn);
            if (v >= 0) {
                bool ok = msi ? msi_enable(d, msi, (u8)v) : msix_enable(d, msix, (u8)v);
                if (ok) {
                    set_intx(d, true);
                    r.kind = msi ? PCI_IRQ_MSI : PCI_IRQ_MSIX;
                    r.number = (u8)v;
                    return r;
                }
            }
        }
    }

    /* The pin, as the firmware routed it onto the legacy pair. Zero or
     * all ones means it wrote nothing there; line 2 is the cascade. */
    u8 line = (u8)(pci_read32(d, 0x3C) & 0xFF);
    u8 pin = (u8)((pci_read32(d, 0x3C) >> 8) & 0xFF);
    if (pin == 0 || line == 0 || line == 2 || line >= 16) return r;
    set_intx(d, false);
    irq_install(line, fn);
    r.kind = PCI_IRQ_LINE;
    r.number = line;
    return r;
}

const char *pci_irq_words(u8 kind)
{
    switch (kind) {
    case PCI_IRQ_MSI:  return "msi";
    case PCI_IRQ_MSIX: return "msi-x";
    case PCI_IRQ_LINE: return "line";
    default:           return "none";
    }
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
