/*
 * rtl8169.c -- the Realtek gigabit family: 8168, 8169, 8101 and kin,
 * which is to say most machines with a cable socket.
 *
 * Descriptor rings like the e1000's, spoken to through io ports like
 * the 8139's. The family has a decade of revisions with their own
 * initialisation folklore; this driver does only what the common
 * programming model promises -- reset, rings, enable -- and refuses
 * quietly if the silicon does not come up. Written against the
 * documentation and the emulator has no model of it, so until it has
 * met real hardware it should be read as a considered attempt, and
 * says so in the boot log.
 */
#include <eb/net.h>
#include <eb/pci.h>
#include <eb/pmm.h>
#include <eb/mm.h>
#include <eb/io.h>
#include <eb/fmt.h>

#define R_MAC     0x00
#define R_TNPDS   0x20              /* transmit ring, 64 bits */
#define R_CMD     0x37
#define R_TPPOLL  0x38
#define R_IMR     0x3C
#define R_ISR     0x3E
#define R_TCR     0x40
#define R_RCR     0x44
#define R_9346CR  0x50
#define R_RMS     0xDA              /* largest packet accepted */
#define R_CPLUS   0xE0
#define R_RDSAR   0xE4              /* receive ring, 64 bits */
#define R_ETTHR   0xEC

#define CMD_RESET 0x10
#define CMD_RX_EN 0x08
#define CMD_TX_EN 0x04

#define OWN (1u << 31)
#define EOR (1u << 30)
#define FS  (1u << 29)
#define LS  (1u << 28)

typedef struct {
    u32 opts1;
    u32 opts2;
    u64 addr;
} __attribute__((packed)) r_desc;

#define RXN 32
#define TXN 8
#define BUFSZ 2048

static u16     io;
static u8      mac[6];
static r_desc *rxd, *txd;
static u8     *rxb[RXN], *txb[TXN];
static u32     rx_cur, tx_cur;

static const u8 *r8169_mac(void) { return mac; }

static bool r8169_send(const void *frame, u32 len)
{
    if (len > 1514) return false;
    r_desc *d = &txd[tx_cur];
    if (d->opts1 & OWN) return false;

    const u8 *s = (const u8 *)frame;
    for (u32 i = 0; i < len; i++) txb[tx_cur][i] = s[i];
    if (len < 60) { while (len < 60) txb[tx_cur][len++] = 0; }

    d->opts2 = 0;
    d->opts1 = OWN | FS | LS | (tx_cur == TXN - 1 ? EOR : 0) | len;
    outb((u16)(io + R_TPPOLL), 0x40);        /* there is work */

    tx_cur = (tx_cur + 1) % TXN;
    return true;
}

static i32 r8169_recv(void *out, u32 max)
{
    r_desc *d = &rxd[rx_cur];
    if (d->opts1 & OWN) return -1;

    u32 len = d->opts1 & 0x3FFF;
    i32 got = -1;
    if (len >= 8 && len <= BUFSZ) {
        u32 take = len - 4;                  /* the crc rides behind */
        if (take > max) take = max;
        u8 *dst = (u8 *)out;
        for (u32 i = 0; i < take; i++) dst[i] = rxb[rx_cur][i];
        got = (i32)take;
    }

    d->opts2 = 0;
    d->opts1 = OWN | (rx_cur == RXN - 1 ? EOR : 0) | BUFSZ;
    rx_cur = (rx_cur + 1) % RXN;
    return got;
}

static const nic_ops r8169_ops = {
    .name = "rtl8169",
    .mac  = r8169_mac,
    .send = r8169_send,
    .recv = r8169_recv,
};

