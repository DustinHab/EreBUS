/*
 * runner.c -- the program that runs what a text says.
 *
 * This is how programs are written inside the system: an ordinary text
 * object, handed to an ordinary process. The runner is the process; the
 * text is the program. The first thing the runner is given is its
 * words, held read-only, and it reads them line by line as it goes --
 * which means the text stays live. Edit a running script and the next
 * pass through a line runs the new words. There is no build step, no
 * load step, and nothing installed: cut the reference and the program
 * is words again.
 *
 * The language is one page. A line is a word and its operands:
 *
 *   note ...         a remark; the line does nothing
 *   say <words>      speak up to 24 letters to the console
 *   wait             sleep until the next gift or message arrives
 *   set x <n|v>      variables a..z hold signed numbers
 *   add sub mul div  arithmetic onto a variable
 *   get x <n|v>      x = eight bytes of "it", at that offset
 *   put x <n|v>      write x into "it" there
 *   if x < <n|v>     also = and >; when false, the next line is skipped
 *   skip <n>         jump n lines forward
 *   back <n>         jump n lines back
 *   stop             the end
 *
 * "it" is the latest thing the runner was given after its words. The
 * outcome of get and put lands in r: zero for done, below zero for
 * refused -- and refusal is not an error, it is the kernel declining to
 * produce an object for a right the script does not hold. A script
 * finds out what it may do the same way every other program here does:
 * by trying.
 *
 * This file is compiled into the kernel image but never runs as the
 * kernel: the section pragma below puts it among the user programs,
 * mapped read-and-execute into ring 3. Hence its two disciplines. No
 * globals -- the mapping is read-only, so all state lives on the
 * stack. And no string literals -- they would land in the kernel's
 * rodata, outside the mapped window; every message is packed into
 * plain integer constants instead, the same way the assembly
 * programs carry theirs.
 */
#include <eb/types.h>

#pragma clang section text = ".user.runner" rodata = ".user.runner.ro"

#define NR_EXIT    0
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

/* The system call, spelled out once per arity. The kernel zeroes the
 * scratch registers on the way back, so everything volatile is named
 * as clobbered and the callee-saved world is left to the compiler. */
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

/* The message as SYS_RECEIVE lays it down: tag, counts, words, then
 * the capabilities as fresh handles. Offsets, not a shared header --
 * this side of the wall carries its own knowledge. */
#define MSG_BYTES  80
#define MSG_NCAPS  12
#define MSG_WORD0  16
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

/* One byte of the script. The words are read fresh on every pass, so
 * an edited text changes the running program at the next line. A
 * refused read answers as the end of the text. */
static i32 script_byte(u64 words, u64 at)
{
    u64 v = sys3(NR_READ, words, at & ~7ULL, 0);
    if (v == (u64)-1) return -1;
    u8 b = (u8)(v >> ((at & 7) * 8));
    return b ? (i32)b : -1;
}

#define LINE_MAX 96

/* Collects line number `ln` into out[], counting from zero. Answers
 * its length, or below zero when the text has no such line. */
static i32 script_line(u64 words, u32 ln, char *out)
{
    u64 at = 0;
    u32 line = 0;

    while (line < ln) {
        i32 c = script_byte(words, at);
        if (c < 0) return -1;
        at++;
        if (c == '\n') line++;
    }

    i32 n = 0;
    for (;;) {
        i32 c = script_byte(words, at + (u64)n);
        if (c < 0) {
            if (n == 0 && ln > 0) return -1;   /* past the end */
            break;
        }
        if (c == '\n') break;
        if (n < LINE_MAX - 1) out[n++] = (char)c;
        else break;
    }
    out[n] = 0;
    return n;
}

/* The first word of a line, packed low-byte first -- the same shape
 * the P8 constants have, so a comparison is one integer against
 * another and no string lives anywhere. */
static u64 word_at(const char *s, i32 *pos)
{
    i32 p = *pos;
    while (s[p] == ' ') p++;
    u64 w = 0;
    u32 n = 0;
    while (s[p] && s[p] != ' ') {
        if (n < 8) w |= (u64)(u8)s[p] << (n * 8);
        n++;
        p++;
    }
    *pos = p;
    return w;
}

