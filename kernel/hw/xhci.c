/*
 * xhci.c -- USB keyboards and mice, through the host controller.
 *
 * The PS/2 driver reaches a USB keyboard only where the firmware
 * pretends there is a PS/2 one, and a machine booted the UEFI way
 * often stops pretending the moment the loader hands over. This is
 * the real thing: the xHCI controller on the PCI bus, its rings in
 * memory, and the devices at its root ports set up far enough to
 * speak the boot protocol -- eight bytes per keyboard report, three
 * or four per mouse report.
 *
 * The controller is driven by rings of 16-byte blocks: a command
 * ring we write and it reads, an event ring it writes and we read,
 * and a transfer ring per endpoint. Nothing here uses an interrupt;
 * a thread reads the event ring every few milliseconds, which is
 * more often than a finger moves. The reports are turned into the
 * bytes a PS/2 keyboard would have sent and handed to the PS/2
 * driver's queues, so the layout tables and the modifier rules live
 * once, and the shell never learns which wire a key came down.
 *
 * What is not here, and said so: devices behind external hubs, USB
 * disks, isochronous anything. A hub is seen and named; a disk is
 * seen and named; neither is driven.
 */
#include <eb/xhci.h>
#include <eb/pci.h>
#include <eb/pmm.h>
#include <eb/vmm.h>
#include <eb/mm.h>
#include <eb/ps2.h>
#include <eb/thread.h>
#include <eb/time.h>
#include <eb/fmt.h>
#include <eb/string.h>
#include <eb/journal.h>

/* ------------------------------------------------------------------ */
/* Registers                                                           */
/* ------------------------------------------------------------------ */

typedef volatile u32 reg32;

static u8    *cap;                    /* capability registers */
static reg32 *op;                     /* operational */
static reg32 *rt;                     /* runtime */
static reg32 *db;                     /* doorbells */
static u32    max_slots, max_ports, csz;   /* csz: a context is 32 or 64 bytes */
static bool   present;

#define OP_USBCMD    0
#define OP_USBSTS    1
#define OP_CRCR_LO   6
#define OP_CRCR_HI   7
#define OP_DCBAAP_LO 12
#define OP_DCBAAP_HI 13
#define OP_CONFIG    14
#define OP_PORTSC(p) (0x100 + 4 * ((p) - 1))     /* 0x400 + 0x10 per port, in dwords */

#define CMD_RUN      (1u << 0)
#define CMD_RESET    (1u << 1)
#define STS_HALTED   (1u << 0)
#define STS_NOT_READY (1u << 11)

#define PORT_CONNECTED (1u << 0)
#define PORT_ENABLED   (1u << 1)
#define PORT_RESET     (1u << 4)
#define PORT_POWER     (1u << 9)
#define PORT_CHANGES   ((1u << 17) | (1u << 18) | (1u << 19) | (1u << 20) | (1u << 21) | (1u << 22))
#define PORT_SPEED(v)  (((v) >> 10) & 0xF)

/* interrupter 0, in dwords from the runtime base */
#define RT_IMAN    8
#define RT_ERSTSZ  10
#define RT_ERSTBA_LO 12
#define RT_ERSTBA_HI 13
#define RT_ERDP_LO 14
#define RT_ERDP_HI 15

/* ------------------------------------------------------------------ */
/* Blocks and rings                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    u64 param;
    u32 status;
    u32 control;
} trb;

#define TRB_TYPE(t)    ((u32)(t) << 10)
#define TRB_CYCLE      (1u << 0)
#define TRB_TOGGLE     (1u << 1)
#define TRB_ISP        (1u << 2)
#define TRB_IOC        (1u << 5)
#define TRB_IDT        (1u << 6)
#define TRB_DIR_IN     (1u << 16)
#define TRB_SLOT(s)    ((u32)(s) << 24)

#define T_NORMAL   1
#define T_SETUP    2
#define T_DATA     3
#define T_STATUS   4
#define T_LINK     6
#define T_ENABLE_SLOT     9
#define T_DISABLE_SLOT    10
#define T_ADDRESS_DEVICE  11
#define T_CONFIGURE_EP    12
#define T_EVALUATE_CTX    13
#define T_TRANSFER_EVENT  32
#define T_COMMAND_EVENT   33
#define T_PORT_EVENT      34

#define RING_TRBS 256                 /* one page */

