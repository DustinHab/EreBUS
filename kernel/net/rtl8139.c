/*
 * rtl8139.c -- Realtek's old warhorse, spoken to through io ports.
 *
 * Simpler than the e1000 by a generation: four fixed transmit slots
 * used in rotation, and one continuous receive buffer the card writes
 * packets into head to tail, each behind a little header saying how
 * long it is. The driver walks behind, reading and advancing a
 * pointer. Nothing here is fast and nothing needs to be.
 */
#include <eb/net.h>
#include <eb/pci.h>
#include <eb/pmm.h>
#include <eb/mm.h>
#include <eb/io.h>
#include <eb/fmt.h>

#define R_MAC     0x00
#define R_TSD0    0x10
#define R_TSAD0   0x20
#define R_RBSTART 0x30
#define R_CMD     0x37
#define R_CAPR    0x38
#define R_IMR     0x3C
#define R_ISR     0x3E
#define R_RCR     0x44
#define R_CONFIG1 0x52

#define CMD_RESET 0x10
#define CMD_RX_EN 0x08
#define CMD_TX_EN 0x04
#define CMD_BUFE  0x01              /* receive buffer empty */

/* Everyone, us, multicast, broadcast; wrap off the end into the
 * overhang rather than around. */
#define RCR_BITS  (0x0F | (1u << 7) | (7u << 8))

#define RX_SIZE   (8192 + 16 + 1536)

static u16  io;
static u8   mac[6];
static u8  *rx;
static u32  rx_off;
static u32  tx_slot;
static u8  *tx_buf[4];
static phys_addr tx_phys[4];

static const u8 *r8139_mac(void) { return mac; }

static bool r8139_send(const void *frame, u32 len)
{
    if (len > 1792) return false;

    u32 tsd = R_TSD0 + tx_slot * 4;
    u32 st = inl((u16)(io + tsd));
    if (!(st & (1u << 13)) && st != 0)       /* still owned by the card */
        return false;

    const u8 *s = (const u8 *)frame;
    for (u32 i = 0; i < len; i++) tx_buf[tx_slot][i] = s[i];
    while (len < 60) tx_buf[tx_slot][len++] = 0;   /* the wire's floor */

    outl((u16)(io + R_TSAD0 + tx_slot * 4), (u32)tx_phys[tx_slot]);
    outl((u16)(io + tsd), len);              /* own goes to the card */

    tx_slot = (tx_slot + 1) & 3;
    return true;
}

static i32 r8139_recv(void *out, u32 max)
{
    if (inb((u16)(io + R_CMD)) & CMD_BUFE) return -1;

    const u8 *p = rx + rx_off;
    u16 status = (u16)(p[0] | (p[1] << 8));
    u16 len = (u16)(p[2] | (p[3] << 8));     /* includes the crc */

    if (!(status & 1) || len < 8 || len > 1800) {
        /* A torn header means the walk lost the card. Start over. */
        outb((u16)(io + R_CMD), CMD_RX_EN | CMD_TX_EN);
        rx_off = 0;
        outw((u16)(io + R_CAPR), (u16)(0 - 16));
        return -1;
    }

    u32 take = (u32)len - 4;
    if (take > max) take = max;
    u8 *dst = (u8 *)out;
    for (u32 i = 0; i < take; i++) dst[i] = p[4 + i];

    rx_off = (rx_off + 4 + len + 3) & ~3u;
    if (rx_off >= 8192) rx_off -= 8192;
    outw((u16)(io + R_CAPR), (u16)(rx_off - 16));
    outw((u16)(io + R_ISR), 0x05);           /* done with rok/rer */

    return (i32)take;
}

static const nic_ops r8139_ops = {
    .name = "rtl8139",
    .mac  = r8139_mac,
    .send = r8139_send,
    .recv = r8139_recv,
};

bool rtl8139_init(void)
{
    const pci_device *dev = NULL;
    for (u32 i = 0; i < pci_device_count(); i++) {
        const pci_device *d = pci_get(i);
        if (d->vendor == 0x10EC && d->device_id == 0x8139) { dev = d; break; }
    }
    if (!dev) return false;

    u32 command = pci_read32(dev, 0x04);
    pci_write32(dev, 0x04, command | (1u << 0) | (1u << 2));

    u32 bar0 = pci_read32(dev, 0x10);
    if (!(bar0 & 1)) return false;           /* this family speaks io */
    io = (u16)(bar0 & ~3u);

    outb((u16)(io + R_CONFIG1), 0);          /* wake it */
    outb((u16)(io + R_CMD), CMD_RESET);
    for (u32 i = 0; i < 100000; i++)
        if (!(inb((u16)(io + R_CMD)) & CMD_RESET)) break;

    for (u32 i = 0; i < 6; i++) mac[i] = inb((u16)(io + R_MAC + i));

    /* The receive run: contiguous, and low -- the card holds only 32
     * bits of address. The frames come off the physical allocator's
     * bottom end, which on the machines this will meet is well below
     * the line. */
    phys_addr rxp = pmm_alloc_contig(PAGE_UP(RX_SIZE) / PAGE_SIZE);
    if (rxp == PMM_NO_FRAME || rxp > 0xFFFFFFFFULL - RX_SIZE) return false;
    rx = (u8 *)phys_to_virt(rxp);
    rx_off = 0;

    for (u32 i = 0; i < 4; i += 2) {
        phys_addr t = pmm_alloc();
        if (t == PMM_NO_FRAME || t > 0xFFFFFFFFULL) return false;
        tx_phys[i] = t;
        tx_phys[i + 1] = t + 2048;
        tx_buf[i] = (u8 *)phys_to_virt(t);
        tx_buf[i + 1] = (u8 *)phys_to_virt(t) + 2048;
    }

    outl((u16)(io + R_RBSTART), (u32)rxp);
    outw((u16)(io + R_IMR), 0);              /* polled, like the others */
    outl((u16)(io + R_RCR), RCR_BITS);
    outb((u16)(io + R_CMD), CMD_RX_EN | CMD_TX_EN);

    kprintf("net:  rtl8139 at %02x:%02x.%u, "
            "%02x:%02x:%02x:%02x:%02x:%02x\n",
            dev->bus, dev->device, dev->function,
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    nic_register(&r8139_ops);
    return true;
}
