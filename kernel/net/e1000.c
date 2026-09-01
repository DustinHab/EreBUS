/*
 * e1000.c -- the network card, an Intel 82540EM.
 *
 * The card is two rings of descriptors in memory: one it fills with
 * frames from the wire, one it drains onto it. The driver hands over
 * physical buffers, moves two tail pointers, and that is the whole
 * conversation. No interrupts are taken: the one thread that speaks
 * to the network polls between its own waits, so a quiet card costs a
 * register read and nothing else.
 *
 * Everything here is deliberately below the protocols: this file
 * knows frames, not addresses. What travels in them is net.c's
 * business.
 */
#include <eb/net.h>
#include <eb/pci.h>
#include <eb/pmm.h>
#include <eb/vmm.h>
#include <eb/mm.h>
#include <eb/io.h>
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
#define R_RAL0    0x5400
#define R_RAH0    0x5404
#define R_MTA     0x5200

#define CTRL_SLU     (1u << 6)      /* set link up */
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

static u32 rr(u32 off)          { return *(volatile u32 *)(regs + off); }
static void wr(u32 off, u32 v)  { *(volatile u32 *)(regs + off) = v; }

static const u8 *e1000_get_mac(void) { return mac; }

static bool e1000_send(const void *frame, u32 len);
static i32  e1000_recv(void *out, u32 max);

static const nic_ops e1000_ops = {
    .name = "e1000",
    .mac  = e1000_get_mac,
    .send = e1000_send,
    .recv = e1000_recv,
};

/* One word of the EEPROM, for cards whose address registers came up
 * empty. QEMU fills them in, so this is the road not usually taken. */
static u16 eeprom_read(u32 addr)
{
    wr(R_EERD, 1u | (addr << 8));
    for (u32 i = 0; i < 100000; i++) {
        u32 v = rr(R_EERD);
        if (v & (1u << 4)) return (u16)(v >> 16);
    }
    return 0;
}

bool e1000_init(void)
{
    const pci_device *dev = pci_find(0x02, 0x00, 0x00);
    if (!dev || dev->vendor != 0x8086) return false;

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

    wr(R_IMC, 0xFFFFFFFFu);         /* no interrupts; this is a polled card */
    (void)rr(R_ICR);

    /* The address the card answers to. */
    u32 ral = rr(R_RAL0), rah = rr(R_RAH0);
    if (ral == 0) {
        u16 w0 = eeprom_read(0), w1 = eeprom_read(1), w2 = eeprom_read(2);
        ral = (u32)w0 | ((u32)w1 << 16);
        rah = (u32)w2;
        wr(R_RAL0, ral);
        wr(R_RAH0, rah | (1u << 31));
    }
    mac[0] = (u8)ral; mac[1] = (u8)(ral >> 8);
    mac[2] = (u8)(ral >> 16); mac[3] = (u8)(ral >> 24);
    mac[4] = (u8)rah; mac[5] = (u8)(rah >> 8);

    for (u32 i = 0; i < 128; i++) wr(R_MTA + i * 4, 0);

    /* The receive ring: a page of descriptors, each with its own
     * buffer. The card owns everything between head and tail. */
    phys_addr rxp = pmm_alloc();
    phys_addr txp = pmm_alloc();
    if (rxp == PMM_NO_FRAME || txp == PMM_NO_FRAME) return false;
    rx_ring = (rx_desc *)phys_to_virt(rxp);
    tx_ring = (tx_desc *)phys_to_virt(txp);

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
    wr(R_CTRL, rr(R_CTRL) | CTRL_SLU);

    kprintf("net:  e1000 at %02x:%02x.%u, %02x:%02x:%02x:%02x:%02x:%02x\n",
            dev->bus, dev->device, dev->function,
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    up = true;
    nic_register(&e1000_ops);
    return true;
}

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