typedef struct {
    trb      *t;
    phys_addr pa;
    u32       enq;
    u32       cycle;
} ring;

static ring cmd;
static trb *ev;
static phys_addr ev_pa;
static u32  ev_deq, ev_cycle;
static u64 *dcbaa;
static phys_addr dcbaa_pa;

static bool ring_make(ring *r)
{
    phys_addr p = pmm_alloc();
    if (p == PMM_NO_FRAME) return false;
    r->pa = p;
    r->t = (trb *)phys_to_virt(p);
    memset(r->t, 0, PAGE_SIZE);
    r->enq = 0;
    r->cycle = 1;
    /* the last block points back at the first and flips the cycle */
    r->t[RING_TRBS - 1].param = p;
    r->t[RING_TRBS - 1].control = TRB_TYPE(T_LINK) | TRB_TOGGLE;
    return true;
}

/* Puts one block on a ring; answers where it lies, for matching the
 * event that will name it. */
static phys_addr ring_put(ring *r, u64 param, u32 status, u32 control)
{
    trb *t = &r->t[r->enq];
    t->param = param;
    t->status = status;
    t->control = (control & ~TRB_CYCLE) | (r->cycle ? TRB_CYCLE : 0);
    phys_addr at = r->pa + (phys_addr)r->enq * sizeof(trb);
    r->enq++;
    if (r->enq == RING_TRBS - 1) {
        trb *link = &r->t[RING_TRBS - 1];
        link->control = (link->control & ~TRB_CYCLE) | (r->cycle ? TRB_CYCLE : 0);
        r->enq = 0;
        r->cycle ^= 1;
    }
    return at;
}

/* ------------------------------------------------------------------ */
/* Devices                                                             */
/* ------------------------------------------------------------------ */

#define DEV_MAX 8
#define KIND_NONE 0
#define KIND_KEYBOARD 1
#define KIND_MOUSE 2
#define KIND_HUB 3
#define KIND_OTHER 4

typedef struct {
    bool used;
    u32  slot, port, speed;
    u8  *dctx; phys_addr dctx_pa;    /* the device context, the controller's */
    u8  *ictx; phys_addr ictx_pa;    /* the input context, ours to fill */
    ring ep0;                         /* control endpoint */
    ring intr;                        /* the interrupt endpoint, for reports */
    u32  intr_dci;                    /* its number in the device context */
    u32  report_len;
    u8  *buf; phys_addr buf_pa;      /* one page of transfer memory */
    u8   kind;
    u8   iface;
    char name[32];
    u8   last[8];                     /* the previous keyboard report */
    u8   held;                        /* the key held for repeating, as a usage */
    u64  held_since, last_repeat;
} usbdev;

static usbdev dev[DEV_MAX];
static u32 keyboards, mice;

/* What the last events said, for whoever is waiting on them. */
static struct {
    bool have;
    phys_addr trb;
    u32 code, slot;
} last_command;

static struct {
    bool have;
    phys_addr trb;
    u32 code, slot, dci, residue;
} last_transfer;

static usbdev *dev_by_slot(u32 slot)
{
    for (u32 i = 0; i < DEV_MAX; i++)
        if (dev[i].used && dev[i].slot == slot) return &dev[i];
    return NULL;
}

static void handle_report(usbdev *d, u32 len);
static void queue_report(usbdev *d);

/* ------------------------------------------------------------------ */
/* The event ring                                                      */
/* ------------------------------------------------------------------ */

static void process_events(void)
{
    for (;;) {
        trb *e = &ev[ev_deq];
        if ((e->control & TRB_CYCLE) != (ev_cycle ? TRB_CYCLE : 0)) break;
        u32 type = (e->control >> 10) & 0x3F;
        u32 code = e->status >> 24;
        u32 slot = e->control >> 24;

        if (type == T_COMMAND_EVENT) {
            last_command.have = true;
            last_command.trb = e->param;
            last_command.code = code;
            last_command.slot = slot;
        } else if (type == T_TRANSFER_EVENT) {
            last_transfer.have = true;
            last_transfer.trb = e->param;
            last_transfer.code = code;
            last_transfer.slot = slot;
            last_transfer.dci = (e->control >> 16) & 0x1F;
            last_transfer.residue = e->status & 0xFFFFFF;
            usbdev *d = dev_by_slot(slot);
            if (d && d->kind != KIND_NONE && last_transfer.dci == d->intr_dci) {
                if (code == 1 || code == 13)
                    handle_report(d, d->report_len - last_transfer.residue);
                queue_report(d);
            }
        }
        /* port changes are read off the ports themselves, later */

        ev_deq++;
        if (ev_deq == RING_TRBS) { ev_deq = 0; ev_cycle ^= 1; }
    }
    phys_addr deq = ev_pa + (phys_addr)ev_deq * sizeof(trb);
    rt[RT_ERDP_LO] = (u32)deq | (1u << 3);            /* and the busy bit cleared */
    rt[RT_ERDP_HI] = (u32)(deq >> 32);
}

