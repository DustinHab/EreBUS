/*
 * xhci.c -- USB keyboards and mice via xHCI, polled by a thread.
 * - firmware handoff, port power, Intel port routing, root ports, hubs (route string, TT), hotplug
 * - keyboards: boot protocol, 8-byte reports, repeat in the driver
 * - mice: report descriptor parsed (buttons, axes, wheel, report id); boot protocol as fallback
 * - reports are fed into the PS/2 queues as scancode set 1 / mouse packets
 * - not driven: USB disks, isochronous endpoints
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

/* Where a pointer keeps its numbers inside its own reports; read out
 * of the description the device carries. See read_pointer_map. */
typedef struct {
    bool have;
    u8   id;                       /* the report number, 0 where there is none */
    u16  buttons_at; u8 buttons_bits;
    u16  x_at, y_at, wheel_at;
    u8   x_bits, y_bits, wheel_bits;
} pointer_map;

typedef struct {
    bool used;
    ring r;
    u32  dci;                         /* its number in the device context */
    u32  report_len;
    u32  at;                          /* where its reports land in the page */
    u8   kind;                        /* keyboard or mouse */
    u8   iface;
    pointer_map map;                  /* for a pointer: where its numbers are */
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

/* ------------------------------------------------------------------ */
/* Where a pointer keeps its numbers                                   */
/* ------------------------------------------------------------------ */
/*
 * A mouse asked to speak the boot protocol sends three bytes: the
 * buttons, and how far it moved in each direction. There is no fourth
 * byte, and there is no wheel -- the boot protocol was defined for a
 * firmware setup screen in 1996, and wheels came later.
 *
 * The wheel is in the device's own layout, which every one of them
 * describes for itself in a small language: this many bits mean this,
 * then this many mean that. So the description is read and the offsets
 * worked out, and after that the reports are taken as the device meant
 * them -- which also gets the mice that count in sixteen bits, and the
 * ones that put a report number in front.
 *
 * When the description cannot be made sense of, the boot protocol is
 * asked for and the old three bytes are read. A pointer that moves
 * without scrolling is better than one that does neither.
 */
/* One field out of a report, as a number that may be negative. */
static i32 field_signed(const u8 *r, u32 len, u16 at, u8 bits)
{
    if (!bits) return 0;
    u32 v = 0;
    for (u32 i = 0; i < bits; i++) {
        u32 b = (u32)at + i;
        if (b / 8 >= len) return 0;
        if (r[b / 8] & (1u << (b % 8))) v |= 1u << i;
    }
    if (bits < 32 && (v & (1u << (bits - 1)))) v |= ~0u << bits;
    return (i32)v;
}

static u32 field_plain(const u8 *r, u32 len, u16 at, u8 bits)
{
    u32 v = 0;
    if (bits > 32) bits = 32;
    for (u32 i = 0; i < bits; i++) {
        u32 b = (u32)at + i;
        if (b / 8 >= len) break;
        if (r[b / 8] & (1u << (b % 8))) v |= 1u << i;
    }
    return v;
}

/* Reads the description. Items are a prefix byte saying what kind and
 * how many bytes follow; globals hold until changed, locals until the
 * next thing they describe. Only what a pointer needs is kept. */
static bool read_pointer_map(const u8 *p, u32 len, pointer_map *m)
{
    u32 usage_page = 0, report_size = 0, report_count = 0, report_id = 0;
    u32 usages[16], nusages = 0;
    u32 usage_min = 0, usage_max = 0;
    bool ranged = false;
    u32 bit = 0;

    memset(m, 0, sizeof(*m));

    for (u32 i = 0; i < len; ) {
        u8 prefix = p[i];
        if (prefix == 0xFE) break;              /* a long item; none of ours are */
        u32 size = prefix & 3u;
        if (size == 3) size = 4;
        u32 tag = prefix >> 4, type = (prefix >> 2) & 3u;
        if (i + 1 + size > len) break;

        u32 value = 0;
        for (u32 b = 0; b < size; b++) value |= (u32)p[i + 1 + b] << (8 * b);
        i += 1 + size;

        if (type == 1) {                        /* holds until changed */
            if (tag == 0) usage_page = value;
            else if (tag == 7) report_size = value;
            else if (tag == 8) { if (report_id != value) { report_id = value; bit = 0; } }
            else if (tag == 9) report_count = value;
        } else if (type == 2) {                 /* describes what comes next */
            if (tag == 0 && nusages < 16) usages[nusages++] = value;
            else if (tag == 1) { usage_min = value; ranged = true; }
            else if (tag == 2) usage_max = value;
        } else if (type == 0) {
            if (tag == 8) {                     /* something the device reports */
                bool constant = (value & 1) != 0;
                if (!constant) {
                    if (usage_page == 0x09 && !m->buttons_bits) {
                        m->buttons_at = (u16)bit;
                        m->buttons_bits = (u8)(report_size * report_count > 8
                                               ? 8 : report_size * report_count);
                    } else {
                        for (u32 k = 0; k < report_count; k++) {
                            u32 u = 0;
                            if (ranged) u = usage_min + k > usage_max ? usage_max : usage_min + k;
                            else if (k < nusages) u = usages[k];
                            else if (nusages) u = usages[nusages - 1];
                            u16 at = (u16)(bit + k * report_size);
                            if (usage_page == 0x01 && u == 0x30) {
                                m->x_at = at; m->x_bits = (u8)report_size; m->id = (u8)report_id;
                            } else if (usage_page == 0x01 && u == 0x31) {
                                m->y_at = at; m->y_bits = (u8)report_size;
                            } else if (usage_page == 0x01 && u == 0x38 && !m->wheel_bits) {
                                m->wheel_at = at; m->wheel_bits = (u8)report_size;
                            }
                        }
                    }
                }
                bit += report_size * report_count;
            }
            nusages = 0; ranged = false; usage_min = usage_max = 0;
        }
    }

    m->have = m->x_bits && m->y_bits;
    return m->have;
}

/* The reading of descriptions, checked against three written out by
 * hand -- because the device that would catch a mistake here is the
 * one nobody has, and the emulated mouse is too kind to be a test: it
 * sends its wheel whatever it was asked for, so it looks the same
 * whether the description was read correctly or ignored entirely.
 *
 * The three are the shapes that actually turn up. A plain mouse, three
 * buttons and eight bits a direction. One that counts in sixteen bits,
 * which every high-resolution mouse does. And one that numbers its
 * reports, because it has more than one thing to say -- there the
 * numbers start a byte later than they look. */
static bool pointer_selftest(void)
{
    static const u8 plain[] = {
        0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00,
        0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01,
        0x95, 0x03, 0x75, 0x01, 0x81, 0x02,             /* three buttons */
        0x95, 0x01, 0x75, 0x05, 0x81, 0x01,             /* five bits of nothing */
        0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38,
        0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06,
        0xC0, 0xC0,
    };
    static const u8 wide[] = {
        0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00,
        0x05, 0x09, 0x19, 0x01, 0x29, 0x05, 0x15, 0x00, 0x25, 0x01,
        0x95, 0x05, 0x75, 0x01, 0x81, 0x02,             /* five buttons */
        0x95, 0x01, 0x75, 0x03, 0x81, 0x01,             /* three bits of nothing */
        0x05, 0x01, 0x09, 0x30, 0x09, 0x31,
        0x16, 0x00, 0x80, 0x26, 0xFF, 0x7F,
        0x75, 0x10, 0x95, 0x02, 0x81, 0x06,             /* sixteen bits each way */
        0x09, 0x38, 0x15, 0x81, 0x25, 0x7F,
        0x75, 0x08, 0x95, 0x01, 0x81, 0x06,             /* and a wheel */
        0xC0, 0xC0,
    };
    static const u8 numbered[] = {
        0x05, 0x01, 0x09, 0x02, 0xA1, 0x01,
        0x85, 0x02,                                     /* this one is report two */
        0x09, 0x01, 0xA1, 0x00,
        0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01,
        0x95, 0x03, 0x75, 0x01, 0x81, 0x02,
        0x95, 0x01, 0x75, 0x05, 0x81, 0x01,
        0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38,
        0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06,
        0xC0, 0xC0,
    };

    pointer_map m;
    if (!read_pointer_map(plain, sizeof(plain), &m)) return false;
    if (m.id || m.buttons_at != 0 || m.buttons_bits != 3) return false;
    if (m.x_at != 8 || m.x_bits != 8) return false;
    if (m.y_at != 16 || m.y_bits != 8) return false;
    if (m.wheel_at != 24 || m.wheel_bits != 8) return false;

    if (!read_pointer_map(wide, sizeof(wide), &m)) return false;
    if (m.buttons_bits != 5) return false;
    if (m.x_at != 8 || m.x_bits != 16) return false;
    if (m.y_at != 24 || m.y_bits != 16) return false;
    if (m.wheel_at != 40 || m.wheel_bits != 8) return false;

    if (!read_pointer_map(numbered, sizeof(numbered), &m)) return false;
    if (m.id != 2 || m.x_at != 8 || m.wheel_at != 24) return false;

    /* And the taking apart, on a report of the first shape: the middle
     * button down, four left, three up, one notch back. */
    static const u8 report[4] = { 0x02, 0xFC, 0x03, 0xFF };
    read_pointer_map(plain, sizeof(plain), &m);
    if (field_plain(report, 4, m.buttons_at, m.buttons_bits) != 2) return false;
    if (field_signed(report, 4, m.x_at, m.x_bits) != -4) return false;
    if (field_signed(report, 4, m.y_at, m.y_bits) != 3) return false;
    if (field_signed(report, 4, m.wheel_at, m.wheel_bits) != -1) return false;

    return true;
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
    d->used = false;
    command(0, TRB_TYPE(T_DISABLE_SLOT) | TRB_SLOT(d->slot), NULL);
    dcbaa[d->slot] = 0;

    /* Hand back what it held. A device that comes and goes all evening
     * would otherwise take four pages with it every time, and the one
     * plugging it in would have no way of knowing why the machine
     * eventually stopped finding anything. */
    if (d->ep0.pa) pmm_free(d->ep0.pa);
    for (u32 k = 0; k < IN_MAX; k++)
        if (d->in[k].r.pa) pmm_free(d->in[k].r.pa);
    if (d->dctx_pa) pmm_free(d->dctx_pa);
    if (d->ictx_pa) pmm_free(d->ictx_pa);
    if (d->buf_pa)  pmm_free(d->buf_pa);
    memset(d, 0, sizeof(*d));
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
    struct { u8 kind, iface, addr; u32 packet, interval, report_len; } found[IN_MAX];
    u32 nfound = 0;
    u8 seen_class[8]; u32 nclass = 0;
    u8 cur_kind = 0, cur_iface = 0;
    u32 cur_report_len = 0;

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
                cur_report_len = 0;
                (void)sub;
            }
        } else if (type == 0x21 && len >= 9 && cur_kind) {
            /* The interface's own descriptor, whose one useful number
             * here is how long the description of its reports is. */
            if (d->buf[i + 6] == 0x22)
                cur_report_len = (u32)(d->buf[i + 7] | (d->buf[i + 8] << 8));
        } else if (type == 5 && len >= 7 && cur_kind && nfound < IN_MAX) {
            /* interrupt, and inbound */
            if ((d->buf[i + 3] & 3) == 3 && (d->buf[i + 2] & 0x80)) {
                found[nfound].kind = cur_kind;
                found[nfound].iface = cur_iface;
                found[nfound].addr = d->buf[i + 2];
                found[nfound].packet = (u32)(d->buf[i + 4] | (d->buf[i + 5] << 8)) & 0x7FF;
                found[nfound].interval = d->buf[i + 6];
                found[nfound].report_len = cur_report_len;
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
        /* A keyboard is read the boot way, eight bytes, which every
         * one of them speaks. A pointer is read the way it describes
         * itself, which may be longer than eight and often is. */
        in->report_len = found[k].kind == KIND_KEYBOARD
                       ? 8 : (packet < 64 ? packet : 64);
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

        /* A pointer is asked how it lays its reports out, because the
         * boot protocol has no wheel in it and never had. */
        if (in->kind == KIND_MOUSE && found[k].report_len) {
            u32 want = found[k].report_len;
            if (want > 1024) want = 1024;
            if (control(d, 0x81, 6, 0x2200, in->iface, (u16)want, d->buf_pa + 1024))
                read_pointer_map(d->buf + 1024, want, &in->map);
        }

        /* Ask for the layout that suits, and for silence while nothing
         * changes. Neither is required of the device, and a device that
         * refuses either is still read: it simply keeps what it had. */
        control(d, 0x21, 0x0B, in->map.have ? 1 : 0, in->iface, 0, 0);
        control(d, 0x21, 0x0A, 0, in->iface, 0, 0);
        if (in->kind == KIND_KEYBOARD) keyboards++; else mice++;
        queue_report(d, in);
    }

    for (u32 k = 0; k < nfound; k++) {
        usbin *in = &d->in[k];
        if (in->kind != KIND_MOUSE) continue;
        if (in->map.have)
            kprintf("usb:  its pointer reports %u buttons, %u bits each way%s\n",
                    in->map.buttons_bits, in->map.x_bits,
                    in->map.wheel_bits ? ", and a wheel" : ", and no wheel");
        else
            kprintf("usb:  its pointer would not describe itself; "
                    "read the boot way, without a wheel\n");
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
        if (in->map.have) {
            const u8 *p = r;
            u32 plen = len;
            /* Where the device numbers its reports, the number comes
             * first and the offsets are counted after it. A report of
             * another number belongs to something else on the same
             * wire -- extra keys, a battery reading -- and is not ours. */
            if (in->map.id) {
                if (!len || r[0] != in->map.id) return;
                p = r + 1;
                plen = len - 1;
            }
            i32 dx = field_signed(p, plen, in->map.x_at, in->map.x_bits);
            i32 dy = field_signed(p, plen, in->map.y_at, in->map.y_bits);
            i32 dz = field_signed(p, plen, in->map.wheel_at, in->map.wheel_bits);
            u32 b = field_plain(p, plen, in->map.buttons_at, in->map.buttons_bits);
            /* the wheel counts away from the person; the page counts down */
            ps2_feed_mouse(dx, dy, -dz, (u8)b);
            return;
        }
        if (len < 3) return;
        i32 dx = (i8)r[1], dy = (i8)r[2], dz = len >= 4 ? (i8)r[3] : 0;
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

static u8 port_tries[256];

static void look_at_ports(void)
{
    for (u32 p = 1; p <= max_ports && p < 256; p++) {
        u32 sc = op[OP_PORTSC(p)];
        bool connected = (sc & PORT_CONNECTED) != 0;
        if (connected && !port_seen[p]) {
            /* Something is electrically present. That is not the same
             * as ready to answer: the standard asks for a settling time
             * before the port is reset, and a device asked too early
             * answers nothing at all. Under emulation the device is
             * ready before the plug is in and this pause looks like
             * superstition; on a real socket it is the difference
             * between plugging a mouse back in and restarting. */
            wait_ms(150);
            if (!(op[OP_PORTSC(p)] & PORT_CONNECTED)) continue;   /* it bounced back out */

            bool came_up = false;
            if (port_reset(p)) {
                where w;
                memset(&w, 0, sizeof(w));
                w.root_port = p;
                w.speed = PORT_SPEED(op[OP_PORTSC(p)]);
                came_up = device_up(&w);
            }
            if (came_up) {
                port_seen[p] = true;
                port_tries[p] = 0;
            } else if (++port_tries[p] >= 3) {
                /* Three goes and then quiet, so a socket with something
                 * broken in it does not fill the log forever. Pulling
                 * whatever it is starts the count again. */
                port_seen[p] = true;
                kprintf("usb:  port %u: something is there, but it would not come up "
                        "(port register %08x)\n", p, op[OP_PORTSC(p)]);
            }
        } else if (!connected && port_seen[p]) {
            port_tries[p] = 0;
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
    kprintf("usb:  %s\n", pointer_selftest()
            ? "self test passed -- a pointer's own layout is read as written"
            : "SELF TEST FAILED -- pointers will be read the boot way, without wheels");

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
