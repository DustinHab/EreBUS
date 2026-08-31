/*
 * time.c -- time stamp counter and the periodic tick.
 *
 * Two sources, on purpose:
 *
 * The time stamp counter is a register that increments with the core
 * clock. Reading it is a single instruction and costs nothing, which is
 * what makes per-line timestamps affordable. What it does not know is
 * how fast it counts -- no register reports that.
 *
 * The programmable interval timer does know: it is driven by a crystal
 * running at a fixed 1.193182 MHz, a number inherited from the colour
 * subcarrier of NTSC television by way of the original IBM PC. So we
 * let the PIT run for a known span and count how far the TSC got.
 *
 * Channel 2 is used for that, not channel 0. Channel 2 was wired to the
 * speaker and can be gated by software, which means it can be polled
 * without any interrupt being configured yet. Channel 0 is then left
 * free for the actual tick.
 */
#include <eb/time.h>
#include <eb/thread.h>
#include <eb/trap.h>
#include <eb/pic.h>
#include <eb/io.h>
#include <eb/fmt.h>

#define PIT_FREQ   1193182u    /* Hz, fixed by the hardware */

#define PIT_CH0    0x40
#define PIT_CH2    0x42
#define PIT_CMD    0x43
#define PORT_61    0x61        /* gate and output of channel 2 */

static u64 tsc_hz;
static u64 epoch;

/* ------------------------------------------------------------------ */
/* Measuring the counter frequency                                     */
/* ------------------------------------------------------------------ */

static bool measure(u32 ms, u64 *out_hz)
{
    u32 count = (u32)((u64)PIT_FREQ * ms / 1000u);
    if (count == 0 || count > 0xFFFF) return false;

    u8 saved = inb(PORT_61);

    /* Gate on, speaker off. Bit 1 drives the speaker -- leaving it set
     * would make the machine beep for the duration of the measurement. */
    outb(PORT_61, (u8)((saved & ~0x02u) | 0x01u));

    /* Channel 2, low byte then high byte, mode 1 (retriggerable
     * one-shot), binary counting. */
    outb(PIT_CMD, 0xB2);
    outb(PIT_CH2, (u8)(count & 0xFF));
    outb(PIT_CH2, (u8)(count >> 8));

    /* Mode 1 starts on the rising edge of the gate, so drop it and
     * raise it again to get a defined start. */
    u8 gate = (u8)(inb(PORT_61) & ~0x01u);
    outb(PORT_61, gate);
    outb(PORT_61, (u8)(gate | 0x01u));

    u64 start = rdtsc();

    /* Bit 5 of port 0x61 mirrors the channel 2 output: it goes high
     * when the count runs out. The guard keeps a dead timer from
     * hanging the boot forever. */
    u64 guard = 0;
    while (!(inb(PORT_61) & 0x20u)) {
        if (++guard > 200000000ULL) {
            outb(PORT_61, saved);
            return false;
        }
    }

    u64 end = rdtsc();
    outb(PORT_61, saved);

    if (end <= start) return false;
    *out_hz = (end - start) * 1000ULL / ms;
    return true;
}

bool time_init(void)
{
    /* Three runs, keep the middle one. A single measurement can be
     * spoiled by a system management interrupt, which is invisible to
     * us and can steal milliseconds. The median throws that out. */
    u64 r[3] = { 0, 0, 0 };
    for (u32 i = 0; i < 3; i++)
        if (!measure(20, &r[i])) return false;

    for (u32 i = 0; i < 2; i++)
        for (u32 j = i + 1; j < 3; j++)
            if (r[j] < r[i]) { u64 t = r[i]; r[i] = r[j]; r[j] = t; }

    /* Below 1 MHz or above 100 GHz something went wrong, and a wrong
     * clock is worse than none. */
    if (r[1] < 1000000ULL || r[1] > 100000000000ULL) return false;

    tsc_hz = r[1];
    epoch  = rdtsc();
    return true;
}

u64 time_tsc_hz(void) { return tsc_hz; }

/* ------------------------------------------------------------------ */
/* The clock on the wall                                               */
/* ------------------------------------------------------------------ */

