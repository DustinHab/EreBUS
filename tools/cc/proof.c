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
    syscall(2, console_handle, 0x54584554, w[0], w[1], w[2]);
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

    if (bad_count == 0) say("all checks ok");
    else say("some checks bad");
    return bad_count;
}