static bool wait_ns(u64 ns, bool (*done)(void))
{
    u64 since = time_ns();
    while (time_ns() - since < ns) {
        process_events();
        if (done()) return true;
        sched_yield();
    }
    process_events();
    return done();
}

static phys_addr waiting_for;
static bool command_done(void)  { return last_command.have && last_command.trb == waiting_for; }
static bool transfer_done(void) { return last_transfer.have && last_transfer.trb == waiting_for; }

/* A command, and its completion code; zero when nothing came back. */
static u32 command(u64 param, u32 control, u32 *slot_out)
{
    last_command.have = false;
    waiting_for = ring_put(&cmd, param, 0, control);
    db[0] = 0;
    if (!wait_ns(1000000000ULL, command_done)) return 0;
    if (slot_out) *slot_out = last_command.slot;
    return last_command.code;
}

/* ------------------------------------------------------------------ */
/* Control transfers                                                   */
/* ------------------------------------------------------------------ */

static bool control(usbdev *d, u8 rtype, u8 req, u16 value, u16 index,
                    u16 length, phys_addr data)
{
    bool in = (rtype & 0x80) != 0;
    u64 setup = (u64)rtype | ((u64)req << 8) | ((u64)value << 16) |
                ((u64)index << 32) | ((u64)length << 48);
    u32 trt = length ? (in ? 3u : 2u) : 0;
    ring_put(&d->ep0, setup, 8, TRB_TYPE(T_SETUP) | TRB_IDT | (trt << 16));
    if (length)
        ring_put(&d->ep0, data, length, TRB_TYPE(T_DATA) | (in ? TRB_DIR_IN : 0));
    last_transfer.have = false;
    waiting_for = ring_put(&d->ep0, 0, 0,
                           TRB_TYPE(T_STATUS) | TRB_IOC | ((length && in) ? 0 : TRB_DIR_IN));
    db[d->slot] = 1;
    if (!wait_ns(1000000000ULL, transfer_done)) return false;
    return last_transfer.code == 1 || last_transfer.code == 13;
}

/* ------------------------------------------------------------------ */
/* Contexts                                                            */
/* ------------------------------------------------------------------ */

static u32 *ictx_at(usbdev *d, u32 i) { return (u32 *)(d->ictx + i * csz); }

static u32 ep0_packet(u32 speed)
{
    switch (speed) {
    case 3:  return 64;               /* high */
    case 4:  return 512;              /* super */
    default: return 8;                /* low, full */
    }
}

/* ------------------------------------------------------------------ */
/* Bringing a device up                                                */
/* ------------------------------------------------------------------ */

static bool port_reset(u32 port)
{
    reg32 *sc = &op[OP_PORTSC(port)];
    *sc = PORT_POWER | PORT_RESET;
    u64 since = time_ns();
    while (time_ns() - since < 500000000ULL) {
        if (!(*sc & PORT_RESET) && (*sc & PORT_ENABLED)) break;
        sched_yield();
    }
    *sc = PORT_POWER | PORT_CHANGES;              /* the change bits, acknowledged */
    return (*sc & PORT_ENABLED) != 0;
}

static usbdev *dev_new(u32 port)
{
    usbdev *d = NULL;
    for (u32 i = 0; i < DEV_MAX && !d; i++) if (!dev[i].used) d = &dev[i];
    if (!d) return NULL;
    memset(d, 0, sizeof(*d));
    phys_addr a = pmm_alloc(), b = pmm_alloc(), c = pmm_alloc();
    if (a == PMM_NO_FRAME || b == PMM_NO_FRAME || c == PMM_NO_FRAME) return NULL;
    if (!ring_make(&d->ep0) || !ring_make(&d->intr)) return NULL;
    d->dctx_pa = a; d->dctx = (u8 *)phys_to_virt(a); memset(d->dctx, 0, PAGE_SIZE);
    d->ictx_pa = b; d->ictx = (u8 *)phys_to_virt(b); memset(d->ictx, 0, PAGE_SIZE);
    d->buf_pa = c;  d->buf = (u8 *)phys_to_virt(c);  memset(d->buf, 0, PAGE_SIZE);
    d->port = port;
    d->used = true;
    return d;
}

