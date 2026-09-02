/* proof.c -- the compiler's list, worked through and checked.
 *
 * Each check says its number and ok or bad on the console; the last
 * line says how many were bad. Run on the machine, read in the
 * journal or the serial log.
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include "words.h"

#define SQUARE(x) ((x) * (x))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define STR(x) #x
#define GLUE(a, b) a##b

#if defined(NOTHING) || 1 + 1 != 2
#error "the preprocessor cannot count"
#endif

#if SQUARE(3) == 9 && MAX(2, 7) == 7
#define ARITH_OK 1
#elif 1
#define ARITH_OK 0
#else
#define ARITH_OK 0
#endif

typedef struct __attribute__((packed)) {
    uint8_t  a;
    uint32_t b;
    uint16_t c;
} packed_t;

typedef struct {
    uint8_t  a;
    uint32_t b;
    uint16_t c;
} loose_t;

_Static_assert(sizeof(packed_t) == 7, "packed struct");
_Static_assert(sizeof(loose_t) == 12, "padded struct");

typedef union {
    uint32_t whole;
    uint8_t  bytes[4];
} view_t;

struct flags {
    unsigned int kind : 3;
    unsigned int on   : 1;
    unsigned int level : 4;
    int          delta : 5;
};

typedef long (*op_fn)(long, long);

static long add(long a, long b) { return a + b; }
static long mul(long a, long b) { return a * b; }

static const char *const names[] = { "zero", "one", "two" };
static long table[4] = { 10, 20, 30 };
static loose_t seed = { .a = 7, .c = 300, .b = 70000 };
static op_fn ops[2] = { add, mul };

long console_handle;
long bad_count;

static void say(char *s)
{
    long w[3];
    char *b = (char *)w;
    long i;
    for (i = 0; i < 24; i++) b[i] = ' ';
    for (i = 0; i < 24 && s[i]; i++) b[i] = s[i];
    /* The console's letter box holds only so many lines at once; a
     * refused send is asked again after yielding, so that a quick
     * program does not outrun the console and lose its words. */
    while (syscall(2, console_handle, 0x54584554, w[0], w[1], w[2]) != 0)
        syscall(1, 0, 0, 0, 0, 0);
}

static void check(long n, bool ok)
{
    char line[24];
    line[0] = 'c'; line[1] = 'h'; line[2] = 'e'; line[3] = 'c'; line[4] = 'k';
    line[5] = ' ';
    line[6] = (char)('0' + n / 10);
    line[7] = (char)('0' + n % 10);
    line[8] = ' ';
    if (ok) { line[9] = 'o'; line[10] = 'k'; line[11] = 0; }
    else { line[9] = 'b'; line[10] = 'a'; line[11] = 'd'; line[12] = 0; bad_count++; }
    say(line);
}

static long sum_var(long count, ...)
{
    va_list ap;
    long s = 0;
    va_start(ap, count);
    while (count-- > 0) s += va_arg(ap, long);
    va_end(ap);
    return s;
}

static double average(double a, double b, double c)
{
    return (a + b + c) / 3.0;
}

static long counted(void)
{
    static long calls = 0;
    return ++calls;
}

typedef struct { long x, y; unsigned char tag; } pair_t;

static pair_t make_pair(long a, long b)
{
    pair_t p;
    p.x = a; p.y = b; p.tag = 9;
    return p;
}

typedef struct { long a, b, c; } triple_t;
typedef struct { char k; } tiny_t;

/* the callee's changes stay with its own copy */
static long sum_triple(triple_t t, long k)
{
    t.a += k;
    return t.a + t.b + t.c;
}

static triple_t bump(triple_t t)
{
    t.c++;
    return t;
}

static long many(long a, long b, long c, long d, long e, triple_t t, long f)
{
    return a + b + c + d + e + f + t.a * 100 + t.b * 10 + t.c;
}

static long tiny(tiny_t t) { return t.k; }

