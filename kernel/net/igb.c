/*
 * igb.c -- Intel's later cabled cards: the 82575 and 82576, the 82580
 * and I350, and the I210 and I211 that sit on a great many desktop
 * boards.
 *
 * Same idea as e1000.c and a different shape of descriptor, which is
 * why it is a separate file rather than another branch in that one.
 * These cards were built for many queues at once, so the rings moved
 * out of the fixed addresses the older family used into a block per
 * queue, and the descriptors grew a second form: what the driver
 * writes into one and what the card writes back over it are no longer
 * the same layout. One queue is used here. A machine that needs
 * sixteen of them is not this machine yet, and pretending otherwise
 * would be sixteen times the code for none of the benefit.
 *
 * No interrupts, as everywhere here: the network thread polls.
 */
#include <eb/net.h>
#include <eb/pci.h>
#include <eb/pmm.h>
#include <eb/vmm.h>
#include <eb/mm.h>
#include <eb/io.h>
#include <eb/time.h>
#include <eb/thread.h>
#include <eb/string.h>
#include <eb/fmt.h>

#define R_CTRL     0x00000
#define R_STATUS   0x00008
#define R_CTRL_EXT 0x00018
#define R_ICR      0x000C0
#define R_IMC      0x000D8
#define R_RCTL     0x00100
#define R_TCTL     0x00400
#define R_MRQC     0x05818
#define R_MTA      0x05200
#define R_RAL0     0x05400
#define R_RAH0     0x05404
#define R_RLPML    0x05004
#define R_VMOLR0   0x05AD0
#define R_VFRE     0x00C8C
#define R_VFTE     0x00C90

/* These cards were built to be shared between several guests of a
 * hypervisor, and the sharing is not something that can be switched
 * off and forgotten: every arriving frame is matched not only against
 * an address but against a *pool*, and a frame matching no pool is
 * dropped without a word. A driver that has never heard of pools sets
 * up perfect rings, sends perfectly good frames, and then watches the
 * answers vanish -- which is exactly what happened here.
 *
 * So the address is claimed for pool zero, and pool zero is told what
 * it will accept. On a card with no sharing to do this is the same
 * pool the frames were always going to land in; the difference is that
 * now it is said out loud. */
#define RAH_POOL0  (1u << 18)
#define RAH_VALID  (1u << 31)
#define VMOLR_ROMPE (1u << 13)       /* what the multicast table matched */
#define VMOLR_ROPE  (1u << 14)       /* what the unicast table matched */
#define VMOLR_AUPE  (1u << 24)       /* frames with no vlan tag */
#define VMOLR_BAM   (1u << 27)       /* broadcasts */

/* One queue's worth of ring registers. The older 82575 kept them where
 * the previous family had them; everything after it moved them. */
#define RX_BLOCK   0x0C000
#define TX_BLOCK   0x0E000
#define RX_OLD     0x02800
#define TX_OLD     0x03800

#define CTRL_SLU   (1u << 6)
#define CTRL_RST   (1u << 26)
#define STATUS_LU  (1u << 1)

#define RCTL_EN    (1u << 1)
#define RCTL_BAM   (1u << 15)
#define RCTL_SECRC (1u << 26)
#define TCTL_EN    (1u << 1)
#define TCTL_PSP   (1u << 3)

#define SRRCTL_ADV   (1u << 25)      /* the card writes back the new shape */
#define QUEUE_ENABLE (1u << 25)

/* What the driver puts in, and what the card writes back over it. The
 * card is told which it is reading by the descriptor type in srrctl. */
typedef union {
    struct { u64 pkt_addr; u64 hdr_addr; } read;
    struct { u32 info; u32 hash; u32 status_error; u16 length; u16 vlan; } wb;
} __attribute__((packed)) rx_desc;

typedef struct {
    u64 addr;
    u32 cmd_type_len;
    u32 olinfo_status;
} __attribute__((packed)) tx_desc;

#define RX_DD   (1u << 0)
#define TX_DD   (1u << 0)

#define D_DATA  (3u << 20)           /* this descriptor carries frame bytes */
#define D_EOP   (1u << 24)           /* the frame ends here */
#define D_IFCS  (1u << 25)           /* the card appends the check sequence */
#define D_RS    (1u << 27)           /* write back when it is done */
#define D_DEXT  (1u << 29)           /* read it the new way */