static void name_from(usbdev *d, const u8 *desc, u32 len)
{
    /* a string descriptor: utf-16, two bytes of header */
    u32 n = 0;
    for (u32 i = 2; i + 1 < len && i < desc[0] && n < sizeof(d->name) - 1; i += 2) {
        u8 c = desc[i];
        d->name[n++] = (c >= 0x20 && c < 0x7F && desc[i + 1] == 0) ? (char)c : '?';
    }
    d->name[n] = 0;
}

static bool device_up(u32 port)
{
    u32 sc = op[OP_PORTSC(port)];
    u32 speed = PORT_SPEED(sc);

    u32 slot = 0;
    if (command(0, TRB_TYPE(T_ENABLE_SLOT), &slot) != 1 || slot == 0 || slot > max_slots) {
        kprintf("usb:  port %u: the controller gave no slot\n", port);
        return false;
    }
    usbdev *d = dev_new(port);
    if (!d) { kprintf("usb:  port %u: no room for another device\n", port); return false; }
    d->slot = slot;
    d->speed = speed;
    dcbaa[slot] = d->dctx_pa;

    /* The input context: the slot, and endpoint zero. */
    u32 *icc = ictx_at(d, 0);
    icc[1] = 0x3;                                  /* slot and endpoint zero */
    u32 *sl = ictx_at(d, 1);
    sl[0] = (1u << 27) | (speed << 20);           /* one context entry, the speed */
    sl[1] = port << 16;                            /* the root port */
    u32 *e0 = ictx_at(d, 2);
    e0[1] = (4u << 3) | (ep0_packet(speed) << 16) | (3u << 1);   /* control, packet, three tries */
    e0[2] = (u32)d->ep0.pa | 1;
    e0[3] = (u32)(d->ep0.pa >> 32);
    e0[4] = 8;

    if (command(d->ictx_pa, TRB_TYPE(T_ADDRESS_DEVICE) | TRB_SLOT(slot), NULL) != 1) {
        kprintf("usb:  port %u: the device took no address\n", port);
        return false;
    }

    /* Who it is: the device descriptor, first eight bytes for the
     * packet size of endpoint zero, then all of it. */
    if (!control(d, 0x80, 6, 0x0100, 0, 8, d->buf_pa)) {
        kprintf("usb:  port %u: no answer to the first question\n", port);
        return false;
    }
    if (speed < 3 && d->buf[7] != ep0_packet(speed)) {
        icc[1] = 0x2;
        e0[1] = (4u << 3) | ((u32)d->buf[7] << 16) | (3u << 1);
        command(d->ictx_pa, TRB_TYPE(T_EVALUATE_CTX) | TRB_SLOT(slot), NULL);
    }
    if (!control(d, 0x80, 6, 0x0100, 0, 18, d->buf_pa)) return false;
    u8 dclass = d->buf[4];
    u8 iproduct = d->buf[15];
    u16 vendor = (u16)(d->buf[8] | (d->buf[9] << 8));
    u16 product = (u16)(d->buf[10] | (d->buf[11] << 8));

    if (iproduct && control(d, 0x80, 6, (u16)(0x0300 | iproduct), 0x0409, 64, d->buf_pa + 256))
        name_from(d, d->buf + 256, 64);
    if (!d->name[0]) {
        const char *p = "a device";
        u32 n = 0;
        while (p[n]) { d->name[n] = p[n]; n++; }
        d->name[n] = 0;
    }

    if (dclass == 9) {
        d->kind = KIND_HUB;
        kprintf("usb:  port %u: %s (%04x:%04x), a hub; what hangs off it is not reached\n",
                port, d->name, vendor, product);
        return true;
    }

    /* Its configuration: nine bytes for the size, then the whole. */
    if (!control(d, 0x80, 6, 0x0200, 0, 9, d->buf_pa)) return false;
    u32 total = (u32)(d->buf[2] | (d->buf[3] << 8));
    if (total > 1024) total = 1024;
    if (!control(d, 0x80, 6, 0x0200, 0, (u16)total, d->buf_pa)) return false;
    u8 config_value = d->buf[5];

    /* Walk the interfaces for a boot keyboard or mouse and its
     * interrupt-in endpoint. */
    u32 ep_addr = 0, ep_packet = 0, ep_interval = 0;
    u8 kind = KIND_OTHER, iface = 0;
    bool in_iface = false;
    for (u32 i = 0; i + 1 < total; i += d->buf[i] ? d->buf[i] : 1) {
        u8 len = d->buf[i], type = d->buf[i + 1];
        if (type == 4 && len >= 9) {
            u8 cls = d->buf[i + 5], sub = d->buf[i + 6], proto = d->buf[i + 7];
            in_iface = false;
            if (kind == KIND_OTHER && cls == 3 && sub == 1 && (proto == 1 || proto == 2)) {
                kind = proto == 1 ? KIND_KEYBOARD : KIND_MOUSE;
                iface = d->buf[i + 2];
                in_iface = true;
            }
        } else if (type == 5 && len >= 7 && in_iface && !ep_addr) {
            if ((d->buf[i + 3] & 3) == 3 && (d->buf[i + 2] & 0x80)) {
                ep_addr = d->buf[i + 2];
                ep_packet = (u32)(d->buf[i + 4] | (d->buf[i + 5] << 8)) & 0x7FF;
                ep_interval = d->buf[i + 6];
            }
        }
    }
    d->iface = iface;

    if (kind == KIND_OTHER || !ep_addr) {
        d->kind = KIND_OTHER;
        kprintf("usb:  port %u: %s (%04x:%04x), class %u; not driven\n",
                port, d->name, vendor, product, dclass);
        return true;
    }

    /* The interrupt endpoint into the device context: type, packet,
     * how often to ask -- in units of 125 us, as a power of two. */
    u32 dci = ((ep_addr & 0xF) << 1) | 1;
    u32 interval;
    if (speed >= 3) {
        interval = ep_interval ? ep_interval - 1 : 0;
    } else {
        interval = 3;
        for (u32 v = ep_interval; v > 1; v >>= 1) interval++;
    }
    if (interval > 15) interval = 15;
    if (ep_packet > 64) ep_packet = 64;
    if (ep_packet == 0) ep_packet = 8;

    icc[1] = 0x1 | (1u << dci);
    icc[0] = 0;
    sl[0] = (sl[0] & ~(0x1Fu << 27)) | (dci << 27);
    u32 *ep = ictx_at(d, dci + 1);
    memset(ep, 0, csz);
    ep[0] = interval << 16;
    ep[1] = (7u << 3) | (ep_packet << 16) | (3u << 1);
    ep[2] = (u32)d->intr.pa | 1;
    ep[3] = (u32)(d->intr.pa >> 32);
    ep[4] = ep_packet | (ep_packet << 16);
    if (command(d->ictx_pa, TRB_TYPE(T_CONFIGURE_EP) | TRB_SLOT(slot), NULL) != 1) {
        kprintf("usb:  port %u: %s: the endpoint could not be set up\n", port, d->name);
        return false;
    }
    if (!control(d, 0x00, 9, config_value, 0, 0, 0)) return false;     /* set configuration */
    control(d, 0x21, 0x0B, 0, iface, 0, 0);                              /* boot protocol */
    control(d, 0x21, 0x0A, 0, iface, 0, 0);                              /* report only changes */

    d->kind = kind;
    d->intr_dci = dci;
    d->report_len = kind == KIND_KEYBOARD ? 8 : (ep_packet < 8 ? ep_packet : 8);
    if (kind == KIND_KEYBOARD) keyboards++; else mice++;
    kprintf("usb:  port %u: %s (%04x:%04x), a %s\n", port, d->name, vendor, product,
            kind == KIND_KEYBOARD ? "keyboard" : "mouse");
    queue_report(d);
    return true;
}

