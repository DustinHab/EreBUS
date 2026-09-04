/*
 * pulse.c -- ring-3 program: paints one column per second into a picture (memory as blue bar, processor share as red mark, grey cursor).
 * - inputs: a writable picture and the readable activity page; which is which is found out by trying
 */
#include <eb/types.h>

#pragma clang section text = ".user.pulse" rodata = ".user.pulse.ro"

#define NR_YIELD   1
#define NR_SEND    2
#define NR_RECEIVE 3
#define NR_READ    4
#define NR_WRITE   5
#define NR_CLOCK   7

#define TAG_TEXT 0x54584554ULL

#define P8(a, b, c, d, e, f, g, h) \
    ((u64)(u8)(a) | ((u64)(u8)(b) << 8) | ((u64)(u8)(c) << 16) | \
     ((u64)(u8)(d) << 24) | ((u64)(u8)(e) << 32) | ((u64)(u8)(f) << 40) | \
     ((u64)(u8)(g) << 48) | ((u64)(u8)(h) << 56))

static inline u64 sys3(u64 nr, u64 a0, u64 a1, u64 a2)
{
    __asm__ volatile ("syscall"
                      : "+a"(nr), "+D"(a0), "+S"(a1), "+d"(a2)
                      :
                      : "rcx", "r11", "r8", "r9", "r10", "memory");
    return nr;
}

static inline u64 sys5(u64 nr, u64 a0, u64 a1, u64 a2, u64 a3, u64 a4)
{
    register u64 r10 __asm__("r10") = a3;
    register u64 r8  __asm__("r8")  = a4;
    __asm__ volatile ("syscall"
                      : "+a"(nr), "+D"(a0), "+S"(a1), "+d"(a2),
                        "+r"(r10), "+r"(r8)
                      :
                      : "rcx", "r11", "r9", "memory");
    return nr;
}

static inline void p_yield(void) { sys3(NR_YIELD, 0, 0, 0); }

static inline void p_say(u64 console, u64 w0, u64 w1, u64 w2)
{
    sys5(NR_SEND, console, TAG_TEXT, w0, w1, w2);
}

#define MSG_BYTES 80
#define MSG_NCAPS 12
#define MSG_CAP0  48

static u64 msg_receive(u64 inbox, u8 *buf)
{
    for (u32 i = 0; i < MSG_BYTES; i++) buf[i] = 0;
    return sys3(NR_RECEIVE, inbox, (u64)buf, 0);
}

static u32 msg_ncaps(const u8 *buf)
{
    return (u32)buf[MSG_NCAPS] | ((u32)buf[MSG_NCAPS + 1] << 8);
}

static u64 msg_u64(const u8 *buf, u32 at)
{
    u64 v = 0;
    for (u32 i = 0; i < 8; i++) v |= (u64)buf[at + i] << (i * 8);
    return v;
}

static i32 byte_at(u64 h, u64 at)
{
    u64 v = sys3(NR_READ, h, at & ~7ULL, 0);
    if (v == (u64)-1) return -1;
    return (i32)((v >> ((at & 7) * 8)) & 0xFF);
}

static bool byte_put(u64 h, u64 at, u8 val)
{
    u64 word = sys3(NR_READ, h, at & ~7ULL, 0);
    if (word == (u64)-1) return false;
    u32 shift = (u32)(at & 7) * 8;
    word = (word & ~((u64)0xFF << shift)) | ((u64)val << shift);
    return sys3(NR_WRITE, h, at & ~7ULL, word) == 0;
}

static u64 p_clock(void)
{
    return sys3(NR_CLOCK, 0, 0, 0);
}

/* Finds a word in the page and answers the number after it, walking
 * byte by byte -- the page is small and rewritten every second, so
 * nothing is worth caching. */
static i64 number_after(u64 page, const char *word, u32 wlen, u32 skip)
{
    for (u32 at = 0; at < 2000; at++) {
        u32 j = 0;
        while (j < wlen) {
            i32 c = byte_at(page, at + j);
            if (c < 0) return -1;
            if ((char)c != word[j]) break;
            j++;
        }
        if (j < wlen) continue;

        u32 p = at + wlen;
        for (u32 s = 0; s <= skip; s++) {
            while (byte_at(page, p) == ' ') p++;
            i64 v = 0;
            bool any = false;
            for (;;) {
                i32 c = byte_at(page, p);
                if (c < '0' || c > '9') break;
                v = v * 10 + (c - '0');
                any = true;
                p++;
            }
            if (!any) return -1;
            if (s == skip) return v;
            while (byte_at(page, p) != ' ' && byte_at(page, p) > 0 &&
                   (byte_at(page, p) < '0' || byte_at(page, p) > '9'))
                p++;
        }
    }
    return -1;
}