#define RX_COUNT 32
#define TX_COUNT 16
#define BUF_SIZE 2048

static volatile u8 *regs;
static u32      rx_base, tx_base;
static rx_desc *rx_ring;
static tx_desc *tx_ring;
static u8      *rx_buf[RX_COUNT];
static phys_addr rx_phys[RX_COUNT];   /* the card is given these back each time */
static u8      *tx_buf[TX_COUNT];
static u32      rx_next, tx_next;
static u8       mac[6];
static bool     up;

static u32  rr(u32 off)         { return *(volatile u32 *)(regs + off); }
static void wr(u32 off, u32 v)  { *(volatile u32 *)(regs + off) = v; }

/* The ring registers of queue zero, wherever this generation put them. */
#define RDBAL  (rx_base + 0x00)
#define RDBAH  (rx_base + 0x04)
#define RDLEN  (rx_base + 0x08)
#define SRRCTL (rx_base + 0x0C)
#define RDH    (rx_base + 0x10)
#define RDT    (rx_base + 0x18)
#define RXDCTL (rx_base + 0x28)
#define TDBAL  (tx_base + 0x00)
#define TDBAH  (tx_base + 0x04)
#define TDLEN  (tx_base + 0x08)
#define TDH    (tx_base + 0x10)
#define TDT    (tx_base + 0x18)
#define TXDCTL (tx_base + 0x28)

static const u8 *igb_get_mac(void) { return mac; }
static bool igb_send(const void *frame, u32 len);
static i32  igb_recv(void *out, u32 max);

static nic_ops igb_ops = {
    .name = "igb",
    .mac  = igb_get_mac,
    .send = igb_send,
    .recv = igb_recv,
};

#define OLD_RINGS 1
#define NEW_RINGS 0

typedef struct { u16 id; u8 old_rings; const char *name; } known;

static const known cards[] = {
    { 0x10A7, OLD_RINGS, "82575eb" },  { 0x10A9, OLD_RINGS, "82575eb" },
    { 0x10D6, OLD_RINGS, "82575gb" },
    { 0x10C9, NEW_RINGS, "82576" },    { 0x10E6, NEW_RINGS, "82576" },
    { 0x10E7, NEW_RINGS, "82576" },    { 0x10E8, NEW_RINGS, "82576" },
    { 0x1526, NEW_RINGS, "82576" },    { 0x150A, NEW_RINGS, "82576" },
    { 0x1518, NEW_RINGS, "82576" },    { 0x150D, NEW_RINGS, "82576" },
    { 0x1522, NEW_RINGS, "i350" },     { 0x1521, NEW_RINGS, "i350" },
    { 0x1523, NEW_RINGS, "i350" },     { 0x1524, NEW_RINGS, "i350" },
    { 0x150E, NEW_RINGS, "82580" },    { 0x150F, NEW_RINGS, "82580" },
    { 0x1510, NEW_RINGS, "82580" },    { 0x1511, NEW_RINGS, "82580" },
    { 0x1516, NEW_RINGS, "82580" },    { 0x1527, NEW_RINGS, "82580" },
    { 0x1533, NEW_RINGS, "i210" },     { 0x1536, NEW_RINGS, "i210" },
    { 0x1537, NEW_RINGS, "i210" },     { 0x1538, NEW_RINGS, "i210" },
    { 0x157B, NEW_RINGS, "i210" },     { 0x157C, NEW_RINGS, "i210" },
    { 0x1F40, NEW_RINGS, "i210" },     { 0x1F41, NEW_RINGS, "i210" },
    { 0x1F45, NEW_RINGS, "i210" },     { 0x1539, NEW_RINGS, "i211" },
};

static const known *look_up(u16 id)
{
    for (u32 i = 0; i < sizeof(cards) / sizeof(cards[0]); i++)
        if (cards[i].id == id) return &cards[i];
    return NULL;
}

static void wait_ms(u64 ms)
{
    u64 since = time_ns();
    while (time_ns() - since < ms * 1000000ULL) sched_yield();
}

/* ------------------------------------------------------------------ */
/* The part that talks to the wire                                     */
/* ------------------------------------------------------------------ */
/*
 * Between the card and the cable sits a second chip, and it is reached
 * a register at a time through a window: put the register number and
 * the operation in, wait for the ready bit, take the answer out.
 *
 * Setting the link-up bit in the card's own control register is not
 * enough on its own, and this is the lesson that cost the afternoon:
 * frames went out perfectly well and every answer was thrown away,
 * because a receiver whose link is down does not take frames even when
 * its rings are flawless. The link comes up when the two ends have
 * agreed on a speed, and they only do that when somebody asks them to.
 */