/* An operand: a single letter names a variable, digits (with an
 * optional minus) name themselves. ok goes false on anything else. */
static i64 operand(const char *s, i32 *pos, const i64 *v, bool *ok)
{
    i32 p = *pos;
    while (s[p] == ' ') p++;

    if (s[p] >= 'a' && s[p] <= 'z' &&
        (s[p + 1] == 0 || s[p + 1] == ' ')) {
        i64 got = v[s[p] - 'a'];
        *pos = p + 1;
        return got;
    }

    bool neg = false;
    if (s[p] == '-') { neg = true; p++; }
    if (s[p] < '0' || s[p] > '9') { *ok = false; return 0; }

    i64 n = 0;
    while (s[p] >= '0' && s[p] <= '9') {
        n = n * 10 + (s[p] - '0');
        p++;
    }
    *pos = p;
    return neg ? -n : n;
}

/* Packs the rest of the line into three words for the console. */
static void say_rest(u64 console, const char *s, i32 pos)
{
    while (s[pos] == ' ') pos++;
    u64 w[3] = { 0, 0, 0 };
    u32 n = 0;
    while (s[pos] && n < 24) {
        w[n / 8] |= (u64)(u8)s[pos] << ((n % 8) * 8);
        n++;
        pos++;
    }
    r_say(console, w[0], w[1], w[2]);
}

/* What a line can tell the loop. */
#define FLOW_WAIT  (-1)
#define FLOW_STOP  (-2)
#define FLOW_WRONG (-3)

/* Runs one line. Answers the next line number, or a FLOW_ code. */
static i64 exec_line(const char *s, u32 ln, u64 console,
                     i64 *v, u64 it, bool *skip)
{
    i32 pos = 0;
    u64 w = word_at(s, &pos);

    /* A pending skip eats exactly the next line, whatever it is --
     * counting lines as written is the whole promise of skip. */
    if (*skip) { *skip = false; return (i64)ln + 1; }

    if (w == 0) return (i64)ln + 1;                       /* blank */

    if (w == P8('n','o','t','e',0,0,0,0)) return (i64)ln + 1;
    if (w == P8('s','t','o','p',0,0,0,0)) return FLOW_STOP;
    if (w == P8('w','a','i','t',0,0,0,0)) return FLOW_WAIT;

    if (w == P8('s','a','y',0,0,0,0,0)) {
        say_rest(console, s, pos);
        return (i64)ln + 1;
    }

    if (w == P8('s','k','i','p',0,0,0,0) ||
        w == P8('b','a','c','k',0,0,0,0)) {
        bool ok = true;
        i64 n = operand(s, &pos, v, &ok);
        if (!ok || n < 0) return FLOW_WRONG;
        if (w == P8('b','a','c','k',0,0,0,0))
            return n > (i64)ln ? 0 : (i64)ln - n;
        return (i64)ln + 1 + n;
    }

    if (w == P8('s','e','t',0,0,0,0,0) ||
        w == P8('a','d','d',0,0,0,0,0) ||
        w == P8('s','u','b',0,0,0,0,0) ||
        w == P8('m','u','l',0,0,0,0,0) ||
        w == P8('d','i','v',0,0,0,0,0)) {
        while (s[pos] == ' ') pos++;
        if (s[pos] < 'a' || s[pos] > 'z') return FLOW_WRONG;
        i32 x = s[pos] - 'a';
        pos++;

        bool ok = true;
        i64 n = operand(s, &pos, v, &ok);
        if (!ok) return FLOW_WRONG;

        if (w == P8('s','e','t',0,0,0,0,0)) v[x] = n;
        if (w == P8('a','d','d',0,0,0,0,0)) v[x] += n;
        if (w == P8('s','u','b',0,0,0,0,0)) v[x] -= n;
        if (w == P8('m','u','l',0,0,0,0,0)) v[x] *= n;
        if (w == P8('d','i','v',0,0,0,0,0)) v[x] = n ? v[x] / n : 0;
        return (i64)ln + 1;
    }

    if (w == P8('g','e','t',0,0,0,0,0) ||
        w == P8('p','u','t',0,0,0,0,0)) {
        while (s[pos] == ' ') pos++;
        if (s[pos] < 'a' || s[pos] > 'z') return FLOW_WRONG;
        i32 x = s[pos] - 'a';
        pos++;

        bool ok = true;
        i64 off = operand(s, &pos, v, &ok);
        if (!ok || off < 0) return FLOW_WRONG;

        if (w == P8('g','e','t',0,0,0,0,0)) {
            u64 got = sys3(NR_READ, it, (u64)off, 0);
            if (got == (u64)-1) { v['r' - 'a'] = -1; }
            else { v[x] = (i64)got; v['r' - 'a'] = 0; }
        } else {
            u64 res = sys3(NR_WRITE, it, (u64)off, (u64)v[x]);
            v['r' - 'a'] = (res == 0) ? 0 : -1;
        }
        return (i64)ln + 1;
    }

    if (w == P8('i','f',0,0,0,0,0,0)) {
        while (s[pos] == ' ') pos++;
        if (s[pos] < 'a' || s[pos] > 'z') return FLOW_WRONG;
        i64 left = v[s[pos] - 'a'];
        pos++;

        while (s[pos] == ' ') pos++;
        char op = s[pos];
        if (op != '<' && op != '=' && op != '>') return FLOW_WRONG;
        pos++;

        bool ok = true;
        i64 right = operand(s, &pos, v, &ok);
        if (!ok) return FLOW_WRONG;

        bool holds = (op == '<') ? (left < right)
                   : (op == '>') ? (left > right)
                                 : (left == right);
        if (!holds) *skip = true;
        return (i64)ln + 1;
    }

    return FLOW_WRONG;
}

