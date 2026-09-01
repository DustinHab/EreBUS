/*
 * ps2.c -- keyboard and mouse.
 */
#include <eb/ps2.h>
#include <eb/trap.h>
#include <eb/pic.h>
#include <eb/io.h>
#include <eb/fmt.h>

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

#define STATUS_OUTPUT_FULL 0x01
#define STATUS_INPUT_FULL  0x02
#define STATUS_FROM_MOUSE  0x20

#define CMD_READ_CONFIG    0x20
#define CMD_WRITE_CONFIG   0x60
#define CMD_DISABLE_MOUSE  0xA7
#define CMD_ENABLE_MOUSE   0xA8
#define CMD_DISABLE_KBD    0xAD
#define CMD_ENABLE_KBD     0xAE
#define CMD_TO_MOUSE       0xD4

#define CFG_KBD_IRQ    0x01
#define CFG_MOUSE_IRQ  0x02
#define CFG_TRANSLATE  0x40

static bool kbd_ok, mouse_ok;
static u64 key_total, mouse_total;

/* ------------------------------------------------------------------ */
/* Talking to the controller                                           */
/* ------------------------------------------------------------------ */

/* Every wait is bounded. A PS/2 controller that stops answering is a
 * real possibility on odd hardware, and a boot that hangs in a driver
 * is worse than one that reports the device missing. */
static bool wait_writable(void)
{
    for (u32 i = 0; i < 100000; i++)
        if (!(inb(PS2_STATUS) & STATUS_INPUT_FULL)) return true;
    return false;
}

static bool wait_readable(void)
{
    for (u32 i = 0; i < 100000; i++)
        if (inb(PS2_STATUS) & STATUS_OUTPUT_FULL) return true;
    return false;
}

static void command(u8 cmd)
{
    if (wait_writable()) outb(PS2_CMD, cmd);
}

static void write_data(u8 value)
{
    if (wait_writable()) outb(PS2_DATA, value);
}

static int read_data(void)
{
    if (!wait_readable()) return -1;
    return inb(PS2_DATA);
}

static int mouse_command(u8 cmd)
{
    command(CMD_TO_MOUSE);
    write_data(cmd);
    return read_data();          /* the device acknowledges with 0xFA */
}

/* A mouse command that takes an argument: the command, its ack, the
 * argument, its ack. */
static int mouse_arg_command(u8 cmd, u8 arg)
{
    if (mouse_command(cmd) != 0xFA) return -1;
    command(CMD_TO_MOUSE);
    write_data(arg);
    return read_data();
}

/* ------------------------------------------------------------------ */
/* Event queues                                                        */
/* ------------------------------------------------------------------ */

#define QUEUE_SIZE 64

static key_event   key_queue[QUEUE_SIZE];
static u32         key_head, key_tail;
static mouse_event mouse_queue[QUEUE_SIZE];
static u32         mouse_head, mouse_tail;

static void push_key(const key_event *e)
{
    u32 next = (key_tail + 1) % QUEUE_SIZE;
    if (next == key_head) return;      /* full: drop, rather than block IRQ */
    key_queue[key_tail] = *e;
    key_tail = next;
}

static void push_mouse(const mouse_event *e)
{
    u32 next = (mouse_tail + 1) % QUEUE_SIZE;
    if (next == mouse_head) return;
    mouse_queue[mouse_tail] = *e;
    mouse_tail = next;
}

bool ps2_poll_key(key_event *out)
{
    u64 flags = irq_save();
    bool got = (key_head != key_tail);
    if (got) {
        *out = key_queue[key_head];
        key_head = (key_head + 1) % QUEUE_SIZE;
    }
    irq_restore(flags);
    return got;
}

bool ps2_poll_mouse(mouse_event *out)
{
    u64 flags = irq_save();
    bool got = (mouse_head != mouse_tail);
    if (got) {
        *out = mouse_queue[mouse_head];
        mouse_head = (mouse_head + 1) % QUEUE_SIZE;
    }
    irq_restore(flags);
    return got;
}

/* ------------------------------------------------------------------ */
/* Keyboard                                                            */
/* ------------------------------------------------------------------ */

/* Scancode set 1, US layout, indexed by make code. Two tables because
 * shift does not simply change case: the number row and the punctuation
 * keys produce entirely different characters. */
static const char plain[0x59] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0,   '*', 0,   ' '
};

static const char shifted[0x59] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"',  '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    0,   '*', 0,   ' '
};

static bool shift_down, ctrl_down, alt_down;

