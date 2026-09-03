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
/* What survives a write to this register. The change bits clear when
 * written with a one and the enable bit disables the port when written
 * with a one, so a read-modify-write has to drop both and put back only
 * what it means: the power, and how the port may wake the machine. */
#define PORT_KEEP      (PORT_POWER | (7u << 25))

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

#define DEV_MAX 16
#define KIND_NONE 0
#define KIND_KEYBOARD 1
#define KIND_MOUSE 2
#define KIND_HUB 3
#define KIND_OTHER 4

/* One human input on a device: a keyboard, or a mouse.
 *
 * Not one per device, because they do not come one per device. The
 * ordinary wireless receiver is a single thing on a single port that
 * carries a keyboard and a mouse as two interfaces, each with its own
 * endpoint, and a driver that stops at the first one it recognises
 * finds the keyboard and never finds the pointer. So each of these has
 * its own transfer ring, its own place in the device context, and its
 * own memory of what was held down. */
#define IN_MAX 4

typedef struct {
    bool used;
    ring r;
    u32  dci;                         /* its number in the device context */
    u32  report_len;
    u32  at;                          /* where its reports land in the page */
    u8   kind;                        /* keyboard or mouse */
    u8   iface;
    u8   last[8];                     /* the previous keyboard report */
    u8   held;                        /* the key held for repeating, as a usage */
    u64  held_since, last_repeat;
} usbin;