/* ------------------------------------------------------------------ */
/* Reports                                                             */
/* ------------------------------------------------------------------ */

static void queue_report(usbdev *d)
{
    ring_put(&d->intr, d->buf_pa + 512, d->report_len,
             TRB_TYPE(T_NORMAL) | TRB_IOC | TRB_ISP);
    db[d->slot] = d->intr_dci;
}

/* HID usage to scancode set 1. 0 for keys that have no PS/2 twin;
 * 0x80 marks the ones that travel behind the 0xE0 prefix. */
static const u8 usage_to_set1[0x66] = {
    0, 0, 0, 0,
    0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26,   /* a - l */
    0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D,   /* m - x */
    0x15, 0x2C,                                                             /* y z */
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,             /* 1 - 0 */
    0x1C, 0x01, 0x0E, 0x0F, 0x39,                                           /* enter esc bs tab space */
    0x0C, 0x0D, 0x1A, 0x1B, 0x2B, 0x2B, 0x27, 0x28, 0x29, 0x33, 0x34, 0x35, /* - = [ ] \ # ; ' ` , . / */
    0x3A,                                                                   /* caps lock */
    0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58, /* f1 - f12 */
    0, 0, 0,                                                                /* print scroll pause */
    0x80 | 0x52, 0x80 | 0x47, 0x80 | 0x49, 0x80 | 0x53, 0x80 | 0x4F, 0x80 | 0x51, /* ins home pgup del end pgdn */
    0x80 | 0x4D, 0x80 | 0x4B, 0x80 | 0x50, 0x80 | 0x48,                     /* right left down up */
    0x45, 0x80 | 0x35, 0x37, 0x4A, 0x4E, 0x80 | 0x1C,                       /* numlock / * - + enter */
    0x4F, 0x50, 0x51, 0x4B, 0x4C, 0x4D, 0x47, 0x48, 0x49, 0x52, 0x53,       /* keypad 1-9 0 . */
    0x56, 0                                                                 /* the key beside the left shift */
};

