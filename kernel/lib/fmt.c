/*
 * fmt.c -- the kernel's formatted output.
 */
#include <eb/fmt.h>
#include <eb/io.h>

#define MAX_SINKS 4
static kout_sink sinks[MAX_SINKS];
static u32       sink_count;
static bool      screen_muted;

static u64 (*clock_ns)(void);
static bool at_line_start = true;

void kout_add_sink(kout_sink sink)
{
    if (sink && sink_count < MAX_SINKS)
        sinks[sink_count++] = sink;
}

/* Sink 0 is always the serial port. */
void kout_mute_screen(bool mute) { screen_muted = mute; }

/* Hands the screen over for good. Muting is temporary by design -- the
 * long diagnostics turn it on and off again -- so something that must
 * never come back needs its own door. */
void kout_detach_screen(void)
{
    if (sink_count > 1) sink_count = 1;
}

void kout_set_clock(u64 (*nanoseconds)(void)) { clock_ns = nanoseconds; }

static void raw_putc(char c)
{
    u32 n = screen_muted && sink_count > 0 ? 1 : sink_count;
    for (u32 i = 0; i < n; i++)
        sinks[i](c);
}

/* Decimal number, right-aligned to at least min_width using padc. */
static void raw_num(u64 v, u32 min_width, char padc)
{
    char tmp[24];
    u32 n = 0;

    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }

    for (u32 i = n; i < min_width; i++) raw_putc(padc);
    while (n) raw_putc(tmp[--n]);
}

/* "[    3.148271] " -- seconds and microseconds since the clock was
 * started, in the same shape dmesg uses. */
static void emit_timestamp(void)
{
    u64 ns = clock_ns();
    raw_putc('[');
    raw_num(ns / 1000000000ULL, 5, ' ');
    raw_putc('.');
    raw_num((ns / 1000ULL) % 1000000ULL, 6, '0');
    raw_putc(']');
    raw_putc(' ');
}

void kputc(char c)
{
    /* Blank lines stay blank -- a timestamp on an empty line is noise. */
    if (at_line_start && clock_ns && c != '\n') {
        at_line_start = false;
        emit_timestamp();
    }
    raw_putc(c);
    if (c == '\n') at_line_start = true;
}

void kputs(const char *s)
{
    if (!s) s = "(null)";
    while (*s) kputc(*s++);
}

/* ------------------------------------------------------------------ */

static void pad(u32 count, char c)
{
    while (count--) kputc(c);
}

/* Emits a finished field with padding. With zero padding a minus sign
 * has to come before the zeros, otherwise "00-42" appears. */
static void emit(const char *s, u32 len, u32 width, bool left, bool zero)
{
    u32 fill = (width > len) ? width - len : 0;

    if (left) {
        for (u32 i = 0; i < len; i++) kputc(s[i]);
        pad(fill, ' ');
        return;
    }
    if (zero && len > 0 && s[0] == '-') {
        kputc('-');
        pad(fill, '0');
        for (u32 i = 1; i < len; i++) kputc(s[i]);
        return;
    }
    pad(fill, zero ? '0' : ' ');
    for (u32 i = 0; i < len; i++) kputc(s[i]);
}

static u32 render_u64(u64 v, u32 base, bool upper, char *buf)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[24];
    u32 n = 0;

    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = digits[v % base]; v /= base; }

    for (u32 i = 0; i < n; i++) buf[i] = tmp[n - 1 - i];
    return n;
}

static u32 render_i64(i64 v, char *buf)
{
    if (v >= 0) return render_u64((u64)v, 10, false, buf);
    buf[0] = '-';
    /* Go through u64: -(i64)MIN does not fit in an i64. */
    return 1 + render_u64((u64)(-(v + 1)) + 1u, 10, false, buf + 1);
}

void kvprintf(const char *fmt, va_list ap)
{
    char buf[32];

    while (*fmt) {
        if (*fmt != '%') { kputc(*fmt++); continue; }
        fmt++;

        bool left = false, zero = false;
        for (;;) {
            if      (*fmt == '-') { left = true;  fmt++; }
            else if (*fmt == '0') { zero = true;  fmt++; }
            else break;
        }
        if (left) zero = false;

        u32 width = 0;
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10u + (u32)(*fmt++ - '0');

        /* Length modifiers only decide how wide we pull from the stack. */
        bool wide = false;
        while (*fmt == 'l' || *fmt == 'z' || *fmt == 'h') {
            if (*fmt != 'h') wide = true;
            fmt++;
        }

        u32 len = 0;
        switch (*fmt) {
        case 'd': case 'i':
            len = render_i64(wide ? va_arg(ap, i64) : (i64)va_arg(ap, i32), buf);
            emit(buf, len, width, left, zero);
            break;
        case 'u':
            len = render_u64(wide ? va_arg(ap, u64) : (u64)va_arg(ap, u32),
                             10, false, buf);
            emit(buf, len, width, left, zero);
            break;
        case 'x':
            len = render_u64(wide ? va_arg(ap, u64) : (u64)va_arg(ap, u32),
                             16, false, buf);
            emit(buf, len, width, left, zero);
            break;
        case 'X':
            len = render_u64(wide ? va_arg(ap, u64) : (u64)va_arg(ap, u32),
                             16, true, buf);
            emit(buf, len, width, left, zero);
            break;
        case 'p':
            kputs("0x");
            len = render_u64((u64)va_arg(ap, void *), 16, false, buf);
            emit(buf, len, 16, false, true);
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            /* Field width in characters, not bytes, so columns still
             * line up if the text is not plain ASCII. */
            u32 chars = 0;
            for (const char *p = s; *p; p++)
                if (((u8)*p & 0xC0) != 0x80) chars++;
            u32 fill = (width > chars) ? width - chars : 0;
            if (!left) pad(fill, ' ');
            kputs(s);
            if (left) pad(fill, ' ');
            break;
        }
        case 'c':
            buf[0] = (char)va_arg(ap, i32);
            emit(buf, 1, width, left, false);
            break;
        case '%':
            kputc('%');
            break;
        case 0:
            return;                        /* '%' at the end of the string */
        default:
            kputc('%');
            kputc(*fmt);
            break;
        }
        fmt++;
    }
}

/* One call, one line: interrupts are held off while it goes out, so
 * that two threads' lines do not interleave letter by letter. A line
 * takes a few milliseconds on the serial port; the timer waits. */
void kprintf(const char *fmt, ...)
{
    u64 flags = irq_save();
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    irq_restore(flags);
}