/* The CMOS real-time clock: the one piece of the machine that knows
 * what time it is rather than how long it has been running. Read once
 * at start-up; from then on the TSC carries it forward, which spares
 * the two-port dance on every glance at the corner of the screen. */
static u64 boot_wall;         /* seconds into the day when we started */
static u64 boot_date;         /* y*10000 + m*100 + d, for the stamp */

static u8 cmos_read(u8 reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static u8 from_bcd(u8 v, bool bcd)
{
    return bcd ? (u8)((v >> 4) * 10 + (v & 0x0F)) : v;
}

void time_read_rtc(void)
{
    /* Wait out an update in progress, then read twice until both reads
     * agree -- the clock ticks on its own schedule, not ours. */
    u8 s1, m1, h1, s2, m2, h2, day, mon, yr;
    bool bcd;

    for (u32 tries = 0; tries < 8; tries++) {
        while (cmos_read(0x0A) & 0x80) { }
        s1 = cmos_read(0x00); m1 = cmos_read(0x02); h1 = cmos_read(0x04);
        day = cmos_read(0x07); mon = cmos_read(0x08); yr = cmos_read(0x09);
        s2 = cmos_read(0x00); m2 = cmos_read(0x02); h2 = cmos_read(0x04);
        if (s1 == s2 && m1 == m2 && h1 == h2) break;
    }

    bcd = !(cmos_read(0x0B) & 0x04);
    u64 hh = from_bcd((u8)(h1 & 0x7F), bcd);
    u64 mm = from_bcd(m1, bcd);
    u64 ss = from_bcd(s1, bcd);

    boot_wall = hh * 3600 + mm * 60 + ss;
    boot_date = (u64)from_bcd(yr, bcd) * 10000 +
                (u64)from_bcd(mon, bcd) * 100 +
                (u64)from_bcd(day, bcd);
}

void time_wall(u32 *h, u32 *m, u32 *s)
{
    u64 now = (boot_wall + time_ns() / 1000000000ULL) % 86400;
    if (h) *h = (u32)(now / 3600);
    if (m) *m = (u32)((now / 60) % 60);
    if (s) *s = (u32)(now % 60);
}

u64 time_boot_stamp(void)
{
    /* Something that tells this boot apart from the last one. Date and
     * time of day to the second: two boots inside the same second would
     * collide, and nothing else on this machine changes faster. */
    return (boot_date << 20) | boot_wall;
}

u64 time_ns(void)
{
    if (!tsc_hz) return 0;

    u64 delta = rdtsc() - epoch;

    /* Split into whole seconds and remainder before scaling. Doing
     * delta * 1000000000 in one go overflows a 64-bit value after a few
     * seconds of uptime. */
    u64 sec = delta / tsc_hz;
    u64 rem = delta % tsc_hz;
    return sec * 1000000000ULL + (rem * 1000000000ULL) / tsc_hz;
}

/* ------------------------------------------------------------------ */
/* Periodic tick                                                       */
/* ------------------------------------------------------------------ */

static volatile u64 ticks;
static u32 tick_hz;

static void on_tick(trap_frame *f)
{
    (void)f;
    ticks++;

    /* Only accounting here. The switch itself happens on the way out of
     * the interrupt, where changing stacks is safe. */
    sched_tick();
}

void pit_init(u32 hz)
{
    if (hz == 0) hz = 100;
    u32 divisor = PIT_FREQ / hz;
    if (divisor == 0) divisor = 1;
    if (divisor > 0xFFFF) divisor = 0xFFFF;

    /* Report what the hardware will actually do, not what was asked
     * for: the divisor is an integer, so the rate is rarely exact. */
    tick_hz = PIT_FREQ / divisor;

    /* Channel 0, low then high byte, mode 2 (rate generator), binary. */
    outb(PIT_CMD, 0x34);
    outb(PIT_CH0, (u8)(divisor & 0xFF));
    outb(PIT_CH0, (u8)(divisor >> 8));

    irq_install(0, on_tick);
}

u64 pit_ticks(void) { return ticks; }
u32 pit_hz(void)    { return tick_hz; }
