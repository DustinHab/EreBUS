/*
 * reckon.c -- ring-3 program: appends the answer to every text line ending in "=".
 * - whole numbers, + - * / %, parentheses, unary minus; a line that does not parse gets "?"
 * - no globals, no string literals
 */
#include <eb/types.h>

#pragma clang section text = ".user.reckon" rodata = ".user.reckon.ro"

#define NR_YIELD   1
#define NR_SEND    2
#define NR_RECEIVE 3
#define NR_READ    4
#define NR_WRITE   5

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

static inline void r_yield(void) { sys3(NR_YIELD, 0, 0, 0); }

static inline void r_say(u64 console, u64 w0, u64 w1, u64 w2)
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

#define TEXT_MAX 3000

static i32 byte_at(u64 h, u64 at)
{
    u64 v = sys3(NR_READ, h, at & ~7ULL, 0);
    if (v == (u64)-1) return -1;
    return (i32)((v >> ((at & 7) * 8)) & 0xFF);
}

/* --- the little parser ---------------------------------------------- */

typedef struct {
    const char *s;
    u32  at, end;
    bool bad;
} rd;

static void skip_sp(rd *p)
{
    while (p->at < p->end && p->s[p->at] == ' ') p->at++;
}

static i64 expr(rd *p, u32 depth);

static i64 factor(rd *p, u32 depth)
{
    if (depth > 24) { p->bad = true; return 0; }
    skip_sp(p);
    if (p->at >= p->end) { p->bad = true; return 0; }

    char c = p->s[p->at];
    if (c == '-') { p->at++; return -factor(p, depth + 1); }
    if (c == '(') {
        p->at++;
        i64 v = expr(p, depth + 1);
        skip_sp(p);
        if (p->at < p->end && p->s[p->at] == ')') p->at++;
        else p->bad = true;
        return v;
    }
    if (c < '0' || c > '9') { p->bad = true; return 0; }

    i64 v = 0;
    while (p->at < p->end && p->s[p->at] >= '0' && p->s[p->at] <= '9') {
        v = v * 10 + (p->s[p->at] - '0');
        p->at++;
    }
    return v;
}

static i64 term(rd *p, u32 depth)
{
    i64 v = factor(p, depth);
    for (;;) {
        skip_sp(p);
        if (p->at >= p->end) return v;
        char c = p->s[p->at];
        if (c != '*' && c != '/' && c != '%') return v;
        p->at++;
        i64 r = factor(p, depth);
        if (c == '*') v *= r;
        else if (r == 0) { p->bad = true; return 0; }
        else if (c == '/') v /= r;
        else v %= r;
    }
}

static i64 expr(rd *p, u32 depth)
{
    if (depth > 24) { p->bad = true; return 0; }
    i64 v = term(p, depth);
    for (;;) {
        skip_sp(p);
        if (p->at >= p->end) return v;
        char c = p->s[p->at];
        if (c != '+' && c != '-') return v;
        p->at++;
        i64 r = term(p, depth);
        v = (c == '+') ? v + r : v - r;
    }
}

/* --------------------------------------------------------------------- */

void user_reckon(u64 console, u64 inbox);

void user_reckon(u64 console, u64 inbox)
{
    u8   buf[MSG_BYTES];
    char in[TEXT_MAX];
    char out[TEXT_MAX + 200];

    /* say "point sums at me" */
    r_say(console,
          P8('p','o','i','n','t',' ','s','u'),
          P8('m','s',' ','a','t',' ','m','e'),
          P8(' ',' ',' ',' ',' ',' ',' ',' '));

    for (;;) {
        if (msg_receive(inbox, buf) != 0) { r_yield(); continue; }
        if (msg_ncaps(buf) == 0) continue;
        u64 text = msg_u64(buf, MSG_CAP0);

        /* The whole text, up to what fits. */
        u32 len = 0;
        while (len < TEXT_MAX - 1) {
            i32 c = byte_at(text, len);
            if (c <= 0) break;
            in[len++] = (char)c;
        }

        /* Line by line: what ends in "=" gets its answer. */
        u32 o = 0, i = 0, answered = 0;
        bool room = true;
        while (i < len && room) {
            u32 ls = i;
            while (i < len && in[i] != '\n') i++;
            u32 le = i;                      /* line is [ls, le) */

            u32 ce = le;
            while (ce > ls && in[ce - 1] == ' ') ce--;

            bool asks = (ce > ls && in[ce - 1] == '=');

            for (u32 k = ls; k < le && o < sizeof(out) - 32; k++)
                out[o++] = in[k];

            if (asks && o < sizeof(out) - 32) {
                rd p;
                p.s = in;
                p.at = ls;
                p.end = ce - 1;              /* before the "=" */
                p.bad = false;
                i64 v = expr(&p, 0);
                skip_sp(&p);
                if (p.at != p.end) p.bad = true;

                out[o++] = ' ';
                if (p.bad) {
                    out[o++] = '?';
                } else {
                    u64 mag = v < 0 ? (u64)-v : (u64)v;
                    char dg[24];
                    u32 nd = 0;
                    if (mag == 0) dg[nd++] = '0';
                    while (mag) { dg[nd++] = (char)('0' + mag % 10); mag /= 10; }
                    if (v < 0) out[o++] = '-';
                    while (nd) out[o++] = dg[--nd];
                }
                answered++;
            }

            if (i < len) out[o++] = '\n';
            i++;
            if (o >= sizeof(out) - 40) room = false;
        }

        if (answered == 0) {
            /* say "nothing asked in it" */
            r_say(console,
                  P8('n','o','t','h','i','n','g',' '),
                  P8('a','s','k','e','d',' ','i','n'),
                  P8(' ','i','t',' ',' ',' ',' ',' '));
            continue;
        }

        /* Write the reworked text back, eight bytes at a stride; a
         * refusal on the way means the text has no room or no write,
         * and the console says so instead of half a text staying. */
        bool ok = true;
        for (u32 w = 0; w < o + 1 && ok; w += 8) {
            u64 word = 0;
            for (u32 b = 0; b < 8; b++) {
                u32 idx = w + b;
                u8 ch = idx < o ? (u8)out[idx] : 0;
                word |= (u64)ch << (b * 8);
            }
            if (sys3(NR_WRITE, text, w, word) != 0) ok = false;
        }

        if (ok)
            /* say "reckoned; look at it" */
            r_say(console,
                  P8('r','e','c','k','o','n','e','d'),
                  P8(';',' ','l','o','o','k',' ','a'),
                  P8('t',' ','i','t',' ',' ',' ',' '));
        else
            /* say "it has no room for me" */
            r_say(console,
                  P8('i','t',' ','h','a','s',' ','n'),
                  P8('o',' ','r','o','o','m',' ','f'),
                  P8('o','r',' ','m','e',' ',' ',' '));
    }
}
