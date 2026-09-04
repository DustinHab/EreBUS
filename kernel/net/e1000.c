/*
 * e1000.c -- Intel cabled cards with legacy descriptors: 8254x, the PCIe parts, chipset-integrated ones.
 * - two descriptor rings, polled by the network thread; no interrupts
 * - three families in one ID table: EEPROM read register differs, the chipset family is never reset
 * - PCIe family: GIO master disable before reset
 * - frames only; protocols live in net.c
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

/* Registers, as byte offsets into the mapped window. */
#define R_CTRL    0x0000
#define R_STATUS  0x0008
#define R_EERD    0x0014
#define R_ICR     0x00C0
#define R_IMC     0x00D8
#define R_RCTL    0x0100
#define R_TCTL    0x0400
#define R_RDBAL   0x2800
#define R_RDBAH   0x2804
#define R_RDLEN   0x2808
#define R_RDH     0x2810
#define R_RDT     0x2818
#define R_TDBAL   0x3800
#define R_TDBAH   0x3804
#define R_TDLEN   0x3808
#define R_TDH     0x3810
#define R_TDT     0x3818
#define R_MTA     0x5200
#define R_RAL0    0x5400
#define R_RAH0    0x5404

#define CTRL_SLU     (1u << 6)      /* set link up */
#define CTRL_RST     (1u << 26)
#define STATUS_LU    (1u << 1)      /* the wire has somebody at the other end */
#define CTRL_GIO_MD  (1u << 2)      /* finish what is on the bus, start nothing new */
#define STATUS_GIO_ME (1u << 19)    /* still a master on the bus */
#define RCTL_EN      (1u << 1)
#define RCTL_BAM     (1u << 15)     /* take broadcasts */
#define RCTL_SECRC   (1u << 26)     /* strip the crc */
#define TCTL_EN      (1u << 1)
#define TCTL_PSP     (1u << 3)

#define TX_CMD_EOP   0x01
#define TX_CMD_IFCS  0x02
#define TX_CMD_RS    0x08
#define DD           0x01           /* descriptor done, both rings */

typedef struct {
    u64 addr;
    u16 length;
    u16 checksum;
    u8  status;
    u8  errors;
    u16 special;
} __attribute__((packed)) rx_desc;

typedef struct {
    u64 addr;
    u16 length;
    u8  cso;
    u8  cmd;
    u8  status;
    u8  css;
    u16 special;
} __attribute__((packed)) tx_desc;

#define RX_COUNT 32
#define TX_COUNT 8
#define BUF_SIZE 2048

static volatile u8 *regs;
static rx_desc *rx_ring;
static tx_desc *tx_ring;
static u8      *rx_buf[RX_COUNT];
static u8      *tx_buf[TX_COUNT];
static u32      rx_next;
static u32      tx_next;
static u8       mac[6];
static bool     up;
static const char *card_name = "e1000";

static u32 rr(u32 off)          { return *(volatile u32 *)(regs + off); }
static void wr(u32 off, u32 v)  { *(volatile u32 *)(regs + off) = v; }

static const u8 *e1000_get_mac(void) { return mac; }

static bool e1000_send(const void *frame, u32 len);
static i32  e1000_recv(void *out, u32 max);

static nic_ops e1000_ops = {
    .name = "e1000",
    .mac  = e1000_get_mac,
    .send = e1000_send,
    .recv = e1000_recv,
};

/* ------------------------------------------------------------------ */
/* Which chip is which                                                 */
/* ------------------------------------------------------------------ */
/*
 * Three ways of being an Intel card with these rings.
 *
 * OLD   the 8254x on a PCI slot. Its little serial memory answers with
 *       the address in bits eight and up, and says so in bit four.
 * PCIE  the 8257x and their kin. The same memory, read with the
 *       address two bits up and the answer in bit one -- a difference
 *       of two shifts that would otherwise read a plausible wrong
 *       address and hand the machine somebody else's name.
 * CHIP  built into the chipset, from the 82577 to the I219. Its
 *       address is put into the card's own registers before the system
 *       ever runs, so there is nothing to read; and the wire is shared
 *       with the firmware's management engine, so it is not reset.
 */
#define OLD  0
#define PCIE 1
#define CHIP 2

typedef struct { u16 id; u8 kind; const char *name; } known;