typedef struct {
    bool used;
    u32  slot, port, speed;
    u8  *dctx; phys_addr dctx_pa;    /* the device context, the controller's */
    u8  *ictx; phys_addr ictx_pa;    /* the input context, ours to fill */
    ring ep0;                         /* control endpoint */
    usbin in[IN_MAX];
    u8  *buf; phys_addr buf_pa;      /* one page of transfer memory */
    u8   kind;
    char name[32];

    /* Where it hangs, for anything not plugged into the machine itself.
     * The route is the chain of hub ports the controller follows to
     * reach it; the parent is the nearest hub that does the translating
     * for a slow device on a fast wire. */
    u32  route, depth;
    u32  parent_slot, parent_port;
    u32  up_slot, up_port;           /* the hub this hangs off, and where */

    /* Only for hubs. */
    u32  hub_ports;
    bool hub_seen[16];
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

static void handle_report(usbdev *d, usbin *in, u32 len);
static void queue_report(usbdev *d, usbin *in);

static usbin *input_by_dci(usbdev *d, u32 dci)
{
    for (u32 i = 0; i < IN_MAX; i++)
        if (d->in[i].used && d->in[i].dci == dci) return &d->in[i];
    return NULL;
}

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
            usbin *in = d ? input_by_dci(d, last_transfer.dci) : NULL;
            if (in) {
                if (code == 1 || code == 13)
                    handle_report(d, in, in->report_len - last_transfer.residue);
                queue_report(d, in);
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

static void wait_ms(u64 ms)
{
    u64 since = time_ns();
    while (time_ns() - since < ms * 1000000ULL) { process_events(); sched_yield(); }
}

/* A port with no power reports nothing plugged into it, forever.
 *
 * Under emulation the ports come up powered and this never comes up.
 * A real controller that says it controls port power leaves them off
 * after a reset, and then the whole machine looks as though nobody
 * owns a keyboard. */
static void power_ports(void)
{
    bool any = false;
    for (u32 p = 1; p <= max_ports && p < 256; p++) {
        reg32 *sc = &op[OP_PORTSC(p)];
        u32 v = *sc;
        if (v & PORT_POWER) continue;
        /* Keep the change bits clear so this does not acknowledge
         * something that has not been looked at yet. */
        *sc = (v & PORT_KEEP) | PORT_POWER;
        any = true;
    }
    if (any) wait_ms(20);
}

static bool port_reset(u32 port)
{
    reg32 *sc = &op[OP_PORTSC(port)];

    /* A usb 3 port trains itself and is enabled by the time anyone
     * looks; resetting it again only takes it away for a moment. Only
     * the slower ports need to be told. */
    if (*sc & PORT_ENABLED) {
        *sc = (*sc & PORT_KEEP) | PORT_POWER | PORT_CHANGES;
        return true;
    }

    *sc = PORT_POWER | PORT_RESET;
    u64 since = time_ns();
    while (time_ns() - since < 500000000ULL) {
        if (!(*sc & PORT_RESET) && (*sc & PORT_ENABLED)) break;
        sched_yield();
    }
    *sc = PORT_POWER | PORT_CHANGES;              /* the change bits, acknowledged */
    if (!(*sc & PORT_ENABLED)) return false;
    wait_ms(10);                                  /* the recovery the standard asks for */
    return true;
}

/* Where a device hangs. At the root all of this is zero; behind a hub
 * the controller needs the whole chain to reach it. */
typedef struct {
    u32 root_port;
    u32 route, depth;
    u32 speed;
    u32 up_slot, up_port;    /* the hub it is plugged into */
    u32 tt_slot, tt_port;    /* the hub that speaks for it, if it is slow */
} where;

static usbdev *dev_new(const where *w)
{
    usbdev *d = NULL;
    for (u32 i = 0; i < DEV_MAX && !d; i++) if (!dev[i].used) d = &dev[i];
    if (!d) return NULL;
    memset(d, 0, sizeof(*d));
    phys_addr a = pmm_alloc(), b = pmm_alloc(), c = pmm_alloc();
    if (a == PMM_NO_FRAME || b == PMM_NO_FRAME || c == PMM_NO_FRAME) return NULL;
    if (!ring_make(&d->ep0)) return NULL;
    d->dctx_pa = a; d->dctx = (u8 *)phys_to_virt(a); memset(d->dctx, 0, PAGE_SIZE);
    d->ictx_pa = b; d->ictx = (u8 *)phys_to_virt(b); memset(d->ictx, 0, PAGE_SIZE);
    d->buf_pa = c;  d->buf = (u8 *)phys_to_virt(c);  memset(d->buf, 0, PAGE_SIZE);
    d->port  = w->root_port;
    d->route = w->route;
    d->depth = w->depth;
    d->speed = w->speed;
    d->parent_slot = w->tt_slot;
    d->parent_port = w->tt_port;
    d->up_slot = w->up_slot;
    d->up_port = w->up_port;
    d->used = true;
    return d;
}

/* How a device is described in a sentence: which port of the machine,
 * and if it is not plugged into the machine, through what. */
static void say_where(const usbdev *d, char *out, u32 max)
{
    static const char digits[] = "0123456789";
    u32 n = 0;
    const char *p = "port ";
    while (*p && n + 1 < max) out[n++] = *p++;
    u32 v = d->port;
    if (v >= 10 && n + 1 < max) out[n++] = digits[(v / 10) % 10];
    if (n + 1 < max) out[n++] = digits[v % 10];
    if (d->depth) {
        p = ", through a hub on ";
        while (*p && n + 1 < max) out[n++] = *p++;
        v = d->up_port;
        if (v >= 10 && n + 1 < max) out[n++] = digits[(v / 10) % 10];
        if (n + 1 < max) out[n++] = digits[v % 10];
    }
    out[n] = 0;
}

static bool device_up(const where *w);

/* ------------------------------------------------------------------ */
/* Hubs                                                                */
/* ------------------------------------------------------------------ */
/*
 * A hub is a device that owns ports of its own, and on a desktop it is
 * where most of what a person touches actually hangs: the front panel,
 * the plug on a monitor, the little box on the desk. Reaching only the
 * ports on the machine itself finds the disk and misses the keyboard.
 *
 * There is no separate bus underneath. The controller addresses a
 * device behind a hub directly, given the chain of ports to walk, so
 * everything here is control transfers to the hub asking it to power a
 * port, to report what is on it, and to reset it -- and then the same
 * device_up as for a port on the machine.
 */

/* Two different numberings, and they must not be confused.
 *
 * A feature selector is what a request names -- port reset is 4, the
 * change bit for it is 20. The status the hub answers with puts the
 * same things at their own places: the state in the low half of a
 * word, what has changed since anyone asked in the high half, and
 * there the change bits start again from zero. */
#define HUB_PORT_CONNECT  0
#define HUB_PORT_ENABLE   1
#define HUB_PORT_RESET    4
#define HUB_PORT_POWER    8
#define HUB_C_CONNECT     16
#define HUB_C_RESET       20

#define ST_CONNECT   (1u << 0)
#define ST_ENABLE    (1u << 1)
#define ST_LOW       (1u << 9)
#define ST_HIGH      (1u << 10)
#define CH_CONNECT   (1u << 0)
#define CH_RESET     (1u << 4)

/* Unplugged: the counts come down, the slot goes back, and anything
 * that was hanging off it goes with it. */
static void device_gone(usbdev *d)
{
    for (u32 k = 0; k < IN_MAX; k++) {
        if (!d->in[k].used) continue;
        if (d->in[k].kind == KIND_KEYBOARD && keyboards) keyboards--;
        if (d->in[k].kind == KIND_MOUSE && mice) mice--;
    }
    if (d->kind == KIND_HUB)
        for (u32 i = 0; i < DEV_MAX; i++)
            if (dev[i].used && dev[i].up_slot == d->slot) device_gone(&dev[i]);

    kprintf("usb:  %s is gone\n", d->name[0] ? d->name : "a device");
    d->used = false;                 /* its memory stays; devices are few */
    command(0, TRB_TYPE(T_DISABLE_SLOT) | TRB_SLOT(d->slot), NULL);
    dcbaa[d->slot] = 0;
}

static bool hub_set(usbdev *d, u32 port, u16 feature)
{
    return control(d, 0x23, 3, feature, (u16)port, 0, 0);
}

static bool hub_clear(usbdev *d, u32 port, u16 feature)
{
    return control(d, 0x23, 1, feature, (u16)port, 0, 0);
}

/* Status is four bytes: what the port is now, and what has changed
 * since anyone last asked. */
static bool hub_status(usbdev *d, u32 port, u32 *out)
{
    if (!control(d, 0xA3, 0, 0, (u16)port, 4, d->buf_pa + 320)) return false;
    const u8 *s = d->buf + 320;
    *out = (u32)s[0] | ((u32)s[1] << 8) | ((u32)s[2] << 16) | ((u32)s[3] << 24);
    return true;
}

static void hub_up(usbdev *d)
{
    /* The hub descriptor, for the one number that matters: how many
     * ports it has. The type differs between the fast and the slow
     * kind, so ask for the one that fits the wire. */
    u16 type = d->speed >= 4 ? 0x2A00 : 0x2900;
    if (!control(d, 0xA0, 6, type, 0, 8, d->buf_pa + 320)) {
        kprintf("usb:  the hub would not say how many ports it has\n");
        return;
    }
    u32 ports = d->buf[320 + 2];
    if (ports > 15) ports = 15;
    d->hub_ports = ports;

    /* The controller has to know it is a hub before it will address
     * anything behind it, and how many ports to expect. */
    u32 *icc = ictx_at(d, 0);
    u32 *sl  = ictx_at(d, 1);
    icc[0] = 0;
    icc[1] = 0x1;                                   /* the slot alone */
    sl[0] |= (1u << 26);                            /* it is a hub */
    sl[1] = (sl[1] & 0x00FFFFFFu) | (ports << 24);
    if (command(d->ictx_pa, TRB_TYPE(T_CONFIGURE_EP) | TRB_SLOT(d->slot), NULL) != 1) {
        kprintf("usb:  the controller would not accept the hub\n");
        return;
    }

    /* Power every port, then give the standard's settling time before
     * believing what they report. */
    for (u32 p = 1; p <= ports; p++) hub_set(d, p, HUB_PORT_POWER);
    wait_ms(120);
}

/* Looks at one hub's ports the way the root ports are looked at. */
static void look_at_hub(usbdev *h)
{
    for (u32 p = 1; p <= h->hub_ports; p++) {
        u32 st;
        if (!hub_status(h, p, &st)) return;
        bool connected = (st & ST_CONNECT) != 0;

        if ((st >> 16) & CH_CONNECT) hub_clear(h, p, HUB_C_CONNECT);

        if (connected && !h->hub_seen[p]) {
            h->hub_seen[p] = true;
            /* Something just plugged in needs a moment before it can
             * answer for itself. */
            wait_ms(100);
            if (!hub_set(h, p, HUB_PORT_RESET)) continue;
            /* The hub resets on its own time and says when it is done. */
            bool done = false;
            for (u32 tries = 0; tries < 20 && !done; tries++) {
                wait_ms(20);
                if (!hub_status(h, p, &st)) break;
                done = ((st >> 16) & CH_RESET) != 0;
            }
            hub_clear(h, p, HUB_C_RESET);
            if (!done || !(st & ST_ENABLE)) {
                kprintf("usb:  port %u of a hub: something is there, "
                        "but it would not come up\n", p);
                continue;
            }
            wait_ms(20);

            /* Which of the wires it turned out to be. A hub on the fast
             * wire says so in the status; one that is itself on the
             * fastest wire has nothing slower behind it. */
            u32 speed = 1;                                   /* full */
            if (st & ST_LOW)  speed = 2;
            if (st & ST_HIGH) speed = 3;
            if (h->speed >= 4) speed = 4;

            where w;
            memset(&w, 0, sizeof(w));
            w.root_port = h->port;
            w.depth = h->depth + 1;
            w.route = h->route | ((p & 0xFu) << (4 * h->depth));
            w.speed = speed;
            w.up_slot = h->slot;
            w.up_port = p;
            /* Whoever translates for a slow device: this hub if it is
             * the fast one it is plugged into, otherwise whatever was
             * already translating further up. */
            if (speed < 3 && h->speed == 3) {
                w.tt_slot = h->slot;
                w.tt_port = p;
            } else {
                w.tt_slot = h->parent_slot;
                w.tt_port = h->parent_port;
            }
            if (w.depth <= 4) device_up(&w);
        } else if (!connected && h->hub_seen[p]) {
            h->hub_seen[p] = false;
            for (u32 i = 0; i < DEV_MAX; i++)
                if (dev[i].used && dev[i].up_slot == h->slot && dev[i].up_port == p)
                    device_gone(&dev[i]);
        }
    }
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

static bool device_up(const where *w)
{
    u32 speed = w->speed;
    u32 port = w->root_port;

    u32 slot = 0;
    if (command(0, TRB_TYPE(T_ENABLE_SLOT), &slot) != 1 || slot == 0 || slot > max_slots) {
        kprintf("usb:  port %u: the controller gave no slot\n", port);
        return false;
    }
    usbdev *d = dev_new(w);
    if (!d) { kprintf("usb:  port %u: no room for another device\n", port); return false; }
    d->slot = slot;
    dcbaa[slot] = d->dctx_pa;

    /* The input context: the slot, and endpoint zero. */
    u32 *icc = ictx_at(d, 0);
    icc[1] = 0x3;                                  /* slot and endpoint zero */
    u32 *sl = ictx_at(d, 1);
    sl[0] = (1u << 27) | (speed << 20) | (w->route & 0xFFFFFu);
    sl[1] = port << 16;                            /* the root port the chain starts at */
    /* A low or full speed device on a high speed wire does not speak
     * for itself: the nearest hub translates, and the controller has to
     * be told which hub and which of its ports. */
    sl[2] = (w->tt_slot & 0xFFu) | ((w->tt_port & 0xFFu) << 8);
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

    char at[48];
    say_where(d, at, sizeof(at));

    /* Its configuration: nine bytes for the size, then the whole. */
    if (!control(d, 0x80, 6, 0x0200, 0, 9, d->buf_pa)) return false;
    u32 total = (u32)(d->buf[2] | (d->buf[3] << 8));
    if (total > 1024) total = 1024;
    if (!control(d, 0x80, 6, 0x0200, 0, (u16)total, d->buf_pa)) return false;
    u8 config_value = d->buf[5];

    /* Walk every interface, not the first one that matches.
     *
     * The device class alone decides nothing: a receiver carrying a
     * keyboard and a mouse calls itself class zero, which means only
     * that the answer is further in. So each interface is looked at,
     * and each one that is a keyboard or a pointer gets its own
     * endpoint set up. */
    struct { u8 kind, iface, addr; u32 packet, interval; } found[IN_MAX];
    u32 nfound = 0;
    u8 seen_class[8]; u32 nclass = 0;
    u8 cur_kind = 0, cur_iface = 0;

    for (u32 i = 0; i + 1 < total; i += d->buf[i] ? d->buf[i] : 1) {
        u8 len = d->buf[i], type = d->buf[i + 1];
        if (type == 4 && len >= 9) {
            u8 cls = d->buf[i + 5], sub = d->buf[i + 6], proto = d->buf[i + 7];
            cur_kind = 0;

            bool known = false;
            for (u32 k = 0; k < nclass; k++) if (seen_class[k] == cls) known = true;
            if (!known && nclass < 8) seen_class[nclass++] = cls;

            /* A human interface device says which of the two it is in
             * its protocol byte. The boot subclass is what promises the
             * fixed report layout, but plenty of keyboards send that
             * layout while claiming no subclass at all, so the protocol
             * is trusted and the promise is merely asked for later. */
            if (cls == 3 && (proto == 1 || proto == 2)) {
                cur_kind = proto == 1 ? KIND_KEYBOARD : KIND_MOUSE;
                cur_iface = d->buf[i + 2];
                (void)sub;
            }
        } else if (type == 5 && len >= 7 && cur_kind && nfound < IN_MAX) {
            /* interrupt, and inbound */
            if ((d->buf[i + 3] & 3) == 3 && (d->buf[i + 2] & 0x80)) {
                found[nfound].kind = cur_kind;
                found[nfound].iface = cur_iface;
                found[nfound].addr = d->buf[i + 2];
                found[nfound].packet = (u32)(d->buf[i + 4] | (d->buf[i + 5] << 8)) & 0x7FF;
                found[nfound].interval = d->buf[i + 6];
                nfound++;
                cur_kind = 0;                  /* one endpoint per interface */
            }
        }
    }

    if (dclass == 9 || (nclass == 1 && seen_class[0] == 9)) {
        if (!control(d, 0x00, 9, config_value, 0, 0, 0)) return false;
        d->kind = KIND_HUB;
        kprintf("usb:  %s: %s (%04x:%04x), a hub\n", at, d->name, vendor, product);
        hub_up(d);
        return true;
    }

    if (!nfound) {
        d->kind = KIND_OTHER;
        kprintf("usb:  %s: %s (%04x:%04x), device class %u, interface class",
                at, d->name, vendor, product, dclass);
        for (u32 k = 0; k < nclass; k++) kprintf(" %u", seen_class[k]);
        if (!nclass) kprintf(" none");
        kprintf("; nothing here to type or point with\n");
        return true;
    }

    /* Every endpoint found goes into one configure, because the
     * controller takes the whole picture at once: the last context in
     * use has to name the highest of them, and a second configure that
     * mentioned only the second endpoint would drop the first. */
    u32 top = 1;
    icc[0] = 0;
    icc[1] = 0x1;
    for (u32 k = 0; k < nfound; k++) {
        u32 dci = ((found[k].addr & 0xF) << 1) | 1;
        usbin *in = &d->in[k];
        if (!ring_make(&in->r)) return false;

        u32 interval;
        if (speed >= 3) {
            interval = found[k].interval ? found[k].interval - 1 : 0;
        } else {
            /* the slower wires count in milliseconds, the controller in
             * eighths of one, as a power of two */
            interval = 3;
            for (u32 v = found[k].interval; v > 1; v >>= 1) interval++;
        }
        if (interval > 15) interval = 15;
        u32 packet = found[k].packet;
        if (packet > 64) packet = 64;
        if (packet == 0) packet = 8;

        icc[1] |= (1u << dci);
        if (dci > top) top = dci;

        u32 *ep = ictx_at(d, dci + 1);
        memset(ep, 0, csz);
        ep[0] = interval << 16;
        ep[1] = (7u << 3) | (packet << 16) | (3u << 1);   /* interrupt in */
        ep[2] = (u32)in->r.pa | 1;
        ep[3] = (u32)(in->r.pa >> 32);
        ep[4] = packet | (packet << 16);

        in->used = true;
        in->dci = dci;
        in->kind = found[k].kind;
        in->iface = found[k].iface;
        in->at = 512 + k * 64;
        in->report_len = found[k].kind == KIND_KEYBOARD
                       ? 8 : (packet < 8 ? packet : 8);
    }
    sl[0] = (sl[0] & ~(0x1Fu << 27)) | (top << 27);

    if (command(d->ictx_pa, TRB_TYPE(T_CONFIGURE_EP) | TRB_SLOT(slot), NULL) != 1) {
        kprintf("usb:  %s: %s: the endpoints could not be set up\n", at, d->name);
        return false;
    }
    if (!control(d, 0x00, 9, config_value, 0, 0, 0)) return false;     /* set configuration */

    d->kind = nfound == 1 ? d->in[0].kind : KIND_OTHER;
    for (u32 k = 0; k < nfound; k++) {
        usbin *in = &d->in[k];
        /* Ask for the fixed layout and for silence while nothing
         * changes. Neither is required of the device, and a device that
         * refuses is still read -- it simply keeps its own layout,
         * which for keyboards and mice is almost always the same one. */
        control(d, 0x21, 0x0B, 0, in->iface, 0, 0);
        control(d, 0x21, 0x0A, 0, in->iface, 0, 0);
        if (in->kind == KIND_KEYBOARD) keyboards++; else mice++;
        queue_report(d, in);
    }

    if (nfound == 1) {
        kprintf("usb:  %s: %s (%04x:%04x), a %s\n", at, d->name, vendor, product,
                d->in[0].kind == KIND_KEYBOARD ? "keyboard" : "mouse");
    } else {
        kprintf("usb:  %s: %s (%04x:%04x), carrying", at, d->name, vendor, product);
        for (u32 k = 0; k < nfound; k++)
            kprintf("%s a %s", k ? " and" : "",
                    d->in[k].kind == KIND_KEYBOARD ? "keyboard" : "mouse");
        kprintf("\n");
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Reports                                                             */
/* ------------------------------------------------------------------ */

static void queue_report(usbdev *d, usbin *in)
{
    ring_put(&in->r, d->buf_pa + in->at, in->report_len,
             TRB_TYPE(T_NORMAL) | TRB_IOC | TRB_ISP);
    db[d->slot] = in->dci;
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

static void handle_report(usbdev *d, usbin *in, u32 len)
{
    const u8 *r = d->buf + in->at;
    if (in->kind == KIND_MOUSE) {
        if (len < 3) return;
        i32 dx = (i8)r[1], dy = (i8)r[2], dz = len >= 4 ? (i8)r[3] : 0;
        /* the wheel counts away from the person; the page counts down */
        ps2_feed_mouse(dx, dy, -dz, r[0]);
        return;
    }
    if (len < 8) return;
    /* all six slots at one means more keys than the keyboard can tell apart */
    if (r[2] == 1 && r[3] == 1 && r[4] == 1) return;

    feed_modifiers(in->last[0], r[0]);
    for (u32 i = 2; i < 8; i++)
        if (in->last[i] && !in_report(r, in->last[i])) {
            feed_usage(in->last[i], false);
            if (in->held == in->last[i]) in->held = 0;
        }
    for (u32 i = 2; i < 8; i++)
        if (r[i] && !in_report(in->last, r[i])) {
            feed_usage(r[i], true);
            in->held = r[i];
            in->held_since = time_ns();
            in->last_repeat = in->held_since;
        }
    memcpy(in->last, r, 8);
}

/* A key held down repeats, as a PS/2 keyboard would have done on its
 * own: after half a second, thirty times a second. */
static void repeat_held(void)
{
    u64 now = time_ns();
    for (u32 i = 0; i < DEV_MAX; i++) {
        usbdev *d = &dev[i];
        if (!d->used) continue;
        for (u32 k = 0; k < IN_MAX; k++) {
            usbin *in = &d->in[k];
            if (!in->used || in->kind != KIND_KEYBOARD || !in->held) continue;
            if (now - in->held_since < 500000000ULL) continue;
            if (now - in->last_repeat < 33000000ULL) continue;
            in->last_repeat = now;
            feed_usage(in->held, true);
        }
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
            if (port_reset(p)) {
                where w;
                memset(&w, 0, sizeof(w));
                w.root_port = p;
                w.speed = PORT_SPEED(op[OP_PORTSC(p)]);
                device_up(&w);
            } else {
                kprintf("usb:  port %u: something is there, but it would not come up\n", p);
            }
        } else if (!connected && port_seen[p]) {
            port_seen[p] = false;
            for (u32 i = 0; i < DEV_MAX; i++)
                if (dev[i].used && !dev[i].depth && dev[i].port == p)
                    device_gone(&dev[i]);
            op[OP_PORTSC(p)] = (sc & PORT_KEEP) | PORT_POWER | PORT_CHANGES;
        }
    }

    /* And the ports of every hub, the same way. Taken after the root
     * ports so a hub plugged in this moment is asked about in the same
     * pass it was found. */
    for (u32 i = 0; i < DEV_MAX; i++)
        if (dev[i].used && dev[i].kind == KIND_HUB && dev[i].hub_ports)
            look_at_hub(&dev[i]);
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

static u32 popcount32(u32 v)
{
    u32 n = 0;
    while (v) { n += v & 1u; v >>= 1; }
    return n;
}

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

/* Some chipsets can hand their slower sockets to either of two
 * controllers, and leave that decision in four registers of the newer
 * one's configuration space.
 *
 * The firmware may have left half the sockets pointed at a controller
 * that is switched off, and then those sockets are simply dead: nothing
 * plugged into them appears anywhere, and a keyboard in the wrong hole
 * looks exactly like no keyboard at all. Each pair is a mask of what
 * may be moved and a register saying where it points, so writing the
 * mask into the register claims everything that is allowed to be
 * claimed and touches nothing else. Where the registers do not exist
 * the mask reads as zero and this writes zero, which changes nothing.
 */
static void claim_shared_ports(const pci_device *pd)
{
    if (pd->vendor != 0x8086) return;

    u32 super = pci_read32(pd, 0xDC);           /* which may be made fast */
    if (super) pci_write32(pd, 0xD8, super);
    u32 slow = pci_read32(pd, 0xD4);            /* which may be moved here */
    if (slow) pci_write32(pd, 0xD0, slow);

    if (super || slow)
        kprintf("usb:  took the shared sockets off the older controller "
                "(%u fast, %u slow)\n", popcount32(super), popcount32(slow));
}

bool xhci_init(void)
{
    const pci_device *pd = pci_find(0x0C, 0x03, 0x30);
    if (!pd) { kprintf("usb:  no xhci controller on the bus\n"); return false; }

    u32 command_reg = pci_read32(pd, 0x04);
    pci_write32(pd, 0x04, command_reg | (1u << 1) | (1u << 2));
    claim_shared_ports(pd);

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

    /* Power anything that is not powered, then give the ports a moment
     * to show what is plugged in. */
    power_ports();
    wait_ms(200);

    /* What the controller can see, before any of it is interpreted.
     * On a machine that will not be sitting next to a debugger, this
     * one line is the difference between a driver that failed and a
     * driver that was never given anything to find. */
    u32 live = 0;
    kprintf("usb:  ports with something on them:");
    for (u32 p = 1; p <= max_ports && p < 256; p++) {
        u32 sc = op[OP_PORTSC(p)];
        if (!(sc & PORT_CONNECTED)) continue;
        live++;
        static const char *const wire[6] = { "?", "full", "low", "high", "super", "super+" };
        u32 s = PORT_SPEED(sc);
        kprintf(" %u(%s)", p, s < 6 ? wire[s] : "?");
    }
    if (!live) kprintf(" none");
    kprintf("\n");
    if (!live) {
        u32 unpowered = 0;
        for (u32 p = 1; p <= max_ports && p < 256; p++)
            if (!(op[OP_PORTSC(p)] & PORT_POWER)) unpowered++;
        kprintf("usb:  %u ports, %u of them without power; nothing is plugged in "
                "that this controller owns\n", max_ports, unpowered);
    }

    look_at_ports();
    if (!keyboards && !mice) kprintf("usb:  no keyboard and no mouse were found\n");
    else kprintf("usb:  %u keyboard%s and %u mouse%s\n",
                 keyboards, keyboards == 1 ? "" : "s", mice, mice == 1 ? "" : "s");

    thread_create("usb", usb_thread, NULL, thread_domain(sched_current()));
    return true;
}

bool xhci_present(void)   { return present; }
u32  xhci_keyboards(void) { return keyboards; }
u32  xhci_mice(void)      { return mice; }
