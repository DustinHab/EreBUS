/*
 * time.c -- TSC for timestamps, PIT for the tick.
 * - TSC rate measured against PIT channel 2 (software-gated, pollable before interrupts exist)
 * - PIT channel 0 drives the periodic tick
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
static u64 boot_unix;         /* seconds since 1970 when we started; 0 until a clock was read */

/* Days since 1970-01-01 of a civil date. */
static i64 days_from_civil(i64 y, u32 m, u32 d)
{
    y -= m <= 2;
    i64 era = (y >= 0 ? y : y - 399) / 400;
    u64 yoe = (u64)(y - era * 400);
    u64 doy = (153 * (m + (m > 2 ? (u32)-3 : 9)) + 2) / 5 + d - 1;
    u64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (i64)doe - 719468;
}

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

    /* The date as a count of seconds, for anything that compares
     * dates; the two-digit year is taken to lie in this century. */
    u32 y = 2000 + from_bcd(yr, bcd), mo = from_bcd(mon, bcd), d = from_bcd(day, bcd);
    if (mo >= 1 && mo <= 12 && d >= 1 && d <= 31)
        boot_unix = (u64)days_from_civil((i64)y, mo, d) * 86400 + boot_wall;
}

u64 time_unix(void)
{
    if (!boot_unix) return 0;
    return boot_unix + time_ns() / 1000000000ULL;
}

void time_set_unix(u64 secs)
{
    u64 up = time_ns() / 1000000000ULL;
    boot_unix = secs > up ? secs - up : secs;
    time_set_wall((u32)((secs / 3600) % 24), (u32)((secs / 60) % 60), (u32)(secs % 60));
}

void time_wall(u32 *h, u32 *m, u32 *s)
{
    u64 now = (boot_wall + time_ns() / 1000000000ULL) % 86400;
    if (h) *h = (u32)(now / 3600);
    if (m) *m = (u32)((now / 60) % 60);
    if (s) *s = (u32)(now % 60);
}

/* Sets the wall clock to a known time of day -- the net's, when a
 * time server answered. The base is adjusted so the running counter
 * lands on the given moment now. */
void time_set_wall(u32 h, u32 m, u32 s)
{
    u64 up = time_ns() / 1000000000ULL;
    u64 want = (u64)h * 3600 + (u64)m * 60 + s;
    boot_wall = (want + 86400ULL * 4 - up % 86400) % 86400;
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