static void on_keyboard(trap_frame *f)
{
    (void)f;
    u8 code = inb(PS2_DATA);

    /* 0xE0 introduces a two-byte code for the keys that did not exist
     * on the original keyboard: the arrows, the right-hand modifiers,
     * the navigation block. Their second byte repeats a code that also
     * belongs to a key on the numeric pad, so the prefix is the only
     * thing telling them apart. */
    static bool extended;
    if (code == 0xE0) { extended = true; return; }

    bool down = !(code & 0x80);
    u8 make = code & 0x7F;

    if (extended) {
        extended = false;

        u32 cp = 0;
        switch (make) {
        case 0x48: cp = KEY_UP;    break;
        case 0x50: cp = KEY_DOWN;  break;
        case 0x4B: cp = KEY_LEFT;  break;
        case 0x4D: cp = KEY_RIGHT; break;
        case 0x47: cp = KEY_HOME;  break;
        case 0x4F: cp = KEY_END;   break;
        case 0x49: cp = KEY_PGUP;  break;
        case 0x51: cp = KEY_PGDN;  break;
        case 0x53: cp = KEY_DELETE; break;
        case 0x1D: ctrl_down = down; return;
        case 0x38: alt_down = down; return;
        default: return;
        }

        key_event ex = {
            .scancode = make, .codepoint = cp, .down = down,
            .shift = shift_down, .ctrl = ctrl_down, .alt = alt_down,
        };
        if (down) key_total++;
        push_key(&ex);
        return;
    }

    switch (make) {
    case 0x2A: case 0x36: shift_down = down; return;
    case 0x1D:            ctrl_down  = down; return;
    case 0x38:            alt_down   = down; return;
    }

    key_event e = {
        .scancode = make,
        .codepoint = 0,
        .down = down,
        .shift = shift_down, .ctrl = ctrl_down, .alt = alt_down,
    };
    if (make < sizeof(plain))
        e.codepoint = (u32)(u8)(shift_down ? shifted[make] : plain[make]);

    if (down) key_total++;
    push_key(&e);
}

/* ------------------------------------------------------------------ */
/* Mouse                                                               */
/* ------------------------------------------------------------------ */

static bool mouse_wheel;             /* the fourth byte carries the wheel */

static void on_mouse(trap_frame *f)
{
    (void)f;

    static u8 packet[4];
    static u32 index;

    u8 byte = inb(PS2_DATA);
    u32 want = mouse_wheel ? 4 : 3;

    /* Bit 3 of the first byte is always set. If it is not, we are out
     * of step with the device -- which happens after a dropped byte --
     * and the fix is to wait for a byte that could be a first one
     * rather than to keep assembling nonsense. */
    if (index == 0 && !(byte & 0x08)) return;

    packet[index++] = byte;
    if (index < want) return;
    index = 0;

    /* Overflow bits mean the counters saturated; the movement in that
     * packet is meaningless. */
    if (packet[0] & 0xC0) return;

    /* The counters are nine bits: eight in the byte, the sign in the
     * first byte. The wheel, when there is one, is a small signed
     * count in the fourth: rolling toward oneself reads positive,
     * which is also the direction that reads further down a page. */
    i32 dx = (i32)packet[1] - ((packet[0] & 0x10) ? 256 : 0);
    i32 dy = (i32)packet[2] - ((packet[0] & 0x20) ? 256 : 0);
    i32 dz = mouse_wheel ? (i32)(i8)packet[3] : 0;
    if (dz > 8) dz = 8;
    if (dz < -8) dz = -8;

    mouse_event e = {
        .dx = dx,
        .dy = -dy,          /* the device counts up, screens count down */
        .dz = dz,
        .buttons = (u8)(packet[0] & 0x07),
    };
    mouse_total++;
    push_mouse(&e);
}

/* ------------------------------------------------------------------ */

void ps2_init(void)
{
    /* Quiet both devices before touching the configuration, so nothing
     * arrives mid-change. */
    command(CMD_DISABLE_KBD);
    command(CMD_DISABLE_MOUSE);

    /* Drain whatever the firmware left behind. */
    for (u32 i = 0; i < 32 && (inb(PS2_STATUS) & STATUS_OUTPUT_FULL); i++)
        (void)inb(PS2_DATA);

    command(CMD_READ_CONFIG);
    int cfg = read_data();
    if (cfg < 0) {
        kprintf("ps2:  controller does not answer\n");
        return;
    }

    /* Interrupts from both, and keep translation on so the keyboard
     * speaks scancode set 1 whatever it actually generates. */
    cfg |= CFG_KBD_IRQ | CFG_MOUSE_IRQ | CFG_TRANSLATE;
    command(CMD_WRITE_CONFIG);
    write_data((u8)cfg);

    command(CMD_ENABLE_KBD);
    command(CMD_ENABLE_MOUSE);

    /* Defaults first. Then the knock that wakes the wheel: three
     * sample rates in a row -- 200, 100, 80 -- is the sequence the
     * IntelliMouse protocol listens for, and a device that knows it
     * answers the next identify with 3 and grows a fourth byte. A
     * device that does not stays a three-byte mouse and none of this
     * did any harm. */
    mouse_ok = (mouse_command(0xF6) == 0xFA);
    if (mouse_ok) {
        mouse_arg_command(0xF3, 200);
        mouse_arg_command(0xF3, 100);
        mouse_arg_command(0xF3, 80);
        if (mouse_command(0xF2) == 0xFA) {
            int id = read_data();
            mouse_wheel = (id == 3);
        }
        mouse_arg_command(0xF3, 100);       /* a sane rate to live at */
        mouse_ok = (mouse_command(0xF4) == 0xFA);
    }
    kbd_ok = true;

    kprintf("ps2:  mouse %s\n",
            !mouse_ok ? "missing" :
            mouse_wheel ? "present, with a wheel" : "present, no wheel");

    irq_install(1, on_keyboard);
    if (mouse_ok) irq_install(12, on_mouse);
}

bool ps2_keyboard_present(void) { return kbd_ok; }
bool ps2_mouse_present(void)    { return mouse_ok; }
u64  ps2_key_count(void)        { return key_total; }
u64  ps2_mouse_count(void)      { return mouse_total; }