void user_pulse(u64 console, u64 inbox);

void user_pulse(u64 console, u64 inbox)
{
    u8  buf[MSG_BYTES];
    u64 canvas = 0, page = 0;

    /* say "a picture, then the page" */
    p_say(console,
          P8('w','a','i','t','i','n','g',' '),
          P8('f','o','r',' ','p','i','c','t'),
          P8('u','r','e',',',' ','p','a','g'));

    while (!canvas || !page) {
        if (msg_receive(inbox, buf) != 0) { p_yield(); continue; }
        if (msg_ncaps(buf) == 0) continue;
        u64 cap = msg_u64(buf, MSG_CAP0);

        /* The one that lets us write is the canvas; the readable one
         * is the page. Found out by trying, like everything. */
        u64 probe = sys3(NR_READ, cap, 0, 0);
        if (probe == (u64)-1) continue;
        if (sys3(NR_WRITE, cap, 0, probe) == 0) canvas = cap;
        else page = cap;
    }

    /* say "i draw the pulse now" */
    p_say(console,
          P8('d','r','a','w','i','n','g',' '),
          P8(' ',' ',' ',' ',' ',' ',' ',' '),
          P8(' ',' ',' ',' ',' ',' ',' ',' '));

    /* The picture's shape, from its own header. */
    u64 w = 0, h = 0;
    for (u32 i = 0; i < 4; i++) {
        w |= (u64)(u8)byte_at(canvas, i) << (i * 8);
        h |= (u64)(u8)byte_at(canvas, 4 + i) << (i * 8);
    }
    if (w == 0 || h == 0 || w > 4096 || h > 4096) {
        /* say "that is no picture" */
        p_say(console,
              P8('n','o','t',' ','a',' ','p','i'),
              P8('c','t','u','r','e',' ',' ',' '),
              P8(' ',' ',' ',' ',' ',' ',' ',' '));
        sys3(0, 0, 0, 0);
        for (;;) p_yield();
    }

    u64 col = 0;
    u64 last = p_clock();

    /* Built on the stack, never in rodata: a static here would land
     * in the kernel's half, outside this program's window -- the same
     * trap the no-string-literals rule guards against. */
    char cpu_word[3];
    cpu_word[0] = 'c'; cpu_word[1] = 'p'; cpu_word[2] = 'u';
    char mem_word[6];
    mem_word[0] = 'm'; mem_word[1] = 'e'; mem_word[2] = 'm';
    mem_word[3] = 'o'; mem_word[4] = 'r'; mem_word[5] = 'y';

    for (;;) {
        /* One column every other second -- the breath in between is
         * what lets persistence find a quiet moment to save in. A
         * canvas touched every second would never be still. */
        while ((p_clock() + 86400 - last) % 86400 < 2) p_yield();
        last = p_clock();

        i64 busy = number_after(page, cpu_word, 3, 0);
        i64 used = number_after(page, mem_word, 6, 0);
        i64 total = number_after(page, mem_word, 6, 1);
        if (busy < 0) busy = 0;
        if (busy > 100) busy = 100;
        if (total <= 0) { used = 0; total = 1; }

        u64 bar = (u64)used * h / (u64)total;
        if (bar > h) bar = h;

        for (u64 y = 0; y < h; y++) {
            u8 ink = 0;
            if (y >= h - bar) ink = 8;               /* memory, blue */
            byte_put(canvas, 8 + y * w + col, ink);
        }
        u64 cy = h - 1 - (u64)busy * (h - 1) / 100;
        byte_put(canvas, 8 + cy * w + col, 3);       /* the cpu, red */

        u64 next = (col + 1) % w;
        for (u64 y = 0; y < h; y++)                  /* the cursor */
            byte_put(canvas, 8 + y * w + next, 2);

        col = next;
    }
}