static void feed_usage(u8 usage, bool down)
{
    u8 code = usage < sizeof(usage_to_set1) ? usage_to_set1[usage] : 0;
    if (!code) return;
    if (code & 0x80) ps2_feed_scancode(0xE0);
    ps2_feed_scancode((u8)((code & 0x7F) | (down ? 0 : 0x80)));
}

/* Modifier bits of the first report byte, as set 1: left ctrl, left
 * shift, left alt, (left gui), right ctrl, right shift, right alt. */
static void feed_modifiers(u8 was, u8 now)
{
    static const u8 codes[8] = { 0x1D, 0x2A, 0x38, 0, 0x80 | 0x1D, 0x36, 0x80 | 0x38, 0 };
    for (u32 b = 0; b < 8; b++) {
        u8 m = (u8)(1u << b);
        if ((was & m) == (now & m) || !codes[b]) continue;
        bool down = (now & m) != 0;
        if (codes[b] & 0x80) ps2_feed_scancode(0xE0);
        ps2_feed_scancode((u8)((codes[b] & 0x7F) | (down ? 0 : 0x80)));
    }
}

static bool in_report(const u8 *r, u8 usage)
{
    for (u32 i = 2; i < 8; i++) if (r[i] == usage) return true;
    return false;
}

static void handle_report(usbdev *d, u32 len)
{
    const u8 *r = d->buf + 512;
    if (d->kind == KIND_MOUSE) {
        if (len < 3) return;
        i32 dx = (i8)r[1], dy = (i8)r[2], dz = len >= 4 ? (i8)r[3] : 0;
        /* the wheel counts away from the person; the page counts down */
        ps2_feed_mouse(dx, dy, -dz, r[0]);
        return;
    }
    if (len < 8) return;
    /* all six slots at one means more keys than the keyboard can tell apart */
    if (r[2] == 1 && r[3] == 1 && r[4] == 1) return;

    feed_modifiers(d->last[0], r[0]);
    for (u32 i = 2; i < 8; i++)
        if (d->last[i] && !in_report(r, d->last[i])) {
            feed_usage(d->last[i], false);
            if (d->held == d->last[i]) d->held = 0;
        }
    for (u32 i = 2; i < 8; i++)
        if (r[i] && !in_report(d->last, r[i])) {
            feed_usage(r[i], true);
            d->held = r[i];
            d->held_since = time_ns();
            d->last_repeat = d->held_since;
        }
    memcpy(d->last, r, 8);
}

/* A key held down repeats, as a PS/2 keyboard would have done on its
 * own: after half a second, thirty times a second. */
static void repeat_held(void)
{
    u64 now = time_ns();
    for (u32 i = 0; i < DEV_MAX; i++) {
        usbdev *d = &dev[i];
        if (!d->used || d->kind != KIND_KEYBOARD || !d->held) continue;
        if (now - d->held_since < 500000000ULL) continue;
        if (now - d->last_repeat < 33000000ULL) continue;
        d->last_repeat = now;
        feed_usage(d->held, true);
    }
}