#define R_MDIC     0x00020
#define R_MDICNFG  0x00E04
#define MDIC_READY (1u << 28)
#define MDIC_ERROR (1u << 30)
#define MDIC_WRITE (1u << 26)
#define MDIC_READ  (2u << 26)
#define MDIC_PHY1  (1u << 21)

#define PHY_CTRL       0
#define PHY_AUTONEG    (1u << 12)
#define PHY_RESTART    (1u << 9)

static u32 phy_at = MDIC_PHY1;       /* the second chip's address, in place */

static bool mdic_wait(u32 *out)
{
    for (u32 i = 0; i < 2000; i++) {
        u32 v = rr(R_MDIC);
        if (v & MDIC_ERROR) return false;
        if (v & MDIC_READY) { if (out) *out = v; return true; }
        for (volatile u32 s = 0; s < 200; s++) { }
    }
    return false;
}

static u16 phy_read(u32 reg)
{
    u32 v = 0;
    wr(R_MDIC, (reg << 16) | phy_at | MDIC_READ);
    if (!mdic_wait(&v)) return 0xFFFF;
    return (u16)v;
}

static void phy_write(u32 reg, u16 value)
{
    wr(R_MDIC, value | (reg << 16) | phy_at | MDIC_WRITE);
    mdic_wait(NULL);
}