/* "line NN: i do not know" -- built digit by digit, because there is
 * no string to point at from here. */
static void complain(u64 console, u32 ln)
{
    u32 shown = ln + 1;                    /* people count from one */
    if (shown > 99) shown = 99;
    u64 d1 = (shown >= 10) ? ('0' + shown / 10) : ' ';
    u64 d2 = '0' + shown % 10;

    r_say(console,
          P8('l','i','n','e',' ',0,0,0) | (d1 << 40) | (d2 << 48) |
              ((u64)':' << 56),
          P8(' ','i',' ','d','o',' ','n','o'),
          P8('t',' ','k','n','o','w',' ',' '));
}

void user_runner(u64 console, u64 inbox);

void user_runner(u64 console, u64 inbox)
{
    u8  buf[MSG_BYTES];
    i64 v[26];
    for (u32 i = 0; i < 26; i++) v[i] = 0;

    /* say(console, "i run wh", "at i am ", "shown   ") */
    r_say(console,
          P8('i',' ','r','u','n',' ','w','h'),
          P8('a','t',' ','i',' ','a','m',' '),
          P8('s','h','o','w','n',' ',' ',' '));

    /* The first gift is the program. Until it arrives there is
     * nothing to do and nothing to know. */
    u64 words = 0;
    while (!words) {
        if (msg_receive(inbox, buf) != 0) { r_yield(); continue; }
        if (msg_ncaps(buf) > 0) words = msg_u64(buf, MSG_CAP0);
    }

    u64  it = 0;
    u32  ln = 0;
    bool skip = false;
    u32  steps = 0;
    char line[LINE_MAX];

    for (;;) {
        /* A spinning script is preempted anyway; yielding on top of
         * that keeps it a polite neighbour rather than a warm one. */
        if (++steps >= 128) { steps = 0; r_yield(); }

        if (script_line(words, ln, line) < 0) break;   /* the end */

        i64 next = exec_line(line, ln, console, v, it, &skip);

        if (next == FLOW_STOP) break;

        if (next == FLOW_WRONG) {
            complain(console, ln);
            break;
        }

        if (next == FLOW_WAIT) {
            if (msg_receive(inbox, buf) == 0) {
                if (msg_ncaps(buf) > 0) it = msg_u64(buf, MSG_CAP0);
                v['m' - 'a'] = (i64)msg_u64(buf, MSG_WORD0);
                ln++;
            } else {
                r_yield();                 /* try the same line again */
            }
            continue;
        }

        ln = (u32)next;
    }

    sys3(NR_EXIT, 0, 0, 0);
    for (;;) r_yield();                    /* exit does not return */
}