/* ------------------------------------------------------------------ */
/* The thread                                                          */
/* ------------------------------------------------------------------ */

static bool port_seen[256];

static void look_at_ports(void)
{
    for (u32 p = 1; p <= max_ports && p < 256; p++) {
        u32 sc = op[OP_PORTSC(p)];
        bool connected = (sc & PORT_CONNECTED) != 0;
        if (connected && !port_seen[p]) {
            port_seen[p] = true;
            if (port_reset(p)) device_up(p);
            else kprintf("usb:  port %u: something is there, but it would not come up\n", p);
        } else if (!connected && port_seen[p]) {
            port_seen[p] = false;
            for (u32 i = 0; i < DEV_MAX; i++)
                if (dev[i].used && dev[i].port == p) {
                    if (dev[i].kind == KIND_KEYBOARD && keyboards) keyboards--;
                    if (dev[i].kind == KIND_MOUSE && mice) mice--;
                    kprintf("usb:  port %u: %s is gone\n", p, dev[i].name);
                    dev[i].used = false;         /* its memory stays; devices are few */
                    command(0, TRB_TYPE(T_DISABLE_SLOT) | TRB_SLOT(dev[i].slot), NULL);
                    dcbaa[dev[i].slot] = 0;
                }
            op[OP_PORTSC(p)] = PORT_POWER | PORT_CHANGES;
        }
    }
}

static void usb_thread(void *arg)
{
    (void)arg;
    u64 last_look = 0;
    for (;;) {
        process_events();
        repeat_held();
        u64 now = time_ns();
        if (now - last_look > 500000000ULL) {     /* twice a second: who came, who went */
            last_look = now;
            look_at_ports();
        }
        u64 since = time_ns();
        while (time_ns() - since < 4000000ULL) sched_yield();
    }
}

/* ------------------------------------------------------------------ */
/* Bringing the controller up                                          */
/* ------------------------------------------------------------------ */

static u32 cap32(u32 off) { return *(reg32 *)(cap + off); }

/* The firmware may still own the controller; asking for it is a
 * capability register with two bits, ours and theirs. */
static void take_from_firmware(void)
{
    u32 hcc = cap32(0x10);
    u32 off = (hcc >> 16) << 2;
    for (u32 guard = 0; off && guard < 64; guard++) {
        u32 v = cap32(off);
        if ((v & 0xFF) == 1) {                     /* usb legacy support */
            *(reg32 *)(cap + off) = v | (1u << 24);
            u64 since = time_ns();
            while ((cap32(off) & (1u << 16)) && time_ns() - since < 1000000000ULL) sched_yield();
            u32 ctl = cap32(off + 4);
            *(reg32 *)(cap + off + 4) = ctl & 0xE0000000u;   /* no more firmware interrupts */
            return;
        }
        u32 next = (v >> 8) & 0xFF;
        if (!next) break;
        off += next << 2;
    }
}