static const known cards[] = {
    /* the 8254x, and what emulators offer */
    { 0x1000, OLD,  "82542" },       { 0x1001, OLD,  "82543gc" },
    { 0x1004, OLD,  "82543gc" },     { 0x1008, OLD,  "82544ei" },
    { 0x1009, OLD,  "82544ei" },     { 0x100C, OLD,  "82544gc" },
    { 0x100D, OLD,  "82544gc" },     { 0x100E, OLD,  "82540em" },
    { 0x100F, OLD,  "82545em" },     { 0x1010, OLD,  "82546eb" },
    { 0x1011, OLD,  "82545em" },     { 0x1012, OLD,  "82546eb" },
    { 0x1013, OLD,  "82541ei" },     { 0x1014, OLD,  "82541er" },
    { 0x1015, OLD,  "82540em" },     { 0x1016, OLD,  "82540ep" },
    { 0x1017, OLD,  "82540ep" },     { 0x1018, OLD,  "82541ei" },
    { 0x1019, OLD,  "82547ei" },     { 0x101A, OLD,  "82547ei" },
    { 0x101D, OLD,  "82546eb" },     { 0x101E, OLD,  "82540ep" },
    { 0x1026, OLD,  "82545gm" },     { 0x1027, OLD,  "82545gm" },
    { 0x1028, OLD,  "82545gm" },     { 0x1075, OLD,  "82547gi" },
    { 0x1076, OLD,  "82541gi" },     { 0x1077, OLD,  "82541gi" },
    { 0x1078, OLD,  "82541er" },     { 0x1079, OLD,  "82546gb" },
    { 0x107A, OLD,  "82546gb" },     { 0x107B, OLD,  "82546gb" },
    { 0x107C, OLD,  "82541pi" },     { 0x10B5, OLD,  "82546gb" },
    { 0x1099, OLD,  "82546gb" },     { 0x10A4, PCIE, "82571eb" },
    { 0x10A5, PCIE, "82571eb" },     { 0x10BC, PCIE, "82571eb" },
    { 0x10D5, PCIE, "82571pt" },     { 0x10D9, PCIE, "82571eb" },
    { 0x10DA, PCIE, "82571eb" },     { 0x105E, PCIE, "82571eb" },
    { 0x1060, PCIE, "82571eb" },     { 0x107D, PCIE, "82572ei" },
    { 0x107E, PCIE, "82572ei" },     { 0x107F, PCIE, "82572ei" },
    { 0x10B9, PCIE, "82572ei" },     { 0x108B, PCIE, "82573v" },
    { 0x108C, PCIE, "82573e" },      { 0x109A, PCIE, "82573l" },
    { 0x10D3, PCIE, "82574l" },      { 0x10F6, PCIE, "82574l" },
    { 0x150C, PCIE, "82583v" },      { 0x10EA, CHIP, "82577lm" },
    { 0x10EB, CHIP, "82577lc" },     { 0x10EF, CHIP, "82578dm" },
    { 0x10F0, CHIP, "82578dc" },     { 0x1502, CHIP, "82579lm" },
    { 0x1503, CHIP, "82579v" },      { 0x153A, CHIP, "i217-lm" },
    { 0x153B, CHIP, "i217-v" },      { 0x155A, CHIP, "i218-lm" },
    { 0x1559, CHIP, "i218-v" },      { 0x15A0, CHIP, "i218-lm" },
    { 0x15A1, CHIP, "i218-v" },      { 0x15A2, CHIP, "i218-lm" },
    { 0x15A3, CHIP, "i218-v" },      { 0x156F, CHIP, "i219-lm" },
    { 0x1570, CHIP, "i219-v" },      { 0x15B7, CHIP, "i219-lm" },
    { 0x15B8, CHIP, "i219-v" },      { 0x15B9, CHIP, "i219-lm" },
    { 0x15D6, CHIP, "i219-v" },      { 0x15D7, CHIP, "i219-lm" },
    { 0x15D8, CHIP, "i219-v" },      { 0x15E3, CHIP, "i219-lm" },
    { 0x15D9, CHIP, "i219-v" },      { 0x15DA, CHIP, "i219-lm" },
    { 0x15BB, CHIP, "i219-lm" },     { 0x15BC, CHIP, "i219-v" },
    { 0x15BD, CHIP, "i219-lm" },     { 0x15BE, CHIP, "i219-v" },
    { 0x15DF, CHIP, "i219-lm" },     { 0x15E0, CHIP, "i219-lm" },
    { 0x15E1, CHIP, "i219-v" },      { 0x15E2, CHIP, "i219-v" },
    { 0x0D4E, CHIP, "i219-lm" },     { 0x0D4F, CHIP, "i219-v" },
    { 0x0D4C, CHIP, "i219-v" },      { 0x0D4D, CHIP, "i219-lm" },
    { 0x1A1C, CHIP, "i219-lm" },     { 0x1A1D, CHIP, "i219-v" },
    { 0x1A1E, CHIP, "i219-lm" },     { 0x1A1F, CHIP, "i219-v" },
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

/* One word of the little serial memory, for cards whose address
 * registers came up empty. */
static u16 eeprom_read(u32 addr, u8 kind)
{
    u32 shift = (kind == OLD) ? 8 : 2;
    u32 done  = (kind == OLD) ? (1u << 4) : (1u << 1);
    wr(R_EERD, 1u | (addr << shift));
    for (u32 i = 0; i < 100000; i++) {
        u32 v = rr(R_EERD);
        if (v & done) return (u16)(v >> 16);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Bringing one card up                                                */
/* ------------------------------------------------------------------ */

static bool bring_up(const pci_device *dev, const known *k, bool need_link)
{
    /* Reaching memory on its own is off until asked for, like every
     * device here. */
    u32 command = pci_read32(dev, 0x04);
    pci_write32(dev, 0x04, command | (1u << 1) | (1u << 2));

    phys_addr bar = pci_bar(dev, 0);
    if (!bar) return false;
    if (!vmm_map(vmm_kernel_pml4(), (virt_addr)phys_to_virt(bar), bar,
                 32 * PAGE_SIZE, PAGE_KERNEL_MMIO))
        return false;
    regs = (volatile u8 *)phys_to_virt(bar);

    /* A device that is not really there answers every read with all
     * ones. Nothing below would fail on that; it would carry on and
     * set up rings for a card that does not exist. */
    if (rr(R_STATUS) == 0xFFFFFFFFu) {
        kprintf("net:  %s at %02x:%02x.%u: its registers read as all ones; "
                "it is not answering\n", k->name, dev->bus, dev->device, dev->function);
        return false;
    }

    /* On the round that only wants a card with a cable in it, look and
     * touch nothing else. The firmware left the link as it found it,
     * and that is a truer answer than anything this driver could make
     * the card say in the next two seconds -- and it costs no reset,
     * which matters for a card that is about to be passed over. */
    if (need_link) {
        if (!(rr(R_STATUS) & STATUS_LU)) return false;
    }

    kprintf("net:  %s at %02x:%02x.%u: selected\n",
            k->name, dev->bus, dev->device, dev->function);

    wr(R_IMC, 0xFFFFFFFFu);         /* no interrupts; this is a polled card */
    (void)rr(R_ICR);

    /* Stop whatever was running before touching where the rings live.
     * The firmware may have had this card carrying its own traffic --
     * a chipset card almost certainly did -- and repointing a ring
     * underneath a running receiver writes frames into memory that has
     * since become somebody else's. */
    wr(R_RCTL, 0);
    wr(R_TCTL, 0);
    wait_ms(10);

    if (k->kind != CHIP) {
        /* The PCI Express parts are asked to finish what they have on
         * the bus before the reset, and waited for; a reset with a
         * transfer in flight is a thing a bridge on the way may never
         * recover from. The old PCI parts have no such bit. */
        if (k->kind == PCIE) {
            wr(R_CTRL, rr(R_CTRL) | CTRL_GIO_MD);
            for (u32 i = 0; i < 100 && (rr(R_STATUS) & STATUS_GIO_ME); i++) wait_ms(1);
        }
        kprintf("net:  %s: resetting\n", k->name);
        wr(R_CTRL, rr(R_CTRL) | CTRL_RST);
        wait_ms(10);
        for (u32 i = 0; i < 100 && (rr(R_CTRL) & CTRL_RST); i++) wait_ms(1);
        wait_ms(10);
        if (rr(R_STATUS) == 0xFFFFFFFFu) {
            kprintf("net:  %s: did not come back from its reset\n", k->name);
            return false;
        }
        wr(R_IMC, 0xFFFFFFFFu);
        (void)rr(R_ICR);
        wr(R_RCTL, 0);
        wr(R_TCTL, 0);
        kprintf("net:  %s: reset\n", k->name);
    }

    /* The address the card answers to. A chipset card was given one
     * before this system ever ran; the others keep it in the little
     * serial memory, and hardware usually copies it up on its own. */
    u32 ral = rr(R_RAL0), rah = rr(R_RAH0);
    if (ral == 0 && (rah & 0xFFFF) == 0 && k->kind != CHIP) {
        u16 w0 = eeprom_read(0, k->kind);
        u16 w1 = eeprom_read(1, k->kind);
        u16 w2 = eeprom_read(2, k->kind);
        ral = (u32)w0 | ((u32)w1 << 16);
        rah = (u32)w2;
        wr(R_RAL0, ral);
        wr(R_RAH0, rah | (1u << 31));
    }
    if (ral == 0 && (rah & 0xFFFF) == 0) return false;   /* no address: not ours to drive */

    mac[0] = (u8)ral; mac[1] = (u8)(ral >> 8);
    mac[2] = (u8)(ral >> 16); mac[3] = (u8)(ral >> 24);
    mac[4] = (u8)rah; mac[5] = (u8)(rah >> 8);
    wr(R_RAH0, (rah & 0xFFFF) | (1u << 31));             /* the address is valid */

    for (u32 i = 0; i < 128; i++) wr(R_MTA + i * 4, 0);

    wr(R_CTRL, rr(R_CTRL) | CTRL_SLU);
    kprintf("net:  %s: mac address read; link requested\n", k->name);

    /* The receive ring: a page of descriptors, each with its own
     * buffer. The card owns everything between head and tail. */
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
        rx_ring[i]     = (rx_desc){ .addr = b };
        rx_ring[i + 1] = (rx_desc){ .addr = b + BUF_SIZE };
        rx_buf[i]      = (u8 *)phys_to_virt(b);
        rx_buf[i + 1]  = (u8 *)phys_to_virt(b) + BUF_SIZE;
    }
    for (u32 i = 0; i < TX_COUNT; i += 2) {
        phys_addr b = pmm_alloc();
        if (b == PMM_NO_FRAME) return false;
        tx_ring[i]     = (tx_desc){ .addr = b, .status = DD };
        tx_ring[i + 1] = (tx_desc){ .addr = b + BUF_SIZE, .status = DD };
        tx_buf[i]      = (u8 *)phys_to_virt(b);
        tx_buf[i + 1]  = (u8 *)phys_to_virt(b) + BUF_SIZE;
    }

    wr(R_RDBAL, (u32)rxp);
    wr(R_RDBAH, (u32)(rxp >> 32));
    wr(R_RDLEN, RX_COUNT * sizeof(rx_desc));
    wr(R_RDH, 0);
    wr(R_RDT, RX_COUNT - 1);

    wr(R_TDBAL, (u32)txp);
    wr(R_TDBAH, (u32)(txp >> 32));
    wr(R_TDLEN, TX_COUNT * sizeof(tx_desc));
    wr(R_TDH, 0);
    wr(R_TDT, 0);

    wr(R_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC);   /* 2048-byte buffers */
    wr(R_TCTL, TCTL_EN | TCTL_PSP | (0x10u << 4) | (0x40u << 12));

    card_name = k->name;
    e1000_ops.name = k->name;

    kprintf("net:  %s at %02x:%02x.%u, %02x:%02x:%02x:%02x:%02x:%02x, %s\n",
            k->name, dev->bus, dev->device, dev->function,
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
            (rr(R_STATUS) & STATUS_LU) ? "link up" : "no link");

    up = true;
    nic_register(&e1000_ops);
    return true;
}

bool e1000_init(bool need_link)
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

/* What was seen but not driven, so a machine with a card this driver
 * does not know says which card that was rather than nothing at all. */
bool e1000_knows(u16 device_id) { return look_up(device_id) != NULL; }

static bool e1000_send(const void *frame, u32 len)
{
    if (!up || len > BUF_SIZE) return false;

    tx_desc *d = &tx_ring[tx_next];
    if (!(d->status & DD)) return false;     /* ring full; caller retries */

    const u8 *s = (const u8 *)frame;
    for (u32 i = 0; i < len; i++) tx_buf[tx_next][i] = s[i];

    d->length = (u16)len;
    d->cmd = TX_CMD_EOP | TX_CMD_IFCS | TX_CMD_RS;
    d->status = 0;

    tx_next = (tx_next + 1) % TX_COUNT;
    wr(R_TDT, tx_next);
    return true;
}

static i32 e1000_recv(void *out, u32 max)
{
    if (!up) return -1;

    rx_desc *d = &rx_ring[rx_next];
    if (!(d->status & DD)) return -1;

    u32 len = d->length;
    if (len > max) len = max;
    u8 *dst = (u8 *)out;
    for (u32 i = 0; i < len; i++) dst[i] = rx_buf[rx_next][i];

    d->status = 0;
    wr(R_RDT, rx_next);
    rx_next = (rx_next + 1) % RX_COUNT;
    return (i32)len;
}
