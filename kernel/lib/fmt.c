/*
 * fmt.c -- formatierte Ausgabe des Kernels.
 */
#include <eb/fmt.h>

#define MAX_SINKS 4
static kout_sink sinks[MAX_SINKS];
static u32       sink_count;
static bool      screen_muted;

void kout_add_sink(kout_sink sink)
{
    if (sink && sink_count < MAX_SINKS)
        sinks[sink_count++] = sink;
}

/* Senke 0 ist immer die serielle Schnittstelle. Beim Ausgeben langer
 * Diagnosen will man den Bildschirm nicht vollschreiben -- damit geht
 * alles Weitere nur noch ans Kabel. */
void kout_mute_screen(bool mute) { screen_muted = mute; }

void kputc(char c)
{
    u32 n = screen_muted && sink_count > 0 ? 1 : sink_count;
    for (u32 i = 0; i < n; i++)
        sinks[i](c);
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

/* Gibt ein fertiges Feld mit Auffuellung aus. Bei Null-Auffuellung muss
 * ein Minuszeichen vor die Nullen, sonst steht "00-42" auf dem Schirm. */
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

/* Zahl rueckwaerts in den Puffer schreiben und die Laenge liefern. */
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
    /* Ueber u64 gehen: -(i64)MIN passt nicht in i64. */
    return 1 + render_u64((u64)(-(v + 1)) + 1u, 10, false, buf + 1);
}

void kvprintf(const char *fmt, va_list ap)
{
    char buf[32];

    while (*fmt) {
        if (*fmt != '%') { kputc(*fmt++); continue; }
        fmt++;

        /* Flaggen: '-' linksbuendig, '0' mit Nullen auffuellen. */
        bool left = false, zero = false;
        for (;;) {
            if      (*fmt == '-') { left = true;  fmt++; }
            else if (*fmt == '0') { zero = true;  fmt++; }
            else break;
        }
        if (left) zero = false;   /* Nullen rechts waeren sinnlos */

        u32 width = 0;
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10u + (u32)(*fmt++ - '0');

        /* Laengenangaben bestimmen nur, wie breit wir vom Stapel holen. */
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
            /* Feldbreite in Zeichen, nicht in Bytes -- sonst rutschen
             * Spalten mit Umlauten auseinander. */
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
            return;                        /* '%' am Ende der Zeichenkette */
        default:
            kputc('%');
            kputc(*fmt);
            break;
        }
        fmt++;
    }
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}