bool rtl8169_init(void)
{
    static const u16 kin[] = { 0x8168, 0x8169, 0x8167, 0x8161, 0x8136 };
    const pci_device *dev = NULL;
    for (u32 i = 0; i < pci_device_count() && !dev; i++) {
        const pci_device *d = pci_get(i);
        if (d->vendor != 0x10EC) continue;
        for (u32 k = 0; k < ARRAY_LEN(kin); k++)
            if (d->device_id == kin[k]) { dev = d; break; }
    }
    if (!dev) return false;

    u32 command = pci_read32(dev, 0x04);
    pci_write32(dev, 0x04, command | (1u << 0) | (1u << 2));

    u32 bar0 = pci_read32(dev, 0x10);
    if (!(bar0 & 1)) return false;
    io = (u16)(bar0 & ~3u);

    outb((u16)(io + R_CMD), CMD_RESET);
    bool settled = false;
    for (u32 i = 0; i < 100000; i++)
        if (!(inb((u16)(io + R_CMD)) & CMD_RESET)) { settled = true; break; }
    if (!settled) return false;

    for (u32 i = 0; i < 6; i++) mac[i] = inb((u16)(io + R_MAC + i));
    bool blank = true;
    for (u32 i = 0; i < 6; i++) if (mac[i] != 0 && mac[i] != 0xFF) blank = false;
    if (blank) return false;                 /* no address, no card */

    phys_addr rdp = pmm_alloc();
    phys_addr tdp = pmm_alloc();
    if (rdp == PMM_NO_FRAME || tdp == PMM_NO_FRAME) return false;
    rxd = (r_desc *)phys_to_virt(rdp);
    txd = (r_desc *)phys_to_virt(tdp);

    for (u32 i = 0; i < RXN; i += 2) {
        phys_addr b = pmm_alloc();
        if (b == PMM_NO_FRAME) return false;
        rxd[i]     = (r_desc){ .opts1 = OWN | BUFSZ, .addr = b };
        rxd[i + 1] = (r_desc){ .opts1 = OWN | BUFSZ, .addr = b + BUFSZ };
        rxb[i]     = (u8 *)phys_to_virt(b);
        rxb[i + 1] = (u8 *)phys_to_virt(b) + BUFSZ;
    }
    rxd[RXN - 1].opts1 |= EOR;

    for (u32 i = 0; i < TXN; i += 2) {
        phys_addr b = pmm_alloc();
        if (b == PMM_NO_FRAME) return false;
        txd[i]     = (r_desc){ .addr = b };
        txd[i + 1] = (r_desc){ .addr = b + BUFSZ };
        txb[i]     = (u8 *)phys_to_virt(b);
        txb[i + 1] = (u8 *)phys_to_virt(b) + BUFSZ;
    }

    outb((u16)(io + R_9346CR), 0xC0);        /* unlock the config */

    outw((u16)(io + R_CPLUS), inw((u16)(io + R_CPLUS)));
    outw((u16)(io + R_RMS), BUFSZ);
    outb((u16)(io + R_ETTHR), 0x3B);         /* whole frames only */

    outl((u16)(io + R_RDSAR), (u32)rdp);
    outl((u16)(io + R_RDSAR + 4), (u32)(rdp >> 32));
    outl((u16)(io + R_TNPDS), (u32)tdp);
    outl((u16)(io + R_TNPDS + 4), (u32)(tdp >> 32));

    outb((u16)(io + R_CMD), CMD_RX_EN | CMD_TX_EN);
    outl((u16)(io + R_TCR), (7u << 8) | (3u << 24));
    outl((u16)(io + R_RCR), 0x0F | (7u << 8) | (7u << 13));
    outw((u16)(io + R_IMR), 0);              /* polled, like the others */

    outb((u16)(io + R_9346CR), 0x00);        /* lock it again */

    kprintf("net:  rtl8169 kin %04x at %02x:%02x.%u, "
            "%02x:%02x:%02x:%02x:%02x:%02x -- first meeting with "
            "this silicon\n",
            dev->device_id, dev->bus, dev->device, dev->function,
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    nic_register(&r8169_ops);
    return true;
}
