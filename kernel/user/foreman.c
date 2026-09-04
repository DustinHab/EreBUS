/*
 * foreman.c -- ring-3 program: hands tasks to the desk over the wire and watches the task's text for the answer.
 * - "again N" in a task's first line repeats it every N seconds; the desk strips old answers
 * - holds the wire (send-only, given at start) and the tasks pointed at it, nothing else
 * - mapped read-only into ring 3: no globals, no string literals
 */
#include <eb/types.h>

#pragma clang section text = ".user.foreman" rodata = ".user.foreman.ro"

#define NR_YIELD   1
#define NR_SEND    2
#define NR_RECEIVE 3
#define NR_READ    4
#define NR_PASS    6
#define NR_CLOCK   7

#define TAG_TEXT 0x54584554ULL
#define TAG_WORK 0x4B524F57ULL

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

static inline void f_yield(void) { sys3(NR_YIELD, 0, 0, 0); }

static inline void f_say(u64 console, u64 w0, u64 w1, u64 w2)
{
    sys5(NR_SEND, console, TAG_TEXT, w0, w1, w2);
}

#define MSG_BYTES  80
#define MSG_NCAPS  12
#define MSG_CAP0   48

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

static u64 f_clock(void)
{
    return sys3(NR_CLOCK, 0, 0, 0);
}

static u64 gone_since(u64 from)
{
    return (f_clock() + 86400 - from) % 86400;
}

static void f_rest(i64 seconds)
{
    if (seconds <= 0) return;
    if (seconds > 86399) seconds = 86399;
    u64 from = f_clock();
    while ((i64)gone_since(from) < seconds) f_yield();
}

/* One byte of a held text, or below zero past its edge. */
static i32 byte_at(u64 h, u64 at)
{
    u64 v = sys3(NR_READ, h, at & ~7ULL, 0);
    if (v == (u64)-1) return -1;
    return (i32)((v >> ((at & 7) * 8)) & 0xFF);
}

/* How far the words reach. Text never holds 0xFF, so a word of it
 * cannot be mistaken for the refusal. */
static u64 text_size(u64 h)
{
    u64 off = 0;
    for (;;) {
        u64 v = sys3(NR_READ, h, off, 0);
        if (v == (u64)-1) return off;
        for (u32 i = 0; i < 8; i++)
            if (((v >> (i * 8)) & 0xFF) == 0) return off + i;
        off += 8;
        if (off >= 65536) return off;
    }
}

/* Whether the text now ends in a written answer: its last full line
 * starting "= " is the desk's hand. */
static bool ends_answered(u64 h, u64 len)
{
    u64 end = len;
    while (end > 0 && byte_at(h, end - 1) == '\n') end--;
    if (end < 2) return false;
    u64 start = end;
    while (start > 0 && byte_at(h, start - 1) != '\n') start--;
    return byte_at(h, start) == '=' && byte_at(h, start + 1) == ' ';
}

/* "again N" in the first line: hand the task in anew every N
 * seconds. Zero when the line says nothing of the kind. */
static i64 first_line_again(u64 h)
{
    i32 line_end = 0;
    while (line_end < 96) {
        i32 c = byte_at(h, (u64)line_end);
        if (c <= 0 || c == '\n') break;
        line_end++;
    }
    for (i32 i = 0; i + 5 < line_end; i++) {
        if (byte_at(h, (u64)i)     != 'a') continue;
        if (byte_at(h, (u64)i + 1) != 'g') continue;
        if (byte_at(h, (u64)i + 2) != 'a') continue;
        if (byte_at(h, (u64)i + 3) != 'i') continue;
        if (byte_at(h, (u64)i + 4) != 'n') continue;

        i32 p = i + 5;
        while (p < line_end && byte_at(h, (u64)p) == ' ') p++;
        i64 n = 0;
        bool any = false;
        while (p < line_end) {
            i32 c = byte_at(h, (u64)p);
            if (c < '0' || c > '9') break;
            n = n * 10 + (c - '0');
            any = true;
            p++;
        }
        if (any) return n;
    }
    return 0;
}

void user_foreman(u64 console, u64 inbox);

void user_foreman(u64 console, u64 inbox)
{
    u8 buf[MSG_BYTES];
    u64 wire = 0;

    /* say "i wait for tasks" */
    f_say(console,
          P8('i',' ','w','a','i','t',' ','f'),
          P8('o','r',' ','t','a','s','k','s'),
          P8(' ',' ',' ',' ',' ',' ',' ',' '));

    for (;;) {
        if (msg_receive(inbox, buf) != 0) { f_yield(); continue; }
        if (msg_ncaps(buf) == 0) continue;

        u64 cap = msg_u64(buf, MSG_CAP0);

        /* The first thing given is the wire; everything after is a
         * task to be seen through. */
        if (wire == 0) {
            wire = cap;
            /* say "the wire is in hand" */
            f_say(console,
                  P8('t','h','e',' ','w','i','r','e'),
                  P8(' ','i','s',' ','i','n',' ','h'),
                  P8('a','n','d',' ',' ',' ',' ',' '));
            continue;
        }

        u64 task = cap;
        for (;;) {
            u64 before = text_size(task);

            if (sys5(NR_PASS, wire, TAG_WORK, task, 3, 0) != 0) {
                /* say "the wire refused me" */
                f_say(console,
                      P8('t','h','e',' ','w','i','r','e'),
                      P8(' ','r','e','f','u','s','e','d'),
                      P8(' ','m','e',' ',' ',' ',' ',' '));
                break;
            }
            /* say "task handed to the desk" */
            f_say(console,
                  P8('t','a','s','k',' ','h','a','n'),
                  P8('d','e','d',' ','t','o',' ','t'),
                  P8('h','e',' ','d','e','s','k',' '));

            /* Watch the task itself: the desk writes the answer into
             * it, and the writing is the signal. */
            u64 from = f_clock();
            bool done = false;
            while ((i64)gone_since(from) < 300) {
                f_rest(1);
                u64 now_len = text_size(task);
                if (now_len > before && ends_answered(task, now_len)) {
                    done = true;
                    break;
                }
            }

            if (!done) {
                /* say "the desk stayed silent" */
                f_say(console,
                      P8('t','h','e',' ','d','e','s','k'),
                      P8(' ','s','t','a','y','e','d',' '),
                      P8('s','i','l','e','n','t',' ',' '));
                break;
            }

            /* say "the task is answered" */
            f_say(console,
                  P8('t','h','e',' ','t','a','s','k'),
                  P8(' ','i','s',' ','a','n','s','w'),
                  P8('e','r','e','d',' ',' ',' ',' '));

            i64 again = first_line_again(task);
            if (again <= 0) break;
            f_rest(again);
        }
    }
}