static bool bring_up(const pci_device *dev, const known *k, bool need_link)
{
    u32 command = pci_read32(dev, 0x04);
    pci_write32(dev, 0x04, command | (1u << 1) | (1u << 2));

    phys_addr bar = pci_bar(dev, 0);
    if (!bar) return false;
    /* The register window is 128 KiB on every one of these, and nothing
     * here reaches past the first 64. Mapping more than the card has
     * would put the next device's window under this driver's feet. */
    if (!vmm_map(vmm_kernel_pml4(), (virt_addr)phys_to_virt(bar), bar,
                 32 * PAGE_SIZE, PAGE_KERNEL_MMIO))
        return false;
    regs = (volatile u8 *)phys_to_virt(bar);
    rx_base = k->old_rings ? RX_OLD : RX_BLOCK;
    tx_base = k->old_rings ? TX_OLD : TX_BLOCK;

    if (rr(R_STATUS) == 0xFFFFFFFFu) {
        kprintf("net:  %s at %02x:%02x.%u: its registers read as all ones; "
                "it is not answering\n", k->name, dev->bus, dev->device, dev->function);
        return false;
    }

    /* On the round that only wants a card with a cable in it: look at
     * the link the firmware left. Where it is down, ask the wire's chip
     * once to agree a speed and give it a moment -- that is a word to
     * the chip, not a reset, and a card left by its firmware with a
     * cable in it but no link is a real thing. A card that is about to
     * be passed over is never reset on the way past. */
    if (need_link && !(rr(R_STATUS) & STATUS_LU)) {
        phy_at = MDIC_PHY1;
        if (!k->old_rings) {
            u32 addr = (rr(R_MDICNFG) >> 21) & 0x1Fu;
            if (addr) phy_at = addr << 21;
        }
        kprintf("net:  %s at %02x:%02x.%u: no link; asking the wire's chip at address %u\n",
                k->name, dev->bus, dev->device, dev->function, phy_at >> 21);
        u16 pc = phy_read(PHY_CTRL);
        if (pc != 0xFFFF) phy_write(PHY_CTRL, (u16)(pc | PHY_AUTONEG | PHY_RESTART));
        for (u32 i = 0; i < 30 && !(rr(R_STATUS) & STATUS_LU); i++) wait_ms(50);
        if (!(rr(R_STATUS) & STATUS_LU)) {
            kprintf("net:  %s: still no link; passed over for now\n", k->name);
            return false;
        }
    }

    kprintf("net:  %s at %02x:%02x.%u: taking it\n",
            k->name, dev->bus, dev->device, dev->function);

    wr(R_IMC, 0xFFFFFFFFu);
    (void)rr(R_ICR);
    wr(R_RCTL, 0);
    wr(R_TCTL, 0);
    wait_ms(10);

    wr(R_CTRL, rr(R_CTRL) | CTRL_RST);
    wait_ms(10);
    for (u32 i = 0; i < 100 && (rr(R_CTRL) & CTRL_RST); i++) wait_ms(1);
    wait_ms(20);                      /* the card reloads itself from its own memory */
    if (rr(R_STATUS) == 0xFFFFFFFFu) {
        kprintf("net:  %s: did not come back from its reset\n", k->name);
        return false;
    }
    kprintf("net:  %s: reset\n", k->name);
    wr(R_IMC, 0xFFFFFFFFu);
    (void)rr(R_ICR);
    wr(R_RCTL, 0);
    wr(R_TCTL, 0);

    /* Where the second chip answers. The 82576 keeps its own at address
     * one; the 82580 and everything after it say where in a register
     * of their own, and asking at the wrong address gets a silence that
     * looks like a broken wire. */
    phy_at = MDIC_PHY1;
    if (!k->old_rings) {
        u32 cfg = rr(R_MDICNFG);
        u32 addr = (cfg >> 21) & 0x1Fu;
        if (addr) phy_at = addr << 21;
    }

    /* The address the card answers to; hardware copies it up out of its
     * own memory while it resets, so there is nothing to read out. */
    u32 ral = rr(R_RAL0), rah = rr(R_RAH0);
    if (ral == 0 && (rah & 0xFFFF) == 0) return false;
    mac[0] = (u8)ral; mac[1] = (u8)(ral >> 8);
    mac[2] = (u8)(ral >> 16); mac[3] = (u8)(ral >> 24);
    mac[4] = (u8)rah; mac[5] = (u8)(rah >> 8);
    wr(R_RAH0, (rah & 0xFFFF) | RAH_POOL0 | RAH_VALID);

    for (u32 i = 0; i < 128; i++) wr(R_MTA + i * 4, 0);
    wr(R_MRQC, 0);                    /* one queue: everything arrives on it */
    wr(R_VMOLR0, VMOLR_BAM | VMOLR_AUPE | VMOLR_ROPE | VMOLR_ROMPE);
    wr(R_VFRE, 1);                    /* pool zero may receive */
    wr(R_VFTE, 1);                    /* and send */

    wr(R_CTRL, rr(R_CTRL) | CTRL_SLU);

    /* Ask the two ends of the cable to agree on a speed. Without this
     * the link stays down however long anyone waits, and a card with a
     * link down keeps its rings and throws every frame away. */
    u16 pc = phy_read(PHY_CTRL);
    if (pc != 0xFFFF) {
        phy_write(PHY_CTRL, (u16)(pc | PHY_AUTONEG | PHY_RESTART));
        kprintf("net:  %s: the wire's chip answers at address %u; asked to agree a speed\n",
                k->name, phy_at >> 21);
    } else {
        kprintf("net:  %s: the wire's chip did not answer at address %u; "
                "the link is whatever the firmware left\n", k->name, phy_at >> 21);
    }

    /* Agreeing takes a moment even between two ends that are only
     * pretending to be far apart. */
    for (u32 i = 0; i < 60 && !(rr(R_STATUS) & STATUS_LU); i++) wait_ms(50);
    kprintf("net:  %s: %s; setting up the rings\n", k->name,
            (rr(R_STATUS) & STATUS_LU) ? "link up" : "no link yet");

    phys_addr rxp = pmm_alloc();
    phys_addr txp = pmm_alloc();
    if (rxp == PMM_NO_FRAME || txp == PMM_NO_FRAME) return false;
    rx_ring = (rx_desc *)phys_to_virt(rxp);
    tx_ring = (tx_desc *)phys_to_virt(txp);
    memset(rx_ring, 0, PAGE_SIZE);
    memset(tx_ring, 0, PAGE_SIZE);
    rx_next = tx_next = 0;

    for (u32 i = 0; i < RX_COUNT; i += 2) {
        phys_addr b = pmm_alloc();
        if (b == PMM_NO_FRAME) return false;
        rx_phys[i]     = b;
        rx_phys[i + 1] = b + BUF_SIZE;
        rx_ring[i].read.pkt_addr     = b;
        rx_ring[i + 1].read.pkt_addr = b + BUF_SIZE;
        rx_buf[i]     = (u8 *)phys_to_virt(b);
        rx_buf[i + 1] = (u8 *)phys_to_virt(b) + BUF_SIZE;
    }
    for (u32 i = 0; i < TX_COUNT; i += 2) {
        phys_addr b = pmm_alloc();
        if (b == PMM_NO_FRAME) return false;
        tx_ring[i].addr     = b;
        tx_ring[i + 1].addr = b + BUF_SIZE;
        tx_ring[i].olinfo_status     = TX_DD;
        tx_ring[i + 1].olinfo_status = TX_DD;
        tx_buf[i]     = (u8 *)phys_to_virt(b);
        tx_buf[i + 1] = (u8 *)phys_to_virt(b) + BUF_SIZE;
    }

    /* Receive. The buffer size lives in its own register here rather
     * than in two bits of the receive control, and the same register
     * is where the card is told to write back the new shape. */
    wr(RDBAL, (u32)rxp);
    wr(RDBAH, (u32)(rxp >> 32));
    wr(RDLEN, RX_COUNT * sizeof(rx_desc));
    wr(SRRCTL, (BUF_SIZE / 1024) | SRRCTL_ADV);
    wr(RDH, 0);
    wr(RDT, 0);
    wr(RXDCTL, QUEUE_ENABLE | 8u | (8u << 8) | (1u << 16));
    for (u32 i = 0; i < 100 && !(rr(RXDCTL) & QUEUE_ENABLE); i++) wait_ms(1);

    wr(TDBAL, (u32)txp);
    wr(TDBAH, (u32)(txp >> 32));
    wr(TDLEN, TX_COUNT * sizeof(tx_desc));
    wr(TDH, 0);
    wr(TDT, 0);
    wr(TXDCTL, QUEUE_ENABLE | 8u | (1u << 8) | (1u << 16));
    for (u32 i = 0; i < 100 && !(rr(TXDCTL) & QUEUE_ENABLE); i++) wait_ms(1);

    wr(R_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC);
    wr(R_TCTL, TCTL_EN | TCTL_PSP);

    /* Only now: the queue has to be running before it is handed
     * descriptors, or the tail is written into a ring nobody reads. */
    wr(RDT, RX_COUNT - 1);

    igb_ops.name = k->name;
    kprintf("net:  %s at %02x:%02x.%u, %02x:%02x:%02x:%02x:%02x:%02x, %s\n",
            k->name, dev->bus, dev->device, dev->function,
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
            (rr(R_STATUS) & STATUS_LU) ? "a cable is in it" : "no cable");

    up = true;
    nic_register(&igb_ops);
    return true;
}