bool xhci_init(void)
{
    const pci_device *pd = pci_find(0x0C, 0x03, 0x30);
    if (!pd) { kprintf("usb:  no xhci controller on the bus\n"); return false; }

    u32 command_reg = pci_read32(pd, 0x04);
    pci_write32(pd, 0x04, command_reg | (1u << 1) | (1u << 2));

    phys_addr bar = pci_bar(pd, 0);
    if (!bar) { kprintf("usb:  the controller has no register window\n"); return false; }
    if (!vmm_map(vmm_kernel_pml4(), (virt_addr)phys_to_virt(bar), bar,
                 16 * PAGE_SIZE, PAGE_KERNEL_MMIO))
        return false;
    cap = (u8 *)phys_to_virt(bar);

    u32 caplen = cap32(0) & 0xFF;
    u32 hcs1 = cap32(0x04), hcs2 = cap32(0x08), hcc = cap32(0x10);
    max_slots = hcs1 & 0xFF;
    max_ports = hcs1 >> 24;
    csz = (hcc & (1u << 2)) ? 64 : 32;
    op = (reg32 *)(cap + caplen);
    rt = (reg32 *)(cap + (cap32(0x18) & ~0x1Fu));
    db = (reg32 *)(cap + (cap32(0x14) & ~0x3u));

    take_from_firmware();

    /* Stop, reset, wait until it is ready to be told anything. */
    op[OP_USBCMD] &= ~CMD_RUN;
    u64 since = time_ns();
    while (!(op[OP_USBSTS] & STS_HALTED) && time_ns() - since < 1000000000ULL) sched_yield();
    op[OP_USBCMD] |= CMD_RESET;
    since = time_ns();
    while ((op[OP_USBCMD] & CMD_RESET) && time_ns() - since < 1000000000ULL) sched_yield();
    since = time_ns();
    while ((op[OP_USBSTS] & STS_NOT_READY) && time_ns() - since < 1000000000ULL) sched_yield();
    if (op[OP_USBSTS] & STS_NOT_READY) {
        kprintf("usb:  the controller did not come out of reset\n");
        return false;
    }

    /* Memory: the slot table, the command ring, the event ring and its
     * one-entry table, and scratch pages if the controller wants them. */
    dcbaa_pa = pmm_alloc();
    phys_addr erst_pa = pmm_alloc();
    ev_pa = pmm_alloc();
    if (dcbaa_pa == PMM_NO_FRAME || erst_pa == PMM_NO_FRAME || ev_pa == PMM_NO_FRAME ||
        !ring_make(&cmd))
        return false;
    dcbaa = (u64 *)phys_to_virt(dcbaa_pa);
    memset(dcbaa, 0, PAGE_SIZE);
    ev = (trb *)phys_to_virt(ev_pa);
    memset(ev, 0, PAGE_SIZE);
    ev_deq = 0;
    ev_cycle = 1;

    u32 scratch = (((hcs2 >> 21) & 0x1F) << 5) | ((hcs2 >> 27) & 0x1F);
    if (scratch) {
        phys_addr arr = pmm_alloc();
        if (arr == PMM_NO_FRAME) return false;
        u64 *a = (u64 *)phys_to_virt(arr);
        memset(a, 0, PAGE_SIZE);
        for (u32 i = 0; i < scratch && i < PAGE_SIZE / 8; i++) {
            phys_addr pg = pmm_alloc();
            if (pg == PMM_NO_FRAME) return false;
            memset(phys_to_virt(pg), 0, PAGE_SIZE);
            a[i] = pg;
        }
        dcbaa[0] = arr;
    }

    op[OP_CONFIG] = max_slots;
    op[OP_DCBAAP_LO] = (u32)dcbaa_pa;
    op[OP_DCBAAP_HI] = (u32)(dcbaa_pa >> 32);
    op[OP_CRCR_LO] = (u32)cmd.pa | 1;
    op[OP_CRCR_HI] = (u32)(cmd.pa >> 32);

    u64 *erst = (u64 *)phys_to_virt(erst_pa);
    memset(erst, 0, PAGE_SIZE);
    erst[0] = ev_pa;
    erst[1] = RING_TRBS;
    rt[RT_ERSTSZ] = 1;
    rt[RT_ERDP_LO] = (u32)ev_pa;
    rt[RT_ERDP_HI] = (u32)(ev_pa >> 32);
    rt[RT_ERSTBA_LO] = (u32)erst_pa;
    rt[RT_ERSTBA_HI] = (u32)(erst_pa >> 32);
    rt[RT_IMAN] = 0;                              /* no interrupt; the thread reads */

    op[OP_USBCMD] |= CMD_RUN;
    since = time_ns();
    while ((op[OP_USBSTS] & STS_HALTED) && time_ns() - since < 1000000000ULL) sched_yield();
    present = !(op[OP_USBSTS] & STS_HALTED);
    if (!present) { kprintf("usb:  the controller would not run\n"); return false; }

    kprintf("usb:  xhci at %02x:%02x.%u, %u ports, %u slots, %u-byte contexts\n",
            pd->bus, pd->device, pd->function, max_ports, max_slots, csz);

    /* Give the ports a moment to show what is plugged in, then walk. */
    since = time_ns();
    while (time_ns() - since < 100000000ULL) sched_yield();
    look_at_ports();
    if (!keyboards && !mice) kprintf("usb:  no keyboard and no mouse at the root ports\n");

    thread_create("usb", usb_thread, NULL, thread_domain(sched_current()));
    return true;
}

bool xhci_present(void)   { return present; }
u32  xhci_keyboards(void) { return keyboards; }
u32  xhci_mice(void)      { return mice; }