long main(long console, long inbox)
{
    console_handle = console;
    bad_count = 0;

    /* 1: function-like macros and #if arithmetic */
    check(1, SQUARE(4) == 16 && MAX(3, 9) == 9 && ARITH_OK);
    check(2, STR(hello)[0] == 'h' && GLUE(1, 2) == 12);

    /* 3, 4: packed and padded structs, real 16-bit fields */
    packed_t p; p.a = 1; p.b = 0x11223344; p.c = 0x5566;
    uint8_t *pb = (uint8_t *)&p;
    check(3, pb[1] == 0x44 && pb[4] == 0x11 && pb[5] == 0x66 && sizeof(p) == 7);
    check(4, seed.a == 7 && seed.b == 70000 && seed.c == 300 && sizeof(loose_t) == 12);

    /* 5: union */
    view_t v; v.whole = 0x04030201;
    check(5, v.bytes[0] == 1 && v.bytes[3] == 4 && sizeof(view_t) == 4);

    /* 6, 7: bit fields, unsigned and signed */
    struct flags f; f.kind = 5; f.on = 1; f.level = 9; f.delta = -3;
    check(6, f.kind == 5 && f.on == 1 && f.level == 9 && sizeof(f) == 4);
    f.level += 2; f.kind = 7; f.on = 0;
    check(7, f.level == 11 && f.kind == 7 && f.on == 0 && f.delta == -3);

    /* 8: initializers, designated and not, pointers to strings */
    check(8, table[1] == 20 && table[3] == 0 && names[2][1] == 'w' && ops[1](6, 7) == 42);
    long local[3] = { 1, 2, 3 };
    loose_t l2 = { .c = 5 };
    check(9, local[0] + local[1] + local[2] == 6 && l2.a == 0 && l2.c == 5);

    /* 10: varargs */
    check(10, sum_var(4, 10, 20, 30, 40) == 100 && sum_var(0) == 0);

    /* 11, 12: floating point */
    double d = average(1.0, 2.0, 6.0);
    check(11, d > 2.99 && d < 3.01);
    float fl = 2.5f;
    double sq = fl * fl;
    long back = (long)(sq * 4.0);
    check(12, back == 25 && (long)-1.5 == -1 && 10 / 4.0 == 2.5);

    /* 13: goto and labels */
    long n = 0;
again:
    n++;
    if (n < 5) goto again;
    check(13, n == 5);

    /* 14: static locals, short arithmetic, casts */
    counted(); counted();
    short sh = -7;
    unsigned short ush = 65535;
    check(14, counted() == 3 && sh * 2 == -14 && ush + 1 == 65536 && (unsigned char)300 == 44);

    /* 15: switch with fallthrough, do, continue, break */
    long acc = 0, i;
    for (i = 0; i < 6; i++) {
        if (i == 1) continue;
        switch (i) {
        case 0: acc += 1;
        case 2: acc += 10; break;
        case 3: acc += 100; break;
        default: acc += 1000;
        }
        if (i == 4) break;
    }
    check(15, acc == 1121);

    /* 16: words from the text beside this one */
    check(16, from_words(3) == 30);

    /* 17, 18: inline assembly with operands, the way the kernel writes
     * it: the system call by hand, and the time stamp counter */
    {
        long w[3];
        char *b = (char *)w;
        const char *msg = "asm said this           ";
        for (i = 0; i < 24; i++) b[i] = msg[i];
        long nr = 2, a0 = console, a1 = 0x54584554, a2 = w[0];
        register long r10 __asm__("r10") = w[1];
        register long r8 __asm__("r8") = w[2];
        __asm__ volatile ("syscall"
                          : "+a"(nr), "+D"(a0), "+S"(a1), "+d"(a2), "+r"(r10), "+r"(r8)
                          :
                          : "rcx", "r11", "memory");
        check(17, nr == 0);

        unsigned int lo1, hi1, lo2, hi2;
        __asm__ volatile ("rdtsc" : "=a"(lo1), "=d"(hi1));
        for (i = 0; i < 1000; i++) acc += i;
        __asm__ volatile ("rdtsc" : "=a"(lo2), "=d"(hi2));
        unsigned long t1 = ((unsigned long)hi1 << 32) | lo1;
        unsigned long t2 = ((unsigned long)hi2 << 32) | lo2;
        check(18, t2 > t1);
    }

    /* 19: a struct returned by value, kept whole and picked by member */
    pair_t pr = make_pair(3, 4);
    check(19, pr.x + pr.y == 7 && make_pair(5, 6).y == 6 && make_pair(1, 2).tag == 9);

    /* 20: compound literals, array and struct */
    long *cl = (long[3]){ 4, 5, 6 };
    pair_t cp = (pair_t){ .y = 8, .x = 7 };
    check(20, cl[2] == 6 && cp.x == 7 && cp.y == 8 && cp.tag == 0);

    /* 21: eight arguments to a variadic function */
    check(21, sum_var(8, 1, 2, 3, 4, 5, 6, 7, 8) == 36);

    /* 22: a struct handed over by value; what the callee changes is its copy */
    triple_t tr = { 1, 2, 3 };
    check(22, sum_triple(tr, 10) == 16 && tr.a == 1 && bump(tr).c == 4 && tr.c == 3);

    /* 23: by value among many arguments, past the sixth, and a one-byte one */
    check(23, many(1, 1, 1, 1, 1, tr, 1) == 129 && tiny((tiny_t){ 7 }) == 7);

    if (bad_count == 0) say("all checks ok");
    else say("some checks bad");
    return bad_count;
}