bool igb_init(bool need_link)
{
    if (up) return false;
    u32 n = pci_device_count();
    for (u32 i = 0; i < n; i++) {
        const pci_device *dev = pci_get(i);
        if (!dev || dev->vendor != 0x8086 || dev->class_code != 0x02) continue;
        const known *k = look_up(dev->device_id);
        if (!k) continue;
        if (bring_up(dev, k, need_link)) return true;
    }
    return false;
}

bool igb_knows(u16 device_id) { return look_up(device_id) != NULL; }

static bool igb_send(const void *frame, u32 len)
{
    if (!up || len > BUF_SIZE) return false;

    tx_desc *d = &tx_ring[tx_next];
    if (!(d->olinfo_status & TX_DD)) return false;    /* ring full; caller retries */

    const u8 *s = (const u8 *)frame;
    for (u32 i = 0; i < len; i++) tx_buf[tx_next][i] = s[i];

    d->cmd_type_len  = len | D_DATA | D_DEXT | D_EOP | D_IFCS | D_RS;
    d->olinfo_status = len << 14;                     /* the whole frame is payload */

    tx_next = (tx_next + 1) % TX_COUNT;
    wr(TDT, tx_next);
    return true;
}

static i32 igb_recv(void *out, u32 max)
{
    if (!up) return -1;

    rx_desc *d = &rx_ring[rx_next];
    if (!(d->wb.status_error & RX_DD)) return -1;

    u32 len = d->wb.length;
    if (len > max) len = max;
    u8 *dst = (u8 *)out;
    for (u32 i = 0; i < len; i++) dst[i] = rx_buf[rx_next][i];

    /* The card wrote its answer over the address it was given, so the
     * address goes back before the descriptor does. */
    d->read.pkt_addr = rx_phys[rx_next];
    d->read.hdr_addr = 0;

    wr(RDT, rx_next);
    rx_next = (rx_next + 1) % RX_COUNT;
    return (i32)len;
}
