/*
 * cc.c -- C compiler: text of C -> text of assembly (asm.c dialect).
 * - one parse pass, one code walk per function; values through rax/xmm0, variables in memory
 * - C11 subset: integer and floating types, pointers, arrays, structs/unions, bit fields, typedefs, enums,
 *   functions (16 params, variadic), full operator set, control flow, initializers, preprocessor with
 *   macros/#if/#include, inline asm in GNU form; integer arithmetic in 64 bits
 * - structs by value: caller keeps a copy and passes its address (own convention)
 * - not supported: 128-bit types, va_arg of a struct
 * - peephole optimizer switches: OPT_* defines, CC_NOPEEP, CC_PEEP_ONLY
 */
#include <eb/cc.h>
#include <eb/lang.h>
#include <eb/string.h>
#include <eb/fmt.h>

#define NAME_MAX    40
#define STR_POOL    524288
#define NSTR        4096
#define NTYPES      4096
#define NMEMBERS    2048
#define STRUCT_MEMBERS_MAX 256        /* members of one struct or union */
#define NSYMS       4096
#define NNODES      16384
#define NMACROS     256
#define MACRO_TEXT  400
#define MPARAMS     8
#define NTYPEDEFS   256
#define NTAGS       128
#define NSRC        12
#define NCOND       32
#define NPARAMS     16                /* a function's parameters at most */
#define REGARGS     6                 /* of which this many ride in registers */
#define EXP_POOL    32768
#define NGLOBALS    384
#define INIT_MAX    65536
#define NFIX        512
#define NDYN        256
#define NGOTO       64

/* ------------------------------------------------------------------ */
/* Types                                                               */
/* ------------------------------------------------------------------ */

enum { T_VOID, T_CHAR, T_SHORT, T_INT, T_LONG, T_FLOAT, T_DOUBLE,
       T_PTR, T_ARR, T_STRUCT, T_UNION, T_FUNC };

typedef struct type type;
struct type {
    u8    kind;
    bool  uns;
    u32   size, align;
    type *base;                       /* ptr, arr: the element; func: what it returns */
    u64   len;                        /* arr */
    u32   mfirst, mcount;             /* struct, union: its members */
    u32   nparams;
    type *params[NPARAMS];
    bool  variadic;
    bool  defined;                    /* struct: the body has been seen */
    bool  packed;
    bool  boolean;                    /* _Bool: a value stored into it becomes 0 or 1 */
    type *ptr;                        /* the pointer to this, once made */
};

typedef struct {
    char  name[NAME_MAX];
    type *ty;
    u32   offset;
    u8    bit_off, bit_width;         /* a bit field, when bit_width is not zero */
} member;

/* ------------------------------------------------------------------ */
/* Symbols                                                             */
/* ------------------------------------------------------------------ */

enum { S_LOCAL, S_GLOBAL, S_FUNC, S_ENUM };

typedef struct {
    char  name[NAME_MAX];
    char  label[NAME_MAX];            /* global: the name it goes by in the assembly */
    type *ty;
    u8    kind;
    i64   val;                        /* local: its offset below rbp; enum: its value */
    bool  defined;                    /* func: a body was seen */
    bool  hidden;                     /* its scope has closed; the slot stays */
    bool  indirect;                   /* local: the slot holds the value's address -- a struct parameter */
    char  reg[8];                     /* register variable: the register asm binds it to */
} sym;

/* ------------------------------------------------------------------ */
/* The tree                                                            */
/* ------------------------------------------------------------------ */

enum {
    ND_NONE, ND_NUM, ND_FNUM, ND_VAR, ND_STR,
    ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_MOD, ND_AND, ND_OR, ND_XOR,
    ND_SHL, ND_SHR, ND_EQ, ND_NE, ND_LT, ND_LE,
    ND_LAND, ND_LOR, ND_NOT, ND_BITNOT, ND_NEG,
    ND_ASSIGN, ND_COND, ND_ADDR, ND_DEREF, ND_MEMBER, ND_CALL,
    ND_CAST, ND_INCDEC, ND_SYSCALL, ND_COMMA, ND_VASTART, ND_VAARG, ND_BYVAL,
    ND_EXPR, ND_RETURN, ND_IF, ND_WHILE, ND_DO, ND_FOR, ND_BLOCK,
    ND_BREAK, ND_CONTINUE, ND_SWITCH, ND_CASE, ND_DEFAULT,
    ND_GOTO, ND_LABEL, ND_ASM, ND_MEMCPY, ND_ZERO
};

typedef struct {
    u8    kind;
    u8    op;                         /* compound assignment: the ND_ of the operation */
    bool  post;                       /* incdec: after, not before */
    u32   lhs, rhs, third;            /* children; 0 is none */
    u32   next;                       /* the next in a list */
    type *ty;
    i64   val;
    u64   fval;                       /* a double, as its bits */
    u32   sym;                        /* var, call by name */
    u32   aux;                        /* member, string, a case's chain, a label */
    u32   line;
} node;

/* ------------------------------------------------------------------ */
/* Tokens                                                              */
/* ------------------------------------------------------------------ */

enum { TK_EOF, TK_NUM, TK_FNUM, TK_IDENT, TK_PUNCT, TK_STR };

typedef struct {
    u8     kind;
    i64    val;
    u64    fval;                      /* a double, as its bits */
    char   text[NAME_MAX];
    u32    str;                       /* TK_STR: which string */
    u32    line;
    char   where[NAME_MAX];           /* which text it came from */
} token;

typedef struct {
    const u8 *text;
    u64  len, pos;
    u32  line;
    char name[NAME_MAX];
    bool macro;                       /* a macro's words; no lines of its own */
    bool bol;
    u32  pool_mark;                   /* where the expansion pool stood before */
} source;

typedef struct {
    char name[NAME_MAX];
    char text[MACRO_TEXT];
    bool func;
    u8   nparams;
    char params[MPARAMS][NAME_MAX];
} macro;

/* One #if and its branches. */
typedef struct { bool parent, active, taken; } cond;

/* ------------------------------------------------------------------ */
/* Initializers                                                        */
/* ------------------------------------------------------------------ */

/* An initializer becomes a byte image, a few places where an address
 * goes instead of bytes, and -- for a local -- a few places computed
 * at run time. */
typedef struct { u32 off; char label[NAME_MAX]; i64 addend; } fixup;
typedef struct { u32 off; type *ty; u32 expr; } dyn_init;

/* ------------------------------------------------------------------ */
/* Inline assembly                                                     */
/* ------------------------------------------------------------------ */

/* asm ("template" : outputs : inputs : clobbers), the way gcc has it
 * and the kernel writes it: AT&T order and spelling in the template,
 * operands by number or [name], constraints naming a register or
 * letting the compiler choose one. It is translated into the
 * assembler's own words, the operands bound to registers before and
 * read back after. */
#define NASM    128
#define ASM_OPS 12

typedef struct {
    char cons[8];                     /* the constraint's letters, = and + taken off */
    char name[NAME_MAX];              /* [name], or empty */
    u32  expr;
    bool out, in;
    bool mem;                         /* "m": the address is bound, not the value */
    bool imm;                         /* "i": a number, written in directly */
    char reg[8];                      /* the register it was given */
    u8   width;                       /* 1, 2, 4 or 8, from its type */
    i64  value;                       /* imm: the number */
} asm_op;

typedef struct { u32 tmpl; asm_op ops[ASM_OPS]; u32 nops; } asm_block;
static asm_block asms[NASM];
static u32 nasm;

/* register T name __asm__("r10"): the name the declarator carried. */
static char last_asm_name[NAME_MAX];

/* ------------------------------------------------------------------ */
/* The compiler's whole state, static: the kernel has no malloc to    */
/* spend on a compile, and one compile runs at a time.                 */
/* ------------------------------------------------------------------ */

typedef struct {
    /* reading */
    source src[NSRC];
    u32    nsrc;
    char   exp_pool[EXP_POOL];
    u32    exp_top;
    macro  macros[NMACROS];
    u32    nmacros;
    cond   conds[NCOND];
    u32    ncond;
    token  cur, nxt;
    cc_find_fn find;
    void  *ctx;

    /* meaning */
    type   types[NTYPES];
    u32    ntypes;
    member members[NMEMBERS];
    u32    nmembers;
    sym    syms[NSYMS];
    u32    nsyms;
    u32    scope[64];
    u32    nscope;
    struct { char name[NAME_MAX]; type *ty; } typedefs[NTYPEDEFS];
    u32    ntypedefs;
    struct { char name[NAME_MAX]; type *ty; } tags[NTAGS];
    u32    ntags;
    type  *t_bool;
    type  *t_void, *t_char, *t_uchar, *t_short, *t_ushort, *t_int, *t_uint,
          *t_long, *t_ulong, *t_float, *t_double, *t_charp;

    node   nodes[NNODES];
    u32    nnodes;

    /* strings */
    u8     spool[STR_POOL];
    u32    spool_len;
    struct { u32 at, n; } strs[NSTR];  /* not "len": an older compiler resolved an inner member's name against the outer struct */
    u32    nstrs;

    /* the function being read */
    i64    frame;
    type  *ret_ty;
    type  *fn_ty;
    u32    fn_no;
    i64    va_area;                   /* offset of the saved registers, variadic */
    i64    ret_slot;                  /* where the caller's result pointer is kept, struct return */
    u32    brk[32], cont[32];
    u32    nbrk, ncont;
    u32    sw_case[32];
    u32    nsw;
    char   gotos[NGOTO][NAME_MAX];    /* labels named in this function */
    u32    ngotos;
    u32    statics;                   /* static locals made so far */
    char   text_section[16];          /* where functions go: code, or user under the pragma */

    /* initializers being built */
    u8     ibuf[INIT_MAX];
    fixup  fixes[NFIX];
    u32    nfix;
    dyn_init dyns[NDYN];
    u32    ndyn;
    u32    ninit_consts;              /* hidden constants for local initializers */

    /* writing */
    char  *out;
    u64    max, len;
    u32    label;
    char  *err;
    u32    errmax;
    bool   bad;
} cstate;

/* The whole state is a few megabytes; it is asked for once, outside
 * the kernel image, and reached as C everywhere below. */
static cstate *Cp;
#define C (*Cp)

/* A name into a name-sized place, cut to fit. */
static void cpy(char *d, const char *s)
{
    u32 i = 0;
    while (s[i] && i < NAME_MAX - 1) { d[i] = s[i]; i++; }
    d[i] = 0;
}

static bool same(const char *a, const char *b) { return strcmp(a, b) == 0; }

void global_add(const char *label, struct type *t, bool has_init, bool is_extern, bool is_static, u32 align);

/* ------------------------------------------------------------------ */
/* Doubles without a vector unit                                       */
/* ------------------------------------------------------------------ */

/* The kernel is built with the vector unit off, so the compiler
 * cannot hold a double as a double; it holds the sixty-four bits of
 * one and makes them with whole-number arithmetic. A literal is a
 * significand and a power of ten; both are folded into a normalised
 * mantissa and a power of two, then rounded into the IEEE shape. */
typedef u64 fbits;

static fbits sf_pack(bool neg, u64 mant, i32 exp2)
{
    u64 sign = neg ? (1ULL << 63) : 0;
    if (mant == 0) return sign;
    while (!(mant >> 63)) { mant <<= 1; exp2--; }
    i32 e = exp2 + 63;
    u64 m = mant;
    u64 rem = m & 0x7FF;
    m >>= 11;
    if (rem > 0x400 || (rem == 0x400 && (m & 1))) { m++; if (m >> 53) { m >>= 1; e++; } }
    i32 be = e + 1023;
    if (be >= 2047) return sign | 0x7FF0000000000000ULL;
    if (be <= 0) return sign;
    return sign | ((u64)be << 52) | (m & 0x000FFFFFFFFFFFFFULL);
}

static void sf_norm(u64 *m, i32 *e)
{
    while (*m && !(*m >> 63)) { *m <<= 1; (*e)--; }
}

/* Both keep to 64-bit arithmetic in halves, so that this compiler can
 * read its own text: there is no 128-bit type here. */
static void sf_mul10(u64 *m, i32 *e)
{
    u64 b = *m & 0xffffffffULL, a = *m >> 32;
    u64 blo = b * 10, mid = a * 10 + (blo >> 32);
    u64 lo = (mid << 32) | (blo & 0xffffffffULL);
    u64 hi = mid >> 32;
    i32 shift = 0;
    while (hi) { lo = (lo >> 1) | (hi << 63); hi >>= 1; shift++; }
    *m = lo;
    *e += shift;
    sf_norm(m, e);
}

static void sf_div10(u64 *m, i32 *e)
{
    /* (m << 64) / 10: the whole part, then the remainder's share in
     * two 32-bit steps */
    u64 hi = *m / 10, r = *m % 10;
    u64 x1 = (r << 32) / 10, r1 = (r << 32) % 10;
    u64 lo = (x1 << 32) | ((r1 << 32) / 10);
    i32 shift = -64;
    while (hi) { lo = (lo >> 1) | (hi << 63); hi >>= 1; shift++; }
    *m = lo;
    *e += shift;
    sf_norm(m, e);
}

static fbits sf_from_decimal(u64 sig, i32 dexp)
{
    if (sig == 0) return 0;
    u64 m = sig;
    i32 e = 0;
    sf_norm(&m, &e);
    while (dexp > 0) { sf_mul10(&m, &e); dexp--; }
    while (dexp < 0) { sf_div10(&m, &e); dexp++; }
    return sf_pack(false, m, e);
}

static fbits sf_from_int(i64 v)
{
    bool neg = v < 0;
    u64 m = neg ? (u64)(-v) : (u64)v;
    return sf_pack(neg, m, 0);
}

static i64 sf_to_int(fbits b)
{
    bool neg = (b >> 63) != 0;
    i32 e = (i32)((b >> 52) & 0x7FF) - 1023;
    if (e < 0 || ((b >> 52) & 0x7FF) == 0) return 0;
    u64 m = (b & 0x000FFFFFFFFFFFFFULL) | (1ULL << 52);
    u64 v = e >= 52 ? (e - 52 >= 12 ? 0 : m << (e - 52)) : m >> (52 - e);
    return neg ? -(i64)v : (i64)v;
}

static u32 sf_to_single(fbits b)
{
    u32 sign = (u32)(b >> 63) << 31;
    i32 be = (i32)((b >> 52) & 0x7FF);
    u64 m = b & 0x000FFFFFFFFFFFFFULL;
    if (be == 0) return sign;
    i32 e = be - 1023 + 127;
    u64 rem = m & 0x1FFFFFFF;
    u32 f = (u32)(m >> 29);
    if (rem > 0x10000000 || (rem == 0x10000000 && (f & 1))) { f++; if (f >> 23) { f >>= 1; e++; } }
    if (e >= 255) return sign | 0x7F800000;
    if (e <= 0) return sign;
    return sign | ((u32)e << 23) | (f & 0x7FFFFF);
}

/* ------------------------------------------------------------------ */
/* Saying what went wrong                                              */
/* ------------------------------------------------------------------ */

static void err_put(u32 *at, const char *s)
{
    while (*s && *at < C.errmax - 1) C.err[(*at)++] = *s++;
}

static void err_dec(u32 *at, u64 v)
{
    char d[24];
    u32 n = 0;
    if (v == 0) d[n++] = '0';
    while (v) { d[n++] = (char)('0' + v % 10); v /= 10; }
    while (n && *at < C.errmax - 1) C.err[(*at)++] = d[--n];
}

static void fail_at(u32 line, const char *where, const char *why, const char *what)
{
    if (C.bad) return;
    C.bad = true;
    if (!why) why = "something is wrong";
    u32 at = 0;
    err_put(&at, "line ");
    err_dec(&at, line);
    if (where && where[0]) { err_put(&at, " in "); err_put(&at, where); }
    err_put(&at, ": ");
    err_put(&at, why);
    if (what && what[0]) {
        err_put(&at, " '");
        err_put(&at, what);
        err_put(&at, "'");
    }
    C.err[at] = 0;
}

static bool in_directive;

static void fail(const char *why, const char *what)
{
    /* Inside a directive the reader is ahead of the parser: the line
     * that matters is the one being read, not the last token's. */
    source *s = in_directive && C.nsrc ? &C.src[C.nsrc - 1] : NULL;
    if (s) fail_at(s->line, s->name, why, what);
    else fail_at(C.cur.line, C.cur.where, why, what);
}

/* ------------------------------------------------------------------ */
/* Reading characters                                                  */
/* ------------------------------------------------------------------ */

static source *top(void) { return C.nsrc ? &C.src[C.nsrc - 1] : NULL; }

static i32 peekc(u32 ahead)
{
    source *s = top();
    if (!s || s->pos + ahead >= s->len) return -1;
    return s->text[s->pos + ahead];
}

static i32 getc_(void)
{
    source *s = top();
    if (!s || s->pos >= s->len) return -1;
    u8 c = s->text[s->pos++];
    if (c == '\n' && !s->macro) { s->line++; s->bol = true; }
    else if (c != ' ' && c != '\t' && c != '\r') s->bol = false;
    return c;
}

static bool push_source(const u8 *text, u64 len, const char *name, bool is_macro)
{
    if (C.nsrc >= NSRC) { fail("includes and macros nest too deep", NULL); return false; }
    source *s = &C.src[C.nsrc++];
    s->text = text;
    s->len = len;
    s->pos = 0;
    s->line = 1;
    s->macro = is_macro;
    s->bol = true;
    s->pool_mark = C.exp_top;
    cpy(s->name, name);
    return true;
}

static void pop_source(void)
{
    if (!C.nsrc) return;
    C.exp_top = C.src[C.nsrc - 1].pool_mark;
    C.nsrc--;
}

/* Room in the expansion pool for a macro's words; freed when the
 * words have been read, in stack order. */
static char *exp_alloc(u32 n)
{
    if (C.exp_top + n > EXP_POOL) { fail("the macro expansions are too large together", NULL); return NULL; }
    char *p = C.exp_pool + C.exp_top;
    C.exp_top += n;
    return p;
}

/* ------------------------------------------------------------------ */
/* Directives                                                          */
/* ------------------------------------------------------------------ */

static bool skipping(void)
{
    return C.ncond && !C.conds[C.ncond - 1].active;
}

static macro *macro_find(const char *nm)
{
    for (u32 i = 0; i < C.nmacros; i++)
        if (same(C.macros[i].name, nm)) return &C.macros[i];
    return NULL;
}

static macro *macro_define(const char *nm)
{
    macro *m = macro_find(nm);
    if (!m) {
        if (C.nmacros >= NMACROS) { fail("too many defines", NULL); return NULL; }
        m = &C.macros[C.nmacros++];
    }
    memset(m, 0, sizeof(*m));
    cpy(m->name, nm);
    return m;
}

static void define_text(const char *nm, const char *text)
{
    macro *m = macro_define(nm);
    if (!m) return;
    u32 k = 0;
    while (text[k] && k < MACRO_TEXT - 1) { m->text[k] = text[k]; k++; }
    m->text[k] = 0;
}

static void define_func(const char *nm, const char *params, const char *text)
{
    macro *m = macro_define(nm);
    if (!m) return;
    m->func = true;
    u32 i = 0;
    while (params[i] && m->nparams < MPARAMS) {
        u32 k = 0;
        while (params[i] && params[i] != ',' && k < NAME_MAX - 1) m->params[m->nparams][k++] = params[i++];
        m->params[m->nparams][k] = 0;
        m->nparams++;
        if (params[i] == ',') i++;
    }
    u32 k = 0;
    while (text[k] && k < MACRO_TEXT - 1) { m->text[k] = text[k]; k++; }
    m->text[k] = 0;
}

/* The rest of the current line, trimmed. A backslash at the end
 * joins the next line on. */
static u32 read_line(char *buf, u32 max)
{
    u32 n = 0;
    for (;;) {
        i32 c = peekc(0);
        if (c < 0) break;
        if (c == '\n') {
            if (n && buf[n - 1] == '\\') { n--; getc_(); if (n < max - 1) buf[n++] = ' '; continue; }
            break;
        }
        getc_();
        if (n < max - 1) buf[n++] = (char)c;
    }
    while (n && (buf[n - 1] == ' ' || buf[n - 1] == '\t' || buf[n - 1] == '\r')) n--;
    buf[n] = 0;
    return n;
}

static bool is_ident_start(i32 c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static bool is_ident_char(i32 c)  { return is_ident_start(c) || (c >= '0' && c <= '9'); }

/* --- #if arithmetic ---------------------------------------------- */

typedef struct { u8 kind; i64 val; char text[NAME_MAX]; } ptok;   /* 0 num, 1 ident, 2 punct */
static ptok ptoks[96];
static u32 nptok, ptat;

static void ptokenize(const char *s, u32 depth)
{
    u32 i = 0;
    while (s[i] && nptok < 96) {
        char c = s[i];
        if (c == ' ' || c == '\t') { i++; continue; }
        if (is_ident_start(c)) {
            char nm[NAME_MAX];
            u32 k = 0;
            while (is_ident_char(s[i])) { if (k < NAME_MAX - 1) nm[k++] = s[i]; i++; }
            nm[k] = 0;
            if (same(nm, "defined")) {
                u32 j = i;
                while (s[j] == ' ') j++;
                bool paren = s[j] == '(';
                if (paren) { j++; while (s[j] == ' ') j++; }
                char dn[NAME_MAX];
                k = 0;
                while (is_ident_char(s[j])) { if (k < NAME_MAX - 1) dn[k++] = s[j]; j++; }
                dn[k] = 0;
                if (paren) { while (s[j] == ' ') j++; if (s[j] == ')') j++; }
                i = j;
                ptoks[nptok].kind = 0;
                ptoks[nptok].val = macro_find(dn) != NULL;
                nptok++;
                continue;
            }
            (void)depth;
            ptoks[nptok].kind = 1;
            cpy(ptoks[nptok].text, nm);
            nptok++;
            continue;
        }
        if (c >= '0' && c <= '9') {
            i64 v = 0;
            if (c == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
                i += 2;
                for (;;) {
                    char h = s[i];
                    i32 d;
                    if (h >= '0' && h <= '9') d = h - '0';
                    else if (h >= 'a' && h <= 'f') d = h - 'a' + 10;
                    else if (h >= 'A' && h <= 'F') d = h - 'A' + 10;
                    else break;
                    v = v * 16 + d;
                    i++;
                }
            } else {
                while (s[i] >= '0' && s[i] <= '9') v = v * 10 + (s[i++] - '0');
            }
            while (s[i] == 'u' || s[i] == 'U' || s[i] == 'l' || s[i] == 'L') i++;
            ptoks[nptok].kind = 0;
            ptoks[nptok].val = v;
            nptok++;
            continue;
        }
        if (c == '\'') {
            i64 v = (u8)s[i + 1];
            if (s[i + 1] == '\\') { v = s[i + 2] == 'n' ? 10 : s[i + 2] == '0' ? 0 : (u8)s[i + 2]; i++; }
            i += 3;
            ptoks[nptok].kind = 0;
            ptoks[nptok].val = v;
            nptok++;
            continue;
        }
        static const char *const ops[] = { "<<", ">>", "<=", ">=", "==", "!=", "&&", "||", NULL };
        bool two = false;
        for (u32 k = 0; ops[k]; k++)
            if (s[i] == ops[k][0] && s[i + 1] == ops[k][1]) {
                ptoks[nptok].kind = 2;
                ptoks[nptok].text[0] = s[i]; ptoks[nptok].text[1] = s[i + 1]; ptoks[nptok].text[2] = 0;
                nptok++;
                i += 2;
                two = true;
                break;
            }
        if (two) continue;
        ptoks[nptok].kind = 2;
        ptoks[nptok].text[0] = c; ptoks[nptok].text[1] = 0;
        nptok++;
        i++;
    }
}

static bool pis(const char *p) { return ptat < nptok && ptoks[ptat].kind == 2 && same(ptoks[ptat].text, p); }

static i64 pexpr(void);

static i64 pprimary(void)
{
    if (ptat >= nptok) return 0;
    if (pis("(")) { ptat++; i64 v = pexpr(); if (pis(")")) ptat++; return v; }
    if (pis("!")) { ptat++; return !pprimary(); }
    if (pis("-")) { ptat++; return -pprimary(); }
    if (pis("~")) { ptat++; return ~pprimary(); }
    if (pis("+")) { ptat++; return pprimary(); }
    ptok *t = &ptoks[ptat++];
    if (t->kind == 0) return t->val;
    return 0;                                  /* an unknown name is zero */
}

static i64 pbin(u32 level)
{
    static const char *const lv[][4] = {
        { "||", 0, 0, 0 }, { "&&", 0, 0, 0 }, { "|", 0, 0, 0 }, { "^", 0, 0, 0 }, { "&", 0, 0, 0 },
        { "==", "!=", 0, 0 }, { "<", "<=", ">", ">=" }, { "<<", ">>", 0, 0 },
        { "+", "-", 0, 0 }, { "*", "/", "%", 0 } };
    if (level >= 10) return pprimary();
    i64 l = pbin(level + 1);
    for (;;) {
        const char *op = NULL;
        for (u32 i = 0; i < 4 && lv[level][i]; i++) if (pis(lv[level][i])) { op = lv[level][i]; break; }
        if (!op) return l;
        ptat++;
        i64 r = pbin(level + 1);
        if (same(op, "||")) l = l || r; else if (same(op, "&&")) l = l && r;
        else if (same(op, "|")) l = l | r; else if (same(op, "^")) l = l ^ r;
        else if (same(op, "&")) l = l & r; else if (same(op, "==")) l = l == r;
        else if (same(op, "!=")) l = l != r; else if (same(op, "<")) l = l < r;
        else if (same(op, "<=")) l = l <= r; else if (same(op, ">")) l = l > r;
        else if (same(op, ">=")) l = l >= r; else if (same(op, "<<")) l = l << r;
        else if (same(op, ">>")) l = l >> r; else if (same(op, "+")) l = l + r;
        else if (same(op, "-")) l = l - r; else if (same(op, "*")) l = l * r;
        else if (same(op, "/")) l = r ? l / r : 0; else if (same(op, "%")) l = r ? l % r : 0;
    }
}

static i64 pexpr(void)
{
    i64 c = pbin(0);
    if (pis("?")) {
        ptat++;
        i64 a = pexpr();
        if (pis(":")) ptat++;
        i64 b = pexpr();
        return c ? a : b;
    }
    return c;
}

/* Macros in a line of text, expanded in place -- object-like ones by
 * their words, function-like ones by their words with the arguments
 * put in -- so that an #if can count with them. "defined" keeps its
 * operand unexpanded and becomes 1 or 0 here. */
static u32 pp_put(char *out, u32 o, u32 max, const char *s)
{
    while (*s && o < max - 1) out[o++] = *s++;
    return o;
}

/* One set of scratch buffers per depth of expansion, static: a
 * kernel thread's stack is small, and eight kilobytes a level would
 * walk right off it. */
static char pp_body[8][4096];
static char pp_raw[8][4096];

static u32 pp_expand(const char *in, char *out, u32 max, u32 depth)
{
    if (depth >= 8) { u32 o = pp_put(out, 0, max, in); out[o] = 0; return o; }
    char *body = pp_body[depth];
    char *raw = pp_raw[depth];
    u32 i = 0, o = 0;
    while (in[i] && o < max - 1) {
        char c = in[i];
        if (!is_ident_start(c)) { out[o++] = c; i++; continue; }
        char nm[NAME_MAX];
        u32 k = 0;
        while (is_ident_char(in[i])) { if (k < NAME_MAX - 1) nm[k++] = in[i]; i++; }
        nm[k] = 0;

        if (same(nm, "defined")) {
            u32 j = i;
            while (in[j] == ' ') j++;
            bool paren = in[j] == '(';
            if (paren) { j++; while (in[j] == ' ') j++; }
            char dn[NAME_MAX];
            k = 0;
            while (is_ident_char(in[j])) { if (k < NAME_MAX - 1) dn[k++] = in[j]; j++; }
            dn[k] = 0;
            if (paren) { while (in[j] == ' ') j++; if (in[j] == ')') j++; }
            i = j;
            o = pp_put(out, o, max, macro_find(dn) ? " 1 " : " 0 ");
            continue;
        }

        macro *m = macro_find(nm);
        if (!m) { o = pp_put(out, o, max, nm); continue; }

        if (!m->func) {
            pp_expand(m->text, body, 4096, depth + 1);
            o = pp_put(out, o, max, " ");
            o = pp_put(out, o, max, body);
            o = pp_put(out, o, max, " ");
            continue;
        }

        /* function-like: only with its parenthesis */
        u32 j = i;
        while (in[j] == ' ') j++;
        if (in[j] != '(') { o = pp_put(out, o, max, nm); continue; }
        j++;
        static char args[MPARAMS][MACRO_TEXT];
        u32 n = 0, ak = 0;
        i32 dep = 0;
        args[0][0] = 0;
        while (in[j]) {
            char a = in[j];
            if (a == '(') dep++;
            if (a == ')') { if (dep == 0) break; dep--; }
            if (a == ',' && dep == 0) {
                args[n][ak] = 0;
                if (n + 1 < MPARAMS) { n++; ak = 0; args[n][0] = 0; }
                j++;
                continue;
            }
            if (ak < MACRO_TEXT - 1) args[n][ak++] = a;
            j++;
        }
        args[n][ak] = 0;
        if (in[j] == ')') j++;
        i = j;
        u32 got = (n == 0 && args[0][0] == 0) ? 0 : n + 1;
        for (u32 a = 0; a < got; a++) {
            u32 s = 0;
            while (args[a][s] == ' ') s++;
            u32 e = 0;
            while (args[a][s + e]) { args[a][e] = args[a][s + e]; e++; }
            args[a][e] = 0;
            while (e && args[a][e - 1] == ' ') args[a][--e] = 0;
        }

        u32 r = 0;
        const char *t = m->text;
        u32 ti = 0;
        while (t[ti] && r < 4096 - 2) {
            if (t[ti] == '#' && t[ti + 1] == '#') { ti += 2; while (r && raw[r - 1] == ' ') r--; while (t[ti] == ' ') ti++; continue; }
            bool str = false;
            if (t[ti] == '#') { str = true; ti++; }
            if (is_ident_start(t[ti])) {
                char pn[NAME_MAX];
                k = 0;
                while (is_ident_char(t[ti])) { if (k < NAME_MAX - 1) pn[k++] = t[ti]; ti++; }
                pn[k] = 0;
                i32 which = -1;
                for (u32 p = 0; p < m->nparams; p++) if (same(m->params[p], pn)) { which = (i32)p; break; }
                if (which >= 0 && (u32)which < got) {
                    if (str) raw[r++] = '"';
                    r = pp_put(raw, r, 4096, args[which]);
                    if (str) raw[r++] = '"';
                } else {
                    r = pp_put(raw, r, 4096, pn);
                }
                continue;
            }
            raw[r++] = t[ti++];
        }
        raw[r] = 0;
        pp_expand(raw, body, 4096, depth + 1);
        o = pp_put(out, o, max, " ");
        o = pp_put(out, o, max, body);
        o = pp_put(out, o, max, " ");
    }
    out[o] = 0;
    return o;
}

static i64 eval_line(const char *s)
{
    static char expanded[8192];
    pp_expand(s, expanded, sizeof(expanded), 0);
    nptok = 0;
    ptat = 0;
    ptokenize(expanded, 0);
    return pexpr();
}

/* --- the standard headers this machine carries inside ------------- */

static void builtin_header(const char *name)
{
    if (same(name, "stdarg.h")) {
        define_text("va_list", "__builtin_va_list");
        define_func("va_start", "ap,last", "__builtin_va_start(ap, last)");
        define_func("va_arg", "ap,t", "__builtin_va_arg(ap, t)");
        define_func("va_end", "ap", "__builtin_va_end(ap)");
        define_func("va_copy", "d,s", "((d) = (s))");
        return;
    }
    if (same(name, "stdbool.h")) {
        define_text("true", "1");
        define_text("false", "0");
        return;
    }
    if (same(name, "stddef.h")) {
        define_text("NULL", "((void *)0)");
        define_text("size_t", "unsigned long");
        define_text("offsetof", "__builtin_offsetof");
        return;
    }
    if (same(name, "stdint.h")) {
        define_text("uint8_t", "unsigned char");
        define_text("uint16_t", "unsigned short");
        define_text("uint32_t", "unsigned int");
        define_text("uint64_t", "unsigned long");
        define_text("int8_t", "signed char");
        define_text("int16_t", "short");
        define_text("int32_t", "int");
        define_text("int64_t", "long");
        define_text("uintptr_t", "unsigned long");
        define_text("intptr_t", "long");
        define_text("size_t", "unsigned long");
        return;
    }
    fail("no built-in header:", name);
}

static void directive(void)
{
    char line[MACRO_TEXT + NAME_MAX + 64];
    read_line(line, sizeof(line));
    u32 i = 0;
    while (line[i] == ' ' || line[i] == '\t') i++;
    char word[16];
    u32 w = 0;
    while (line[i] && line[i] != ' ' && line[i] != '\t' && line[i] != '(' && w < 15) word[w++] = line[i++];
    word[w] = 0;
    while (line[i] == ' ' || line[i] == '\t') i++;

    /* Conditions are read even while skipping: they nest. */
    if (same(word, "if") || same(word, "ifdef") || same(word, "ifndef")) {
        if (C.ncond >= NCOND) { fail("conditions nest too deep", NULL); return; }
        bool parent = !skipping();
        bool v = false;
        if (parent) {
            if (same(word, "if")) v = eval_line(line + i) != 0;
            else {
                char name[NAME_MAX];
                u32 n = 0;
                while (line[i] && line[i] != ' ' && line[i] != '\t' && n < NAME_MAX - 1) name[n++] = line[i++];
                name[n] = 0;
                bool have = macro_find(name) != NULL;
                v = word[2] == 'n' ? !have : have;
            }
        }
        C.conds[C.ncond].parent = parent;
        C.conds[C.ncond].active = v;
        C.conds[C.ncond].taken = v;
        C.ncond++;
        return;
    }
    if (same(word, "elif")) {
        if (!C.ncond) { fail("#elif without #if", NULL); return; }
        cond *c = &C.conds[C.ncond - 1];
        if (c->parent && !c->taken && eval_line(line + i) != 0) { c->active = true; c->taken = true; }
        else c->active = false;
        return;
    }
    if (same(word, "else")) {
        if (!C.ncond) { fail("#else without #if", NULL); return; }
        cond *c = &C.conds[C.ncond - 1];
        c->active = c->parent && !c->taken;
        c->taken = true;
        return;
    }
    if (same(word, "endif")) {
        if (!C.ncond) { fail("#endif without #if", NULL); return; }
        C.ncond--;
        return;
    }
    if (skipping()) return;

    if (same(word, "define")) {
        char name[NAME_MAX];
        u32 n = 0;
        while (line[i] && line[i] != ' ' && line[i] != '\t' && line[i] != '(' && n < NAME_MAX - 1) name[n++] = line[i++];
        name[n] = 0;
        if (!name[0]) { fail("#define what?", NULL); return; }
        if (line[i] == '(') {
            /* function-like: the names between the parentheses */
            i++;
            char params[MPARAMS * NAME_MAX];
            u32 k = 0;
            while (line[i] && line[i] != ')') {
                if (line[i] != ' ' && line[i] != '\t' && k < sizeof(params) - 1) params[k++] = line[i];
                i++;
            }
            params[k] = 0;
            if (line[i] == ')') i++;
            while (line[i] == ' ' || line[i] == '\t') i++;
            define_func(name, params, line + i);
        } else {
            while (line[i] == ' ' || line[i] == '\t') i++;
            define_text(name, line + i);
        }
        return;
    }
    if (same(word, "undef")) {
        char name[NAME_MAX];
        u32 n = 0;
        while (line[i] && line[i] != ' ' && n < NAME_MAX - 1) name[n++] = line[i++];
        name[n] = 0;
        macro *m = macro_find(name);
        if (m) *m = C.macros[--C.nmacros];
        return;
    }
    if (same(word, "include")) {
        char name[NAME_MAX];
        u32 n = 0;
        if (line[i] == '<') {
            i++;
            while (line[i] && line[i] != '>' && n < NAME_MAX - 1) name[n++] = line[i++];
            name[n] = 0;
            /* The few standard headers live inside; anything else in
             * angle brackets is looked for beside the source like a
             * quoted name, which is how a kernel file finds its own. */
            const u8 *text;
            u64 len;
            if (C.find && C.find(C.ctx, name, &text, &len)) { push_source(text, len, name, false); return; }
            builtin_header(name);
            return;
        }
        if (line[i] != '"') { fail("#include wants a name in quotes or angle brackets", NULL); return; }
        i++;
        while (line[i] && line[i] != '"' && n < NAME_MAX - 1) name[n++] = line[i++];
        name[n] = 0;
        const u8 *text;
        u64 len;
        if (!C.find || !C.find(C.ctx, name, &text, &len)) {
            fail("no text of that name beside this one:", name);
            return;
        }
        push_source(text, len, name, false);
        return;
    }
    if (same(word, "error")) { fail("#error:", line + i); return; }
    if (same(word, "pragma")) {
        /* #pragma clang section text = "...": functions go to the
         * named section from here on -- the kernel's ring-3 programs
         * live in .user. An empty name brings them back to the code. */
        const char *p = line + i;
        const char *tx = NULL;
        for (u32 k = 0; p[k]; k++)
            if (p[k] == 't' && p[k + 1] == 'e' && p[k + 2] == 'x' && p[k + 3] == 't' && (k == 0 || p[k - 1] == ' ')) { tx = p + k + 4; break; }
        if (tx) {
            while (*tx == ' ' || *tx == '=') tx++;
            if (*tx == '"') {
                tx++;
                bool user = false;
                for (u32 k = 0; tx[k] && tx[k] != '"'; k++)
                    if (tx[k] == 'u' && tx[k + 1] == 's' && tx[k + 2] == 'e' && tx[k + 3] == 'r') user = true;
                cpy(C.text_section, user ? "user" : "code");
            }
        }
        return;
    }
    if (same(word, "line") || same(word, "warning")) return;
    fail("unknown directive", word);
}

/* ------------------------------------------------------------------ */
/* Tokens                                                              */
/* ------------------------------------------------------------------ */

static const char *const puncts[] = {
    "<<=", ">>=", "...",
    "==", "!=", "<=", ">=", "&&", "||", "++", "--", "+=", "-=", "*=",
    "/=", "%=", "&=", "|=", "^=", "<<", ">>", "->",
    NULL };

static i32 escape(void)
{
    i32 c = getc_();
    switch (c) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case 'a': return 7;
    case 'b': return 8;
    case 'f': return 12;
    case 'v': return 11;
    case 'e': return 27;
    case 'x': {
        i32 v = 0;
        for (u32 k = 0; k < 2; k++) {
            i32 h = peekc(0);
            i32 d;
            if (h >= '0' && h <= '9') d = h - '0';
            else if (h >= 'a' && h <= 'f') d = h - 'a' + 10;
            else if (h >= 'A' && h <= 'F') d = h - 'A' + 10;
            else break;
            getc_();
            v = v * 16 + d;
        }
        return v;
    }
    default:
        if (c >= '0' && c <= '7') {
            i32 v = c - '0';
            for (u32 k = 0; k < 2 && peekc(0) >= '0' && peekc(0) <= '7'; k++) v = v * 8 + (getc_() - '0');
            return v;
        }
        return c;
    }
}

static void lex_raw(token *t);

/* Reads a macro call's arguments as raw text, balanced in parens,
 * split at the commas outside them. */
static u32 macro_args(char args[MPARAMS][MACRO_TEXT])
{
    u32 n = 0, k = 0;
    i32 depth = 0;
    args[0][0] = 0;
    for (;;) {
        i32 c = getc_();
        if (c < 0) { fail("a macro's arguments never close", NULL); return 0; }
        if (c == '\'' || c == '"') {
            /* a letter or a string: its commas and parentheses are its own */
            i32 q = c;
            if (k < MACRO_TEXT - 1) args[n][k++] = (char)c;
            for (;;) {
                c = getc_();
                if (c < 0) { fail("a macro's arguments never close", NULL); return 0; }
                if (k < MACRO_TEXT - 1) args[n][k++] = (char)c;
                if (c == '\\') { c = getc_(); if (c >= 0 && k < MACRO_TEXT - 1) args[n][k++] = (char)c; continue; }
                if (c == q) break;
            }
            continue;
        }
        if (c == '(' ) depth++;
        if (c == ')') { if (depth == 0) break; depth--; }
        if (c == ',' && depth == 0) {
            args[n][k] = 0;
            if (n + 1 >= MPARAMS) { fail("too many macro arguments", NULL); return 0; }
            n++; k = 0; args[n][0] = 0;
            continue;
        }
        if (c == '\n') c = ' ';
        if (k < MACRO_TEXT - 1) args[n][k++] = (char)c;
    }
    args[n][k] = 0;
    /* trim */
    for (u32 a = 0; a <= n; a++) {
        u32 s = 0;
        while (args[a][s] == ' ') s++;
        u32 e = 0;
        while (args[a][s + e]) { args[a][e] = args[a][s + e]; e++; }
        args[a][e] = 0;
        while (e && args[a][e - 1] == ' ') args[a][--e] = 0;
    }
    return (n == 0 && args[0][0] == 0) ? 0 : n + 1;
}

/* Builds a function-like macro's expansion: parameters replaced by
 * their arguments, #x quoted, ## joined. */
static bool expand_func(macro *m)
{
    static char args[MPARAMS][MACRO_TEXT];
    u32 got = macro_args(args);
    if (C.bad) return false;
    if (got != m->nparams && !(m->nparams == 0 && got == 0)) {
        fail("wrong number of arguments to the macro", m->name);
        return false;
    }
    static char scratch[4096];
    char *buf = scratch;
    u32 o = 0;
    const char *t = m->text;
    u32 i = 0;
    while (t[i] && o < 4090) {
        if (t[i] == '#' && t[i + 1] == '#') { i += 2; while (o && buf[o - 1] == ' ') o--; while (t[i] == ' ') i++; continue; }
        bool stringize = false;
        if (t[i] == '#') { stringize = true; i++; }
        if (is_ident_start(t[i])) {
            char nm[NAME_MAX];
            u32 k = 0;
            while (is_ident_char(t[i])) { if (k < NAME_MAX - 1) nm[k++] = t[i]; i++; }
            nm[k] = 0;
            i32 which = -1;
            for (u32 p = 0; p < m->nparams; p++) if (same(m->params[p], nm)) { which = (i32)p; break; }
            if (which >= 0) {
                if (stringize) buf[o++] = '"';
                for (u32 q = 0; args[which][q] && o < 4090; q++) buf[o++] = args[which][q];
                if (stringize) buf[o++] = '"';
            } else {
                for (u32 q = 0; nm[q] && o < 4090; q++) buf[o++] = nm[q];
            }
            continue;
        }
        buf[o++] = t[i++];
    }
    buf[o] = 0;
    /* Only as much of the pool as the words need: nested expansions
     * add up, and a kernel file has many. */
    char *kept = exp_alloc(o + 1);
    if (!kept) return false;
    memcpy(kept, buf, o + 1);
    return push_source((const u8 *)kept, o, m->name, true);
}

static void lex_ident_or_macro(token *t, i32 first)
{
    u32 n = 0;
    t->text[n++] = (char)first;
    for (;;) {
        i32 c = peekc(0);
        if (!is_ident_char(c)) break;
        getc_();
        if (n < NAME_MAX - 1) t->text[n++] = (char)c;
    }
    t->text[n] = 0;
    t->kind = TK_IDENT;

    /* A macro's name becomes its words, unless we are already inside
     * those words -- a macro that names itself stays a name. */
    macro *m = macro_find(t->text);
    if (!m) return;
    for (u32 i = 0; i < C.nsrc; i++)
        if (C.src[i].macro && same(C.src[i].name, t->text)) return;
    if (m->func) {
        /* Only with a parenthesis right after; otherwise it is a name. */
        u32 k = 0;
        while (peekc(k) == ' ' || peekc(k) == '\t' || peekc(k) == '\n') k++;
        if (peekc(k) != '(') return;
        for (u32 q = 0; q <= k; q++) getc_();
        if (!expand_func(m)) return;
        lex_raw(t);
        return;
    }
    u32 ml = 0;
    while (m->text[ml]) ml++;
    if (ml == 0) { lex_raw(t); return; }
    if (!push_source((const u8 *)m->text, ml, m->name, true)) return;
    lex_raw(t);
}

static void lex_raw(token *t)
{
    for (;;) {
        source *s = top();
        if (!s) { t->kind = TK_EOF; return; }
        if (s->pos >= s->len) {
            pop_source();
            continue;
        }
        t->line = s->line;
        cpy(t->where, s->name);

        i32 c = peekc(0);
        bool at_bol = s->bol;

        if (c == '\n' || c == ' ' || c == '\t' || c == '\r') { getc_(); continue; }

        if (c == '#' && at_bol && !s->macro) {
            getc_();
            in_directive = true;
            directive();
            in_directive = false;
            if (C.bad) { t->kind = TK_EOF; return; }
            continue;
        }

        if (skipping()) {
            read_line(t->text, NAME_MAX);
            continue;
        }

        if (c == '/' && peekc(1) == '/') {
            while (peekc(0) >= 0 && peekc(0) != '\n') getc_();
            continue;
        }
        if (c == '/' && peekc(1) == '*') {
            getc_(); getc_();
            while (peekc(0) >= 0 && !(peekc(0) == '*' && peekc(1) == '/')) getc_();
            if (peekc(0) < 0) { fail("the comment never closes", NULL); t->kind = TK_EOF; return; }
            getc_(); getc_();
            continue;
        }

        getc_();

        if (is_ident_start(c)) {
            lex_ident_or_macro(t, c);
            return;
        }

        if ((c >= '0' && c <= '9') || (c == '.' && peekc(0) >= '0' && peekc(0) <= '9')) {
            i64 v = 0;
            bool fp = false;
            u64 sig = 0;                       /* the digits, as one number */
            i32 dexp = 0;                      /* and the power of ten they carry */
            u32 digits = 0;
            if (c == '0' && (peekc(0) == 'x' || peekc(0) == 'X')) {
                getc_();
                for (;;) {
                    i32 h = peekc(0);
                    i32 d;
                    if (h >= '0' && h <= '9') d = h - '0';
                    else if (h >= 'a' && h <= 'f') d = h - 'a' + 10;
                    else if (h >= 'A' && h <= 'F') d = h - 'A' + 10;
                    else break;
                    getc_();
                    v = (i64)((u64)v * 16 + (u64)d);   /* the bits, wrapping like the constant does */
                }
            } else {
                if (c != '.') { v = c - '0'; sig = (u64)(c - '0'); digits = 1; }
                while (peekc(0) >= '0' && peekc(0) <= '9') {
                    i32 d = getc_() - '0';
                    v = (i64)((u64)v * 10 + (u64)d);
                    if (digits < 19) { sig = sig * 10 + (u64)d; digits++; } else dexp++;
                }
                if (c == '.' || peekc(0) == '.') {
                    if (c != '.') getc_();
                    fp = true;
                    while (peekc(0) >= '0' && peekc(0) <= '9') {
                        i32 d = getc_() - '0';
                        if (digits < 19) { sig = sig * 10 + (u64)d; digits++; dexp--; }
                    }
                }
                if (peekc(0) == 'e' || peekc(0) == 'E') {
                    fp = true;
                    getc_();
                    bool neg = false;
                    if (peekc(0) == '-') { neg = true; getc_(); } else if (peekc(0) == '+') getc_();
                    i32 e = 0;
                    while (peekc(0) >= '0' && peekc(0) <= '9') e = e * 10 + (getc_() - '0');
                    dexp += neg ? -e : e;
                }
            }
            while (peekc(0) == 'u' || peekc(0) == 'U' || peekc(0) == 'l' || peekc(0) == 'L' ||
                   peekc(0) == 'f' || peekc(0) == 'F') {
                if (peekc(0) == 'f' || peekc(0) == 'F') fp = true;
                getc_();
            }
            if (fp) { t->kind = TK_FNUM; t->fval = sf_from_decimal(sig, dexp); }
            else { t->kind = TK_NUM; t->val = v; }
            return;
        }

        if (c == '\'') {
            i32 v = getc_();
            if (v == '\\') v = escape();
            if (getc_() != '\'') { fail("the letter never closes", NULL); t->kind = TK_EOF; return; }
            t->kind = TK_NUM;
            t->val = v;
            return;
        }

        if (c == '"') {
            if (C.nstrs >= NSTR) { fail("too many strings", NULL); t->kind = TK_EOF; return; }
            u32 at = C.spool_len;
            for (;;) {
                i32 v = getc_();
                if (v < 0 || v == '\n') { fail("the string never closes", NULL); t->kind = TK_EOF; return; }
                if (v == '"') break;
                if (v == '\\') v = escape();
                if (C.spool_len >= STR_POOL - 1) { fail("the strings are too long together", NULL); t->kind = TK_EOF; return; }
                C.spool[C.spool_len++] = (u8)v;
            }
            C.spool[C.spool_len++] = 0;
            C.strs[C.nstrs].at = at;
            C.strs[C.nstrs].n = C.spool_len - at;
            t->kind = TK_STR;
            t->str = C.nstrs++;
            return;
        }

        for (u32 i = 0; puncts[i]; i++) {
            const char *p = puncts[i];
            u32 pl = (u32)strlen(p);
            bool m = p[0] == c;
            for (u32 k = 1; m && k < pl; k++) if (peekc(k - 1) != p[k]) m = false;
            if (m) {
                for (u32 k = 1; k < pl; k++) getc_();
                t->kind = TK_PUNCT;
                cpy(t->text, p);
                return;
            }
        }
        t->kind = TK_PUNCT;
        t->text[0] = (char)c;
        t->text[1] = 0;
        return;
    }
}

static void advance(void)
{
    C.cur = C.nxt;
    lex_raw(&C.nxt);
}

static bool is(const char *p)     { return C.cur.kind == TK_PUNCT && same(C.cur.text, p); }
static bool is_kw(const char *w)  { return C.cur.kind == TK_IDENT && same(C.cur.text, w); }
static bool nxt_is(const char *p) { return C.nxt.kind == TK_PUNCT && same(C.nxt.text, p); }

static bool eat(const char *p)
{
    if (!is(p)) return false;
    advance();
    return true;
}

static void expect(const char *p)
{
    if (!eat(p)) {
        char what[NAME_MAX + 12];
        u32 n = 0;
        const char *pre = "expected ";
        while (pre[n]) { what[n] = pre[n]; n++; }
        for (u32 i = 0; p[i] && n < sizeof(what) - 1; i++) what[n++] = p[i];
        what[n] = 0;
        fail(what, C.cur.text);
    }
}

/* ------------------------------------------------------------------ */
/* Types, made                                                         */
/* ------------------------------------------------------------------ */

static type *new_type(u8 kind, u32 size, u32 align)
{
    if (C.ntypes >= NTYPES) { fail("too many types", NULL); return C.t_long; }
    type *t = &C.types[C.ntypes++];
    memset(t, 0, sizeof(*t));
    t->kind = kind;
    t->size = size;
    t->align = align;
    return t;
}

static type *ptr_to(type *b)
{
    if (b->ptr) return b->ptr;
    type *t = new_type(T_PTR, 8, 8);
    t->base = b;
    b->ptr = t;
    return t;
}

static type *array_of(type *b, u64 n)
{
    type *t = new_type(T_ARR, (u32)(b->size * n), b->align);
    t->base = b;
    t->len = n;
    return t;
}

static bool is_int(const type *t)   { return t->kind >= T_CHAR && t->kind <= T_LONG; }
static bool is_flt(const type *t)   { return t->kind == T_FLOAT || t->kind == T_DOUBLE; }
static bool is_num(const type *t)   { return is_int(t) || is_flt(t); }
static bool is_ptr(const type *t)   { return t->kind == T_PTR || t->kind == T_ARR; }
static bool is_rec(const type *t)   { return t->kind == T_STRUCT || t->kind == T_UNION; }

static type *typedef_find(const char *nm)
{
    for (u32 i = 0; i < C.ntypedefs; i++)
        if (same(C.typedefs[i].name, nm)) return C.typedefs[i].ty;
    return NULL;
}

static void typedef_add(const char *nm, type *t)
{
    for (u32 i = 0; i < C.ntypedefs; i++)
        if (same(C.typedefs[i].name, nm)) { C.typedefs[i].ty = t; return; }
    if (C.ntypedefs >= NTYPEDEFS) { fail("too many typedefs", NULL); return; }
    cpy(C.typedefs[C.ntypedefs].name, nm);
    C.typedefs[C.ntypedefs].ty = t;
    C.ntypedefs++;
}

static type *tag_find(const char *nm)
{
    for (u32 i = 0; i < C.ntags; i++)
        if (same(C.tags[i].name, nm)) return C.tags[i].ty;
    return NULL;
}

/* The wider of two numeric types, the way c promotes. */
static type *common_type(type *a, type *b)
{
    if (is_flt(a) || is_flt(b)) return C.t_double;
    bool lng = a->kind == T_LONG || b->kind == T_LONG;
    bool uns = (a->kind == T_LONG && a->uns) || (b->kind == T_LONG && b->uns) ||
               (!lng && ((a->kind == T_INT && a->uns) || (b->kind == T_INT && b->uns)));
    if (lng) return uns ? C.t_ulong : C.t_long;
    return uns ? C.t_uint : C.t_int;
}

/* A c name as the assembler's label. Names are kept as they are, so
 * that c and assembly meet on the same words -- kmain is kmain; the
 * few that read as a register get an underscore after them. */
static const char *symname(const char *nm)
{
    static const char *const regs[] = {
        "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
        "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
        "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
        "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
        "cr0", "cr2", "cr3", "cr4", "cr8", "es", "cs", "ss", "ds", "fs", "gs", NULL };
    static char bufs[2][NAME_MAX + 2];
    static u32 which;
    bool hit = false;
    for (u32 i = 0; regs[i] && !hit; i++) if (same(nm, regs[i])) hit = true;
    if (!hit && nm[0] == 'r' && nm[1] >= '0' && nm[1] <= '9') hit = true;            /* r8 .. r15d */
    if (!hit && nm[0] == 'x' && nm[1] == 'm' && nm[2] == 'm' && nm[3] >= '0' && nm[3] <= '9') hit = true;
    if (!hit) return nm;
    char *b = bufs[which];
    which ^= 1;
    u32 k = 0;
    while (nm[k] && k < NAME_MAX - 1) { b[k] = nm[k]; k++; }
    b[k++] = '_';
    b[k] = 0;
    return b;
}

/* ------------------------------------------------------------------ */
/* Symbols, scoped                                                     */
/* ------------------------------------------------------------------ */

static sym *sym_find(const char *nm)
{
    for (u32 i = C.nsyms; i > 0; i--)
        if (!C.syms[i - 1].hidden && same(C.syms[i - 1].name, nm)) return &C.syms[i - 1];
    return NULL;
}

static sym *sym_add(const char *nm, type *ty, u8 kind)
{
    if (C.nsyms >= NSYMS) { fail("too many names", NULL); return &C.syms[0]; }
    sym *s = &C.syms[C.nsyms++];
    memset(s, 0, sizeof(*s));
    cpy(s->name, nm);
    cpy(s->label, nm);
    s->ty = ty;
    s->kind = kind;
    return s;
}

static void scope_in(void)  { if (C.nscope < 64) C.scope[C.nscope++] = C.nsyms; }

/* Closing a scope hides its names; it does not give their slots
 * back. The tree refers to names by slot and is typed only after the
 * whole function is read, so a slot reused inside the function would
 * quietly mean something else by then. The function gives all its
 * slots back at once, when its code has been written. */
static void scope_out(void)
{
    if (!C.nscope) return;
    u32 mark = C.scope[--C.nscope];
    for (u32 i = mark; i < C.nsyms; i++) C.syms[i].hidden = true;
}

static i64 local_slot(type *ty)
{
    u32 al = ty->align ? ty->align : 1;
    if (al > 16) al = 16;
    C.frame += ty->size ? ty->size : 1;
    C.frame = (C.frame + al - 1) / al * al;
    return -C.frame;
}

/* ------------------------------------------------------------------ */
/* Nodes                                                               */
/* ------------------------------------------------------------------ */

static u32 mk(u8 kind)
{
    if (C.nnodes >= NNODES) { fail("the function is too large", NULL); return 0; }
    u32 i = C.nnodes++;
    memset(&C.nodes[i], 0, sizeof(node));
    C.nodes[i].kind = kind;
    C.nodes[i].line = C.cur.line;
    return i;
}

#define N(i) (&C.nodes[i])

static u32 mk_num(i64 v)
{
    u32 i = mk(ND_NUM);
    N(i)->val = v;
    N(i)->ty = C.t_long;
    return i;
}

static u32 mk_bin(u8 kind, u32 l, u32 r)
{
    u32 i = mk(kind);
    N(i)->lhs = l;
    N(i)->rhs = r;
    return i;
}

static u32 mk_un(u8 kind, u32 l)
{
    u32 i = mk(kind);
    N(i)->lhs = l;
    return i;
}

static u32 mk_cast(u32 e, type *t)
{
    u32 i = mk_un(ND_CAST, e);
    N(i)->ty = t;
    return i;
}

/* ------------------------------------------------------------------ */
/* Giving every node its type                                          */
/* ------------------------------------------------------------------ */

static void add_type(u32 i);

static u32 scaled(u32 n, u64 by)
{
    if (by == 1) return n;
    u32 m = mk_bin(ND_MUL, n, mk_num((i64)by));
    N(m)->ty = C.t_long;
    return m;
}

static void add_type(u32 i)
{
    if (!i || C.bad) return;
    node *n = N(i);
    /* A statement is typed on the way in, not the way out: the cases
     * of a switch point at one another, and a walk that only marked
     * a node when done with it would never be done with them. */
    if (n->kind >= ND_EXPR) {
        if (n->ty) return;
        n->ty = C.t_void;
    } else if (n->ty && n->kind != ND_CAST) return;

    add_type(n->lhs);
    add_type(n->rhs);
    add_type(n->third);
    /* A list is walked by whoever owns it, in a loop, never by each
     * member recursing into the next: a long block would otherwise
     * be a deep stack, and a kernel thread's stack is small. */
    if (n->kind != ND_STR && n->kind != ND_MEMBER && n->kind != ND_CASE &&
        n->kind != ND_DEFAULT && n->kind != ND_GOTO && n->kind != ND_LABEL &&
        n->kind != ND_MEMCPY)
        for (u32 a = n->aux; a; a = N(a)->next) add_type(a);

    node *l = n->lhs ? N(n->lhs) : NULL;
    node *r = n->rhs ? N(n->rhs) : NULL;

    switch (n->kind) {
    case ND_NUM:  n->ty = C.t_long; break;
    case ND_FNUM: n->ty = C.t_double; break;
    case ND_STR:  n->ty = array_of(C.t_char, C.strs[n->aux].n); break;
    case ND_VAR:  n->ty = C.syms[n->sym].ty; break;

    case ND_ADD:
        if (is_ptr(l->ty) && is_int(r->ty)) {
            n->rhs = scaled(n->rhs, l->ty->base->size ? l->ty->base->size : 1);
            n->ty = l->ty->kind == T_ARR ? ptr_to(l->ty->base) : l->ty;
        } else if (is_int(l->ty) && is_ptr(r->ty)) {
            n->lhs = scaled(n->lhs, r->ty->base->size ? r->ty->base->size : 1);
            n->ty = r->ty->kind == T_ARR ? ptr_to(r->ty->base) : r->ty;
        } else if (is_num(l->ty) && is_num(r->ty)) {
            n->ty = common_type(l->ty, r->ty);
            if (is_flt(n->ty)) {
                if (!is_flt(l->ty)) n->lhs = mk_cast(n->lhs, C.t_double);
                if (!is_flt(r->ty)) n->rhs = mk_cast(n->rhs, C.t_double);
            }
        } else fail_at(n->line, NULL, "those cannot be added", NULL);
        break;

    case ND_SUB:
        if (is_ptr(l->ty) && is_int(r->ty)) {
            n->rhs = scaled(n->rhs, l->ty->base->size ? l->ty->base->size : 1);
            n->ty = l->ty->kind == T_ARR ? ptr_to(l->ty->base) : l->ty;
        } else if (is_ptr(l->ty) && is_ptr(r->ty)) {
            u32 sub = mk_bin(ND_SUB, n->lhs, n->rhs);
            N(sub)->ty = C.t_long;
            u32 keep = n->next;
            u32 line = n->line;
            u32 sz = l->ty->base->size ? l->ty->base->size : 1;
            memset(n, 0, sizeof(*n));
            n->kind = ND_DIV;
            n->lhs = sub;
            n->rhs = mk_num((i64)sz);
            n->ty = C.t_long;
            n->next = keep;
            n->line = line;
        } else if (is_num(l->ty) && is_num(r->ty)) {
            n->ty = common_type(l->ty, r->ty);
            if (is_flt(n->ty)) {
                if (!is_flt(l->ty)) n->lhs = mk_cast(n->lhs, C.t_double);
                if (!is_flt(r->ty)) n->rhs = mk_cast(n->rhs, C.t_double);
            }
        } else fail_at(n->line, NULL, "those cannot be subtracted", NULL);
        break;

    case ND_MUL: case ND_DIV:
        if (!is_num(l->ty) || !is_num(r->ty)) { fail_at(n->line, NULL, "that wants numbers on both sides", NULL); break; }
        n->ty = common_type(l->ty, r->ty);
        if (is_flt(n->ty)) {
            if (!is_flt(l->ty)) n->lhs = mk_cast(n->lhs, C.t_double);
            if (!is_flt(r->ty)) n->rhs = mk_cast(n->rhs, C.t_double);
        }
        break;

    case ND_MOD: case ND_AND: case ND_OR: case ND_XOR:
        if (!is_int(l->ty) || !is_int(r->ty)) { fail_at(n->line, NULL, "that wants whole numbers on both sides", NULL); break; }
        n->ty = common_type(l->ty, r->ty);
        break;

    case ND_SHL: case ND_SHR:
        if (!is_int(l->ty)) { fail_at(n->line, NULL, "only a whole number shifts", NULL); break; }
        n->ty = l->ty->kind == T_LONG ? l->ty : (l->ty->uns ? C.t_uint : C.t_int);
        break;

    case ND_EQ: case ND_NE: case ND_LT: case ND_LE:
        if (is_flt(l->ty) || is_flt(r->ty)) {
            if (!is_flt(l->ty)) n->lhs = mk_cast(n->lhs, C.t_double);
            if (!is_flt(r->ty)) n->rhs = mk_cast(n->rhs, C.t_double);
        }
        n->ty = C.t_long;
        break;

    case ND_LAND: case ND_LOR: case ND_NOT:
        n->ty = C.t_long;
        break;

    case ND_BITNOT:
        n->ty = l->ty;
        break;

    case ND_NEG:
        n->ty = is_flt(l->ty) ? C.t_double : l->ty;
        break;

    case ND_ASSIGN:
        if (l->ty->kind == T_ARR)
            fail_at(n->line, NULL, "an array is not something to assign to:",
                    l->kind == ND_VAR ? C.syms[l->sym].name : "that");
        if (is_flt(l->ty) && !is_flt(r->ty) && !n->op) n->rhs = mk_cast(n->rhs, C.t_double);
        if (!is_flt(l->ty) && is_flt(r->ty) && !n->op && !is_rec(l->ty)) n->rhs = mk_cast(n->rhs, C.t_long);
        n->ty = l->ty;
        break;

    case ND_COND:
        if (is_flt(r->ty) || is_flt(N(n->third)->ty)) {
            if (!is_flt(r->ty)) n->rhs = mk_cast(n->rhs, C.t_double);
            if (!is_flt(N(n->third)->ty)) n->third = mk_cast(n->third, C.t_double);
            n->ty = C.t_double;
        } else n->ty = r->ty->kind == T_VOID ? C.t_void : r->ty;
        break;

    case ND_COMMA:
        n->ty = r->ty;
        break;

    case ND_ADDR:
        n->ty = ptr_to(l->ty->kind == T_ARR ? l->ty->base : l->ty);
        break;

    case ND_DEREF:
        if (!is_ptr(l->ty)) { fail_at(n->line, NULL, "only a pointer can be followed", NULL); break; }
        n->ty = l->ty->base->kind == T_VOID ? C.t_char : l->ty->base;
        break;

    case ND_MEMBER:
        n->ty = C.members[n->aux].ty;
        break;

    case ND_CALL: {
        type *ft = NULL;
        if (n->sym) ft = C.syms[n->sym].ty;
        else if (l) ft = l->ty->kind == T_PTR ? l->ty->base : l->ty;
        if (!ft || ft->kind != T_FUNC) { fail_at(n->line, NULL, "that is not a function", NULL); n->ty = C.t_long; break; }
        n->ty = ft->base;
        /* Arguments take the parameters' kinds: a whole number to a
         * double parameter becomes a double, and the other way. */
        u32 k = 0;
        for (u32 a = n->aux, prev = 0; a; prev = a, a = N(a)->next, k++) {
            if (k >= ft->nparams) break;
            type *want = ft->params[k];
            node *an = N(a);
            u32 rep = 0;
            if (is_flt(want) && !is_flt(an->ty)) rep = mk_cast(a, C.t_double);
            else if (!is_flt(want) && is_num(want) && is_flt(an->ty)) rep = mk_cast(a, C.t_long);
            else if (want->kind == T_FLOAT && an->ty->kind == T_DOUBLE) rep = a;
            else if (want->boolean && an->ty && !an->ty->boolean) rep = mk_cast(a, want);   /* 0 or 1 on arrival */
            else if (is_rec(want) && an->kind != ND_BYVAL) {
                if (!is_rec(an->ty) || an->ty->size != want->size) { fail_at(n->line, NULL, "this parameter takes a struct", NULL); break; }
                if (!C.fn_ty) { fail_at(n->line, NULL, "a struct handed over outside a function is not here", NULL); break; }
                /* By value means the callee gets a copy of its own: a
                 * nameless local of the caller's, filled at the call,
                 * whose address travels like any other argument. */
                sym *tmp = sym_add("", want, S_LOCAL);
                tmp->val = local_slot(want);
                u32 bv = mk_un(ND_BYVAL, a);
                N(bv)->sym = (u32)(tmp - C.syms);
                N(bv)->ty = ptr_to(want);
                N(bv)->val = (i64)want->size;
                rep = bv;
            }
            if (rep && rep != a) {
                N(rep)->next = an->next;
                an->next = 0;
                if (prev) N(prev)->next = rep; else n->aux = rep;
                a = rep;
            }
        }
        /* A struct comes back in a nameless local of the caller's,
         * whose address goes ahead of the arguments; the callee hands
         * that address back in rax, so the call's value is the record
         * like any other. */
        if (is_rec(ft->base) && !n->post) {
            if (!C.fn_ty) { fail_at(n->line, NULL, "a call returning a struct outside a function is not here", NULL); break; }
            sym *tmp = sym_add("", ft->base, S_LOCAL);
            tmp->val = local_slot(ft->base);
            u32 v = mk(ND_VAR);
            N(v)->sym = (u32)(tmp - C.syms);
            N(v)->ty = ft->base;
            u32 ad = mk_un(ND_ADDR, v);
            N(ad)->ty = ptr_to(ft->base);
            N(ad)->next = n->aux;
            n->aux = ad;
            n->val++;
            n->post = true;
        }
        break;
    }

    case ND_CAST:
        break;

    case ND_INCDEC:
        n->ty = l->ty;
        break;

    case ND_SYSCALL: case ND_VASTART:
        n->ty = C.t_long;
        break;

    case ND_VAARG:
        break;                        /* set when made */

    default:
        n->ty = C.t_void;
        break;
    }
    /* A node whose typing failed has said so; what follows must still
     * find a type there rather than nothing. */
    if (!n->ty) n->ty = C.t_long;
}

/* ------------------------------------------------------------------ */
/* Reading declarations                                                */
/* ------------------------------------------------------------------ */

static u32 expr(void);
static u32 assign(void);
static i64 const_eval(u32 i);
static type *declspec(bool *is_typedef, bool *is_extern, bool *is_static);
static type *declarator(type *base, char *name);
static void skip_attributes(void);
static u32 stmt(void);

static bool is_typename_word(const char *w)
{
    static const char *const kws[] = {
        "void", "char", "short", "int", "long", "float", "double",
        "unsigned", "signed", "struct", "union", "enum", "const",
        "static", "extern", "volatile", "inline", "register", "restrict",
        "_Bool", "bool", "typedef", "__attribute__", "__builtin_va_list", NULL };
    for (u32 i = 0; kws[i]; i++) if (same(w, kws[i])) return true;
    return typedef_find(w) != NULL;
}

static bool is_typename(void)
{
    return C.cur.kind == TK_IDENT && is_typename_word(C.cur.text);
}

/* __attribute__((...)): packed and aligned(n) matter, the rest is
 * heard and let go. */
static u32 last_aligned;              /* aligned(n) most recently read */

static bool skip_one_attribute(void)
{
    bool packed = false;
    if (!is_kw("__attribute__")) return false;
    advance();
    expect("(");
    expect("(");
    i32 depth = 2;
    while (depth > 0 && C.cur.kind != TK_EOF && !C.bad) {
        if (is_kw("packed") || is_kw("__packed__")) packed = true;
        if ((is_kw("aligned") || is_kw("__aligned__")) && nxt_is("(")) {
            advance();
            advance();
            depth++;
            last_aligned = (u32)const_eval(assign());
            continue;
        }
        if (is("(")) depth++;
        if (is(")")) depth--;
        advance();
    }
    return packed;
}

static void skip_attributes(void)
{
    last_asm_name[0] = 0;
    last_aligned = 0;
    while (is_kw("__attribute__")) skip_one_attribute();
    /* __asm__("name") after a declarator: for a register variable it
     * names the register, which the inline assembly then binds to;
     * for anything else it names the symbol elsewhere, and here every
     * name is its own. */
    if (is_kw("__asm__") || is_kw("asm")) {
        advance();
        expect("(");
        if (C.cur.kind == TK_STR) {
            const u8 *s = C.spool + C.strs[C.cur.str].at;
            u32 k = 0;
            while (s[k] && k < NAME_MAX - 1) { last_asm_name[k] = (char)s[k]; k++; }
            last_asm_name[k] = 0;
            advance();
        }
        expect(")");
    }
    while (is_kw("__attribute__")) skip_one_attribute();
}

static type *record_decl(bool is_union)
{
    bool packed = false;
    while (is_kw("__attribute__")) packed |= skip_one_attribute();

    /* Tags have a namespace of their own: struct object is a fine
     * thing to say when object is also a typedef, and usually is. */
    char tag[NAME_MAX] = { 0 };
    if (C.cur.kind == TK_IDENT && !same(C.cur.text, "__attribute__")) {
        cpy(tag, C.cur.text);
        advance();
    }
    while (is_kw("__attribute__")) packed |= skip_one_attribute();

    type *t = tag[0] ? tag_find(tag) : NULL;
    if (!is("{")) {
        if (t) return t;
        if (!tag[0]) { fail(is_union ? "union wants a name or a body" : "struct wants a name or a body", NULL); return C.t_long; }
        t = new_type(is_union ? T_UNION : T_STRUCT, 0, 1);
        if (C.ntags < NTAGS) { cpy(C.tags[C.ntags].name, tag); C.tags[C.ntags].ty = t; C.ntags++; }
        return t;
    }
    advance();
    if (!t) {
        t = new_type(is_union ? T_UNION : T_STRUCT, 0, 1);
        if (tag[0] && C.ntags < NTAGS) { cpy(C.tags[C.ntags].name, tag); C.tags[C.ntags].ty = t; C.ntags++; }
    }
    if (t->defined) { fail("that struct already has a body", tag); return t; }
    t->packed = packed;

    t->mfirst = C.nmembers;
    /* The members of this struct, by index. A member whose type is an
     * inner struct body adds that body's members to the table while
     * this one is still being read, so the indices are gathered and
     * the entries copied to a contiguous run at the end. */
    u32 mine[STRUCT_MEMBERS_MAX];
    u32 nmine = 0;
    u32 off = 0, al = 1, bitpos = 0, unit_off = 0, unit_size = 0;
    while (!is("}") && C.cur.kind != TK_EOF && !C.bad) {
        if (is_kw("_Static_assert")) { stmt(); continue; }
        bool td, ex, st;
        type *base = declspec(&td, &ex, &st);
        for (;;) {
            char nm[NAME_MAX];
            type *mt = declarator(base, nm);
            skip_attributes();
            if (C.bad) return t;
            if (C.nmembers >= NMEMBERS) { fail("too many members", NULL); return t; }
            if (nmine >= STRUCT_MEMBERS_MAX) { fail("too many members in one struct", tag); return t; }
            mine[nmine++] = C.nmembers;
            member *m = &C.members[C.nmembers++];
            memset(m, 0, sizeof(*m));
            cpy(m->name, nm);
            m->ty = mt;
            u32 ma = packed ? 1 : (mt->align ? mt->align : 1);

            if (eat(":")) {
                /* A bit field: bits within a unit of its type's size,
                 * started afresh when they would not fit. */
                i64 w = const_eval(assign());
                if (w <= 0 || w > 64) { fail("a bit field wants a width from 1 to 64", nm); return t; }
                if (is_union) { unit_off = 0; bitpos = 0; unit_size = mt->size; }
                else if (unit_size != mt->size || bitpos + w > unit_size * 8) {
                    off = (off + ma - 1) / ma * ma;
                    unit_off = off;
                    unit_size = mt->size;
                    bitpos = 0;
                    off += unit_size;
                }
                m->offset = unit_off;
                m->bit_off = (u8)bitpos;
                m->bit_width = (u8)w;
                bitpos += (u32)w;
                if (is_union && mt->size > off) off = mt->size;
            } else {
                unit_size = 0;
                bitpos = 0;
                if (is_union) {
                    m->offset = 0;
                    if (mt->size > off) off = mt->size;
                } else {
                    off = (off + ma - 1) / ma * ma;
                    m->offset = off;
                    off += mt->size;
                }
            }
            if (ma > al) al = ma;
            if (!eat(",")) break;
        }
        expect(";");
    }
    expect("}");
    while (is_kw("__attribute__")) packed |= skip_one_attribute();
    if (packed) al = 1;
    t->packed = packed;
    if (C.nmembers - t->mfirst != nmine) {
        /* inner bodies interleaved their members with ours: copy ours
         * to the end, in order */
        if (C.nmembers + nmine > NMEMBERS) { fail("too many members", NULL); return t; }
        u32 start = C.nmembers;
        for (u32 i = 0; i < nmine; i++) {
            memcpy(&C.members[C.nmembers], &C.members[mine[i]], sizeof(member));
            C.nmembers++;
        }
        t->mfirst = start;
    }
    t->mcount = nmine;
    t->align = al;
    t->size = (off + al - 1) / al * al;
    t->defined = true;
    return t;
}

static type *enum_decl(void)
{
    if (C.cur.kind == TK_IDENT && !is("{")) advance();
    if (!eat("{")) return C.t_int;
    i64 v = 0;
    while (!is("}") && C.cur.kind != TK_EOF && !C.bad) {
        if (C.cur.kind != TK_IDENT) { fail("an enum wants names", C.cur.text); return C.t_int; }
        char nm[NAME_MAX];
        cpy(nm, C.cur.text);
        advance();
        if (eat("=")) v = const_eval(assign());
        sym *s = sym_add(nm, C.t_int, S_ENUM);
        s->val = v++;
        if (!eat(",")) break;
    }
    expect("}");
    return C.t_int;
}

static type *declspec(bool *is_typedef, bool *is_extern, bool *is_static)
{
    *is_typedef = false;
    *is_extern = false;
    *is_static = false;
    bool uns = false, sgn = false, saw_long = false, saw_short = false, boolean = false;
    i32 base = -1;
    type *named = NULL;
    for (;;) {
        if (C.cur.kind != TK_IDENT) break;
        const char *w = C.cur.text;
        if (same(w, "typedef")) { *is_typedef = true; advance(); continue; }
        if (same(w, "extern"))  { *is_extern = true; advance(); continue; }
        if (same(w, "static"))  { *is_static = true; advance(); continue; }
        if (same(w, "const") || same(w, "volatile") || same(w, "inline") ||
            same(w, "register") || same(w, "restrict") || same(w, "__inline__") ||
            same(w, "__restrict")) { advance(); continue; }
        if (same(w, "__attribute__")) { skip_one_attribute(); continue; }
        if (same(w, "unsigned")) { uns = true; advance(); continue; }
        if (same(w, "signed"))   { sgn = true; advance(); continue; }
        if (same(w, "void"))   { base = T_VOID; advance(); continue; }
        if (same(w, "char"))   { base = T_CHAR; advance(); continue; }
        if (same(w, "_Bool") || same(w, "bool")) { base = T_CHAR; uns = true; boolean = true; advance(); continue; }
        if (same(w, "short"))  { saw_short = true; advance(); continue; }
        if (same(w, "int"))    { if (base < 0) base = T_INT; advance(); continue; }
        if (same(w, "long"))   { saw_long = true; advance(); continue; }
        if (same(w, "float"))  { base = T_FLOAT; advance(); continue; }
        if (same(w, "double")) { base = T_DOUBLE; advance(); continue; }
        if (same(w, "__builtin_va_list")) { named = C.t_charp; advance(); continue; }
        if (same(w, "struct")) { advance(); named = record_decl(false); continue; }
        if (same(w, "union"))  { advance(); named = record_decl(true); continue; }
        if (same(w, "enum"))   { advance(); named = enum_decl(); continue; }
        type *td = typedef_find(w);
        if (td && base < 0 && !named && !saw_long && !saw_short && !uns && !sgn) { named = td; advance(); continue; }
        break;
    }
    if (named) return named;
    if (saw_short) base = T_SHORT;
    else if (saw_long && base != T_DOUBLE) base = T_LONG;
    if (base < 0) {
        if (uns || sgn) base = T_INT;
        else { fail("a type was expected here, not", C.cur.text); return C.t_long; }
    }
    switch (base) {
    case T_VOID:   return C.t_void;
    case T_CHAR:   return boolean ? C.t_bool : uns ? C.t_uchar : C.t_char;
    case T_SHORT:  return uns ? C.t_ushort : C.t_short;
    case T_INT:    return uns ? C.t_uint : C.t_int;
    case T_FLOAT:  return C.t_float;
    case T_DOUBLE: return C.t_double;
    default:       return uns ? C.t_ulong : C.t_long;
    }
}

static type *func_suffix(type *ret, char pnames[NPARAMS][NAME_MAX])
{
    type *f = new_type(T_FUNC, 8, 8);
    f->base = ret;
    if (is_kw("void") && nxt_is(")")) advance();
    while (!is(")") && C.cur.kind != TK_EOF && !C.bad) {
        if (eat("...")) { f->variadic = true; break; }
        if (f->nparams >= NPARAMS) { fail("sixteen parameters is the most a function takes here", NULL); return f; }
        bool td, ex, st;
        type *pt = declspec(&td, &ex, &st);
        char nm[NAME_MAX];
        pt = declarator(pt, nm);
        skip_attributes();
        if (pt->kind == T_ARR) pt = ptr_to(pt->base);
        if (pt->kind == T_FUNC) pt = ptr_to(pt);
        if (pnames) cpy(pnames[f->nparams], nm);
        f->params[f->nparams++] = pt;
        if (!eat(",")) break;
    }
    expect(")");
    return f;
}

static type *declarator(type *base, char *name)
{
    name[0] = 0;
    for (;;) {
        if (eat("*")) { base = ptr_to(base); continue; }
        if (is_kw("const") || is_kw("volatile") || is_kw("restrict") || is_kw("__restrict")) { advance(); continue; }
        break;
    }

    /* ( * name [n] ) ( params ): a pointer to a function, or an array
     * of them; ( * name ) [n]: a pointer to an array. */
    if (is("(") && nxt_is("*")) {
        advance();
        advance();
        while (is_kw("const") || is_kw("volatile") || is_kw("restrict")) advance();
        if (C.cur.kind == TK_IDENT) { cpy(name, C.cur.text); advance(); }
        u64 dims[4];
        u32 nd = 0;
        while (eat("[")) {
            if (nd >= 4) { fail("too many dimensions", NULL); return base; }
            dims[nd++] = is("]") ? 0 : (u64)const_eval(assign());
            expect("]");
        }
        expect(")");
        type *pointee = base;
        if (eat("(")) {
            pointee = func_suffix(base, NULL);
        } else {
            u64 od[8];
            u32 on = 0;
            while (eat("[")) {
                if (on >= 8) { fail("too many dimensions", NULL); return base; }
                od[on++] = is("]") ? 0 : (u64)const_eval(assign());
                expect("]");
            }
            while (on) pointee = array_of(pointee, od[--on]);
        }
        type *t = ptr_to(pointee);
        while (nd) t = array_of(t, dims[--nd]);
        return t;
    }

    /* A type's name may still name a member or a variable once the
     * type before it has been read: members and locals shadow. */
    if (C.cur.kind == TK_IDENT) {
        cpy(name, C.cur.text);
        advance();
    }

    if (eat("(")) return func_suffix(base, NULL);

    u64 dims[8];
    u32 nd = 0;
    while (eat("[")) {
        if (nd >= 8) { fail("too many dimensions", NULL); return base; }
        if (is("]")) dims[nd++] = 0;
        else dims[nd++] = (u64)const_eval(assign());
        expect("]");
    }
    while (nd) base = array_of(base, dims[--nd]);
    return base;
}

/* A type without a name, for casts and sizeof. */
static type *abstract_type(void)
{
    bool td, ex, st;
    type *t = declspec(&td, &ex, &st);
    char nm[NAME_MAX];
    return declarator(t, nm);
}

/* ------------------------------------------------------------------ */
/* Reading expressions                                                 */
/* ------------------------------------------------------------------ */

static u32 cast_expr(void);

static u32 arg_list(u32 n, u32 most, const char *what)
{
    u32 tail = 0, count = 0;
    while (!is(")") && !C.bad) {
        u32 a = assign();
        if (count >= most) { fail(what, NULL); return n; }
        if (tail) N(tail)->next = a; else N(n)->aux = a;
        tail = a;
        count++;
        if (!eat(",")) break;
    }
    expect(")");
    N(n)->val = count;
    return n;
}

static u32 primary(void)
{
    if (eat("(")) {
        u32 e = expr();
        expect(")");
        return e;
    }
    if (C.cur.kind == TK_NUM) {
        u32 n = mk_num(C.cur.val);
        advance();
        return n;
    }
    if (C.cur.kind == TK_FNUM) {
        u32 n = mk(ND_FNUM);
        N(n)->fval = C.cur.fval;
        N(n)->ty = C.t_double;
        advance();
        return n;
    }
    if (C.cur.kind == TK_STR) {
        u32 n = mk(ND_STR);
        N(n)->aux = C.cur.str;
        advance();
        while (C.cur.kind == TK_STR) {
            u32 a = C.strs[N(n)->aux].at;
            u32 al = C.strs[N(n)->aux].n - 1;
            u32 b = C.strs[C.cur.str].at;
            u32 bl = C.strs[C.cur.str].n;
            if (C.spool_len + al + bl >= STR_POOL || C.nstrs >= NSTR) { fail("the strings are too long together", NULL); return n; }
            u32 at = C.spool_len;
            memcpy(C.spool + C.spool_len, C.spool + a, al);
            C.spool_len += al;
            memcpy(C.spool + C.spool_len, C.spool + b, bl);
            C.spool_len += bl;
            C.strs[C.nstrs].at = at;
            C.strs[C.nstrs].n = al + bl;
            N(n)->aux = C.nstrs++;
            advance();
        }
        return n;
    }
    if (is_kw("sizeof")) {
        advance();
        if (is("(") && C.nxt.kind == TK_IDENT && is_typename_word(C.nxt.text)) {
            advance();
            type *t = abstract_type();
            expect(")");
            return mk_num((i64)t->size);
        }
        u32 e = cast_expr();
        add_type(e);
        return mk_num((i64)N(e)->ty->size);
    }
    if (is_kw("__builtin_offsetof")) {
        advance();
        expect("(");
        type *t = abstract_type();
        expect(",");
        if (C.cur.kind != TK_IDENT || !is_rec(t)) { fail("offsetof wants a struct and a member", C.cur.text); return mk_num(0); }
        i64 off = -1;
        for (u32 i = 0; i < t->mcount; i++)
            if (same(C.members[t->mfirst + i].name, C.cur.text)) off = (i64)C.members[t->mfirst + i].offset;
        if (off < 0) { fail("the struct has no member called", C.cur.text); return mk_num(0); }
        advance();
        expect(")");
        return mk_num(off);
    }
    if (is_kw("syscall")) {
        advance();
        expect("(");
        return arg_list(mk(ND_SYSCALL), 6, "syscall takes the number and five arguments at most");
    }
    if (is_kw("__builtin_va_start")) {
        advance();
        expect("(");
        u32 n = mk(ND_VASTART);
        N(n)->lhs = assign();
        if (eat(",")) assign();                    /* the last named parameter: not needed */
        expect(")");
        return n;
    }
    if (is_kw("__builtin_va_arg")) {
        advance();
        expect("(");
        u32 n = mk(ND_VAARG);
        N(n)->lhs = assign();
        expect(",");
        N(n)->ty = abstract_type();
        expect(")");
        return n;
    }
    if (is_kw("__builtin_va_end")) {
        advance();
        expect("(");
        assign();
        expect(")");
        return mk_num(0);
    }
    if (is_kw("__builtin_return_address") || is_kw("__builtin_expect")) {
        advance();
        expect("(");
        u32 e = assign();
        while (eat(",")) assign();
        expect(")");
        return e;
    }
    if (C.cur.kind == TK_IDENT) {
        sym *s = sym_find(C.cur.text);
        if (!s) { fail("unknown name", C.cur.text); return mk_num(0); }
        u32 idx = (u32)(s - C.syms);
        advance();
        if (s->kind == S_ENUM) return mk_num(s->val);
        u32 n = mk(ND_VAR);
        N(n)->sym = idx;
        return n;
    }
    fail("cannot parse", C.cur.text);
    return mk_num(0);
}

static u32 postfix(void)
{
    u32 n = primary();
    for (;;) {
        if (eat("[")) {
            u32 idx = expr();
            expect("]");
            n = mk_un(ND_DEREF, mk_bin(ND_ADD, n, idx));
            continue;
        }
        if (eat("(")) {
            u32 c = mk(ND_CALL);
            node *fn = N(n);
            if (fn->kind == ND_VAR && C.syms[fn->sym].kind == S_FUNC) N(c)->sym = fn->sym;
            else N(c)->lhs = n;
            n = arg_list(c, NPARAMS, "sixteen arguments is the most a call takes here");
            continue;
        }
        if (is(".") || is("->")) {
            bool arrow = is("->");
            advance();
            if (arrow) n = mk_un(ND_DEREF, n);
            add_type(n);
            type *t = N(n)->ty;
            if (!is_rec(t)) { fail("only a struct or union has members", C.cur.text); return n; }
            if (C.cur.kind != TK_IDENT) { fail("a member's name was expected", C.cur.text); return n; }
            i64 found = -1;
            for (u32 i = 0; i < t->mcount; i++)
                if (same(C.members[t->mfirst + i].name, C.cur.text)) { found = (i64)(t->mfirst + i); break; }
            if (found < 0) { fail("no member is called", C.cur.text); return n; }
            advance();
            u32 m = mk_un(ND_MEMBER, n);
            N(m)->aux = (u32)found;
            n = m;
            continue;
        }
        if (is("++") || is("--")) {
            u32 d = mk_un(ND_INCDEC, n);
            N(d)->val = is("++") ? 1 : -1;
            N(d)->post = true;
            advance();
            n = d;
            continue;
        }
        return n;
    }
}

static u32 unary(void)
{
    if (eat("+")) return cast_expr();
    if (eat("-")) return mk_un(ND_NEG, cast_expr());
    if (eat("!")) return mk_un(ND_NOT, cast_expr());
    if (eat("~")) return mk_un(ND_BITNOT, cast_expr());
    if (eat("*")) return mk_un(ND_DEREF, cast_expr());
    if (eat("&")) return mk_un(ND_ADDR, cast_expr());
    if (is("++") || is("--")) {
        i64 d = is("++") ? 1 : -1;
        advance();
        u32 n = mk_un(ND_INCDEC, unary());
        N(n)->val = d;
        return n;
    }
    return postfix();
}

static void initializer(type *t, u32 off, bool allow_dyn);
static u32 init_const_emit_later(u32 size);

/* (type){ ... }: a nameless local of that type, filled in place; its
 * value is the object, which for an array means its address. */
static u32 compound_literal(type *t)
{
    if (!C.fn_ty) { fail("a compound literal outside a function is not here", NULL); return mk_num(0); }
    sym *s = sym_add("", t, S_LOCAL);
    u32 si = (u32)(s - C.syms);
    memset(C.ibuf, 0, t->size > INIT_MAX ? INIT_MAX : t->size + 8);
    C.nfix = 0;
    C.ndyn = 0;
    initializer(t, 0, true);
    if (C.bad) return mk_num(0);
    s->val = local_slot(t);

    bool any = false;
    for (u32 k = 0; k < t->size && !any; k++) if (C.ibuf[k]) any = true;
    u32 v = mk(ND_VAR); N(v)->sym = si;
    u32 chain;
    if (any || C.nfix) {
        u32 ic = init_const_emit_later(t->size);
        chain = mk_un(ND_MEMCPY, v);
        N(chain)->aux = ic;
        N(chain)->val = t->size;
    } else {
        chain = mk_un(ND_ZERO, v);
        N(chain)->val = t->size;
    }
    for (u32 k = 0; k < C.ndyn; k++) {
        u32 v2 = mk(ND_VAR); N(v2)->sym = si;
        u32 pc = mk_cast(mk_un(ND_ADDR, v2), ptr_to(C.t_char));
        u32 at = mk_bin(ND_ADD, pc, mk_num(C.dyns[k].off));
        u32 el = mk_un(ND_DEREF, mk_cast(at, ptr_to(C.dyns[k].ty)));
        chain = mk_bin(ND_COMMA, chain, mk_bin(ND_ASSIGN, el, C.dyns[k].expr));
    }
    u32 v3 = mk(ND_VAR); N(v3)->sym = si;
    return mk_bin(ND_COMMA, chain, v3);
}

static u32 cast_expr(void)
{
    if (is("(") && C.nxt.kind == TK_IDENT && is_typename_word(C.nxt.text)) {
        advance();
        type *t = abstract_type();
        expect(")");
        if (is("{")) return compound_literal(t);
        return mk_cast(cast_expr(), t);
    }
    return unary();
}

typedef struct { const char *op; u8 nd; bool swap; } binop_t;
static const binop_t levels[][5] = {
    { { "||", ND_LOR, false }, { NULL, 0, false } },
    { { "&&", ND_LAND, false }, { NULL, 0, false } },
    { { "|", ND_OR, false }, { NULL, 0, false } },
    { { "^", ND_XOR, false }, { NULL, 0, false } },
    { { "&", ND_AND, false }, { NULL, 0, false } },
    { { "==", ND_EQ, false }, { "!=", ND_NE, false }, { NULL, 0, false } },
    { { "<", ND_LT, false }, { "<=", ND_LE, false }, { ">", ND_LT, true }, { ">=", ND_LE, true }, { NULL, 0, false } },
    { { "<<", ND_SHL, false }, { ">>", ND_SHR, false }, { NULL, 0, false } },
    { { "+", ND_ADD, false }, { "-", ND_SUB, false }, { NULL, 0, false } },
    { { "*", ND_MUL, false }, { "/", ND_DIV, false }, { "%", ND_MOD, false }, { NULL, 0, false } },
};
#define NLEVELS (sizeof(levels) / sizeof(levels[0]))

static u32 binary(u32 level)
{
    if (level >= NLEVELS) return cast_expr();
    u32 n = binary(level + 1);
    for (;;) {
        const binop_t *hit = NULL;
        for (u32 i = 0; levels[level][i].op; i++)
            if (is(levels[level][i].op)) { hit = &levels[level][i]; break; }
        if (!hit) return n;
        advance();
        u32 r = binary(level + 1);
        n = hit->swap ? mk_bin(hit->nd, r, n) : mk_bin(hit->nd, n, r);
    }
}

static u32 conditional(void)
{
    u32 c = binary(0);
    if (!eat("?")) return c;
    u32 n = mk(ND_COND);
    N(n)->lhs = c;
    N(n)->rhs = expr();
    expect(":");
    N(n)->third = conditional();
    return n;
}

static u32 assign(void)
{
    u32 l = conditional();
    static const struct { const char *op; u8 nd; } ops[] = {
        { "=", 0 }, { "+=", ND_ADD }, { "-=", ND_SUB }, { "*=", ND_MUL },
        { "/=", ND_DIV }, { "%=", ND_MOD }, { "&=", ND_AND }, { "|=", ND_OR },
        { "^=", ND_XOR }, { "<<=", ND_SHL }, { ">>=", ND_SHR }, { NULL, 0 } };
    for (u32 i = 0; ops[i].op; i++) {
        if (is(ops[i].op)) {
            advance();
            u32 n = mk_bin(ND_ASSIGN, l, assign());
            N(n)->op = ops[i].nd;
            return n;
        }
    }
    return l;
}

static u32 expr(void)
{
    u32 n = assign();
    while (eat(",")) n = mk_bin(ND_COMMA, n, assign());
    return n;
}

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

typedef struct { i64 v; char label[NAME_MAX]; bool addr; } cval;

/* A value the compiler can work out itself, possibly an address --
 * a global's, a function's, a string's -- plus a number. */
static bool const_val(u32 i, cval *out)
{
    memset(out, 0, sizeof(*out));
    if (!i || C.bad) return false;
    node *n = N(i);
    cval a, b;
    switch (n->kind) {
    case ND_NUM: out->v = n->val; return true;
    case ND_FNUM: out->v = sf_to_int(n->fval); return true;
    case ND_STR: {
        u32 at = 0;
        const char *pre = ".Ls";
        while (pre[at]) { out->label[at] = pre[at]; at++; }
        u32 v = n->aux;
        char d[12];
        u32 nd = 0;
        if (v == 0) d[nd++] = '0';
        while (v) { d[nd++] = (char)('0' + v % 10); v /= 10; }
        while (nd) out->label[at++] = d[--nd];
        out->label[at] = 0;
        out->addr = true;
        return true;
    }
    case ND_VAR: {
        sym *s = &C.syms[n->sym];
        if (s->kind == S_FUNC) { cpy(out->label, symname(s->label)); out->addr = true; return true; }
        if (s->kind == S_GLOBAL && s->ty->kind == T_ARR) { cpy(out->label, symname(s->label)); out->addr = true; return true; }
        return false;
    }
    case ND_ADDR: {
        node *l = N(n->lhs);
        if (l->kind == ND_VAR && C.syms[l->sym].kind == S_GLOBAL) {
            cpy(out->label, symname(C.syms[l->sym].label)); out->addr = true; return true;
        }
        if (l->kind == ND_VAR && C.syms[l->sym].kind == S_FUNC) {
            cpy(out->label, symname(C.syms[l->sym].label)); out->addr = true; return true;
        }
        if (l->kind == ND_MEMBER && const_val(l->lhs, &a) && a.addr) { *out = a; out->v += C.members[l->aux].offset; return true; }
        if (l->kind == ND_DEREF && const_val(l->lhs, &a)) { *out = a; return true; }
        return false;
    }
    case ND_MEMBER:
        return false;
    case ND_NEG: if (!const_val(n->lhs, &a) || a.addr) return false; out->v = -a.v; return true;
    case ND_BITNOT: if (!const_val(n->lhs, &a) || a.addr) return false; out->v = ~a.v; return true;
    case ND_NOT: if (!const_val(n->lhs, &a) || a.addr) return false; out->v = !a.v; return true;
    case ND_CAST: if (!const_val(n->lhs, &a)) return false; *out = a;
        if (!a.addr && n->ty && is_int(n->ty) && n->ty->size < 8) {
            u64 m = (1ULL << (n->ty->size * 8)) - 1;
            out->v = (i64)((u64)a.v & m);
            if (!n->ty->uns && (out->v >> (n->ty->size * 8 - 1)) & 1) out->v -= (i64)(m + 1);
        }
        return true;
    case ND_COND: if (!const_val(n->lhs, &a) || a.addr) return false;
        return const_val(a.v ? n->rhs : n->third, out);
    case ND_ADD: case ND_SUB: case ND_MUL: case ND_DIV: case ND_MOD:
    case ND_AND: case ND_OR: case ND_XOR: case ND_SHL: case ND_SHR:
    case ND_EQ: case ND_NE: case ND_LT: case ND_LE: case ND_LAND: case ND_LOR: {
        if (!const_val(n->lhs, &a) || !const_val(n->rhs, &b)) return false;
        if (a.addr || b.addr) {
            /* an address plus or minus a number stays an address */
            if (n->kind == ND_ADD && a.addr && !b.addr) { *out = a; out->v += b.v; return true; }
            if (n->kind == ND_ADD && b.addr && !a.addr) { *out = b; out->v += a.v; return true; }
            if (n->kind == ND_SUB && a.addr && !b.addr) { *out = a; out->v -= b.v; return true; }
            return false;
        }
        i64 x = a.v, y = b.v;
        /* Folded in unsigned arithmetic, so that a constant that
         * overflows wraps the way the machine's own instruction would
         * instead of being undefined here; a shift keeps to 63. */
        bool edge = (y == -1 && x == (i64)0x8000000000000000ULL);
        switch (n->kind) {
        case ND_ADD: out->v = (i64)((u64)x + (u64)y); break;
        case ND_SUB: out->v = (i64)((u64)x - (u64)y); break;
        case ND_MUL: out->v = (i64)((u64)x * (u64)y); break;
        case ND_DIV: out->v = (y && !edge) ? x / y : (edge ? x : 0); break;
        case ND_MOD: out->v = (y && !edge) ? x % y : 0; break;
        case ND_AND: out->v = x & y; break;
        case ND_OR:  out->v = x | y; break;
        case ND_XOR: out->v = x ^ y; break;
        case ND_SHL: out->v = (i64)((u64)x << ((u64)y & 63)); break;
        case ND_SHR: out->v = x >> ((u64)y & 63); break;
        case ND_EQ:  out->v = x == y; break;
        case ND_NE:  out->v = x != y; break;
        case ND_LT:  out->v = x < y; break;
        case ND_LE:  out->v = x <= y; break;
        case ND_LAND: out->v = x && y; break;
        default:     out->v = x || y; break;
        }
        return true;
    }
    default:
        return false;
    }
}

static i64 const_eval(u32 i)
{
    cval c;
    if (!const_val(i, &c) || c.addr) {
        if (!C.bad) fail_at(N(i)->line, NULL, "that has to be a number the compiler can work out", NULL);
        return 0;
    }
    return c.v;
}

/* ------------------------------------------------------------------ */
/* Initializers                                                        */
/* ------------------------------------------------------------------ */

static void ibuf_put(u32 off, u64 v, u32 size)
{
    if (off + size > INIT_MAX) return;
    for (u32 i = 0; i < size; i++) C.ibuf[off + i] = (u8)(v >> (i * 8));
}

/* A constant double, as its bits: a literal, its negation, a whole
 * number, or a cast of one. */
static bool const_fbits(u32 i, fbits *out)
{
    node *n = N(i);
    switch (n->kind) {
    case ND_FNUM: *out = n->fval; return true;
    case ND_NUM:  *out = sf_from_int(n->val); return true;
    case ND_NEG:  if (!const_fbits(n->lhs, out)) return false; *out ^= 1ULL << 63; return true;
    case ND_CAST: return const_fbits(n->lhs, out);
    default: {
        cval c;
        if (!const_val(i, &c) || c.addr) return false;
        *out = sf_from_int(c.v);
        return true;
    }
    }
}

static void init_scalar(type *t, u32 off, u32 e, bool allow_dyn)
{
    if (off + t->size > INIT_MAX) { fail("the initializer is too large", NULL); return; }
    add_type(e);
    if (C.bad) return;
    if (is_flt(t)) {
        fbits b;
        if (const_fbits(e, &b)) {
            if (t->kind == T_FLOAT) ibuf_put(off, sf_to_single(b), 4);
            else ibuf_put(off, b, 8);
            return;
        }
    }
    cval c;
    if (const_val(e, &c)) {
        if (c.addr) {
            if (C.nfix >= NFIX) { fail("too many addresses in initializers", NULL); return; }
            C.fixes[C.nfix].off = off;
            cpy(C.fixes[C.nfix].label, c.label);
            C.fixes[C.nfix].addend = c.v;
            C.nfix++;
            return;
        }
        ibuf_put(off, (u64)c.v, t->size);
        return;
    }
    if (!allow_dyn) { fail_at(N(e)->line, NULL, "a global starts as something the compiler can work out", NULL); return; }
    if (C.ndyn >= NDYN) { fail("too many computed initializers", NULL); return; }
    C.dyns[C.ndyn].off = off;
    C.dyns[C.ndyn].ty = t;
    C.dyns[C.ndyn].expr = e;
    C.ndyn++;
}

static void initializer(type *t, u32 off, bool allow_dyn);

/* { a, b, .name = c, [3] = d, ... } for an array or a record; a bare
 * string for a char array. */
/* Neighbouring strings join into one, wherever a string may stand. */
static u32 joined_string(void)
{
    u32 s = C.cur.str;
    advance();
    while (C.cur.kind == TK_STR && !C.bad) {
        u32 a = C.strs[s].at, al = C.strs[s].n - 1;
        u32 bb = C.strs[C.cur.str].at, bl = C.strs[C.cur.str].n;
        if (C.spool_len + al + bl >= STR_POOL || C.nstrs >= NSTR) { fail("the strings are too long together", NULL); return s; }
        u32 at = C.spool_len;
        memcpy(C.spool + C.spool_len, C.spool + a, al); C.spool_len += al;
        memcpy(C.spool + C.spool_len, C.spool + bb, bl); C.spool_len += bl;
        C.strs[C.nstrs].at = at; C.strs[C.nstrs].n = al + bl;
        s = C.nstrs++;
        advance();
    }
    return s;
}

static void initializer(type *t, u32 off, bool allow_dyn)
{
    if (t->kind == T_ARR && C.cur.kind == TK_STR && t->base->kind == T_CHAR) {
        u32 s = joined_string();
        if (t->len == 0) { t->len = C.strs[s].n; t->size = (u32)t->len; }
        for (u32 k = 0; k < C.strs[s].n && k < t->len && off + k < INIT_MAX; k++)
            C.ibuf[off + k] = C.spool[C.strs[s].at + k];
        return;
    }
    if (!is("{")) {
        if (t->kind == T_ARR || is_rec(t)) {
            /* a record from an expression: only a whole-record copy */
            if (is_rec(t) && allow_dyn) {
                u32 e = assign();
                if (C.ndyn >= NDYN) { fail("too many computed initializers", NULL); return; }
                C.dyns[C.ndyn].off = off; C.dyns[C.ndyn].ty = t; C.dyns[C.ndyn].expr = e; C.ndyn++;
                return;
            }
            fail("an array or struct starts with braces", NULL);
            return;
        }
        /* A constant's nodes are spent once it is evaluated, so a long
         * table (a font, say) does not fill the tree. */
        u32 mark = C.nnodes;
        init_scalar(t, off, assign(), allow_dyn);
        if (!allow_dyn && !C.bad) C.nnodes = mark;
        return;
    }
    advance();                                     /* { */
    if (t->kind == T_ARR) {
        u64 idx = 0, high = 0;
        while (!is("}") && C.cur.kind != TK_EOF && !C.bad) {
            if (eat("[")) { idx = (u64)const_eval(assign()); expect("]"); expect("="); }
            if (t->len && idx >= t->len) { fail("more values than the array has room for", NULL); return; }
            if (t->base->size && off + idx * t->base->size + t->base->size > INIT_MAX) { fail("the initializer is too large", NULL); return; }
            initializer(t->base, (u32)(off + idx * t->base->size), allow_dyn);
            idx++;
            if (idx > high) high = idx;
            if (!eat(",")) break;
        }
        expect("}");
        if (t->len == 0) { t->len = high; t->size = (u32)(high * t->base->size); }
        return;
    }
    if (is_rec(t)) {
        u32 mi = 0;
        while (!is("}") && C.cur.kind != TK_EOF && !C.bad) {
            if (eat(".")) {
                if (C.cur.kind != TK_IDENT) { fail("a member's name was expected", C.cur.text); return; }
                i64 found = -1;
                for (u32 i = 0; i < t->mcount; i++)
                    if (same(C.members[t->mfirst + i].name, C.cur.text)) { found = (i64)i; break; }
                if (found < 0) { fail("no member is called", C.cur.text); return; }
                advance();
                expect("=");
                mi = (u32)found;
            }
            if (mi >= t->mcount) { fail("more values than the struct has members", NULL); return; }
            member *m = &C.members[t->mfirst + mi];
            if (m->bit_width) {
                i64 v = const_eval(assign());
                u64 mask = m->bit_width == 64 ? ~0ULL : ((1ULL << m->bit_width) - 1);
                u64 have = 0;
                for (u32 k = 0; k < m->ty->size; k++) have |= (u64)C.ibuf[off + m->offset + k] << (k * 8);
                have &= ~(mask << m->bit_off);
                have |= ((u64)v & mask) << m->bit_off;
                ibuf_put(off + m->offset, have, m->ty->size);
            } else {
                initializer(m->ty, off + m->offset, allow_dyn);
            }
            mi++;
            if (t->kind == T_UNION) { while (!is("}") && !C.bad) advance(); break; }
            if (!eat(",")) break;
        }
        expect("}");
        return;
    }
    /* { scalar } */
    init_scalar(t, off, assign(), allow_dyn);
    while (eat(",")) { if (is("}")) break; assign(); }
    expect("}");
}

/* ------------------------------------------------------------------ */
/* Reading statements                                                  */
/* ------------------------------------------------------------------ */

static u32 block(void)
{
    u32 b = mk(ND_BLOCK);
    scope_in();
    u32 tail = 0;
    while (!is("}") && C.cur.kind != TK_EOF && !C.bad) {
        u32 s = stmt();
        if (!s) continue;
        if (tail) N(tail)->next = s; else N(b)->aux = s;
        tail = s;
    }
    expect("}");
    scope_out();
    return b;
}

static void block_append(u32 b, u32 *tail, u32 s)
{
    if (!s) return;
    if (*tail) N(*tail)->next = s; else N(b)->aux = s;
    *tail = s;
}

/* The hidden constant a local initializer copies from. */
static u32 init_const_emit_later(u32 size);

typedef struct { u32 no; u32 size; u8 *bytes; fixup *fixes; u32 nfix; } init_const;
static init_const iconsts[512];
static u8 iconst_pool[65536];

/* The addresses initializers carry, all of a text's together: a table
 * of a dozen programs with three pointers a row is ordinary. */
#define FIXPOOL 65536
static fixup *fixpool;
static u32    fixpool_used;

static fixup *fixups_kept(u32 n)
{
    if (!fixpool) fixpool = (fixup *)lang_big_alloc(sizeof(fixup) * FIXPOOL);
    if (!fixpool || fixpool_used + n > FIXPOOL) { fail("too many addresses in initializers together", NULL); return NULL; }
    fixup *f = fixpool + fixpool_used;
    memcpy(f, C.fixes, sizeof(fixup) * n);
    fixpool_used += n;
    return f;
}
static u32 iconst_used;

static u32 init_const_emit_later(u32 size)
{
    if (C.ninit_consts >= 64 || iconst_used + size > sizeof(iconst_pool)) { fail("too many local initializers", NULL); return 0; }
    init_const *ic = &iconsts[C.ninit_consts];
    ic->no = C.ninit_consts;
    ic->size = size;
    ic->bytes = iconst_pool + iconst_used;
    memcpy(ic->bytes, C.ibuf, size);
    ic->nfix = C.nfix;
    ic->fixes = fixups_kept(C.nfix);
    if (!ic->fixes) ic->nfix = 0;
    iconst_used += size;
    return C.ninit_consts++;
}

static u32 local_decl(void)
{
    bool td, ex, st;
    type *base = declspec(&td, &ex, &st);
    u32 b = mk(ND_BLOCK);
    u32 tail = 0;
    for (;;) {
        char nm[NAME_MAX];
        type *t = declarator(base, nm);
        skip_attributes();
        if (C.bad) return b;
        if (td) {
            typedef_add(nm, t);
        } else if (nm[0] && st) {
            /* A static local is a global with a private name. */
            char label[NAME_MAX];
            u32 k = 0;
            while (nm[k] && k < NAME_MAX - 8) { label[k] = nm[k]; k++; }
            label[k++] = '.';
            u32 v = ++C.statics;
            char d[12];
            u32 nd = 0;
            if (v == 0) d[nd++] = '0';
            while (v) { d[nd++] = (char)('0' + v % 10); v /= 10; }
            while (nd && k < NAME_MAX - 1) label[k++] = d[--nd];
            label[k] = 0;
            sym *s = sym_add(nm, t, S_GLOBAL);
            cpy(s->label, label);
            memset(C.ibuf, 0, t->size > INIT_MAX ? INIT_MAX : t->size + 8);
            C.nfix = 0;
            C.ndyn = 0;
            bool has = false;
            if (eat("=")) { initializer(t, 0, false); has = true; }
            global_add(label, t, has, false, true, 0);
        } else if (nm[0]) {
            if (t->kind == T_VOID) { fail("a variable cannot be void", nm); return b; }
            if (t->kind == T_FUNC) { sym_add(nm, t, S_FUNC); if (!eat(",")) break; continue; }
            sym *s = sym_add(nm, t, S_LOCAL);
            for (u32 k = 0; k < 8; k++) s->reg[k] = last_asm_name[k];
            if (eat("=")) {
                if (is("{") || (t->kind == T_ARR && C.cur.kind == TK_STR)) {
                    memset(C.ibuf, 0, t->size > INIT_MAX ? INIT_MAX : t->size + 8);
                    C.nfix = 0;
                    C.ndyn = 0;
                    initializer(t, 0, true);
                    if (C.bad) return b;
                    s->val = local_slot(t);
                    u32 si = (u32)(s - C.syms);
                    /* copy the constant part, then compute the rest */
                    bool any = false;
                    for (u32 k = 0; k < t->size && !any; k++) if (C.ibuf[k]) any = true;
                    u32 v = mk(ND_VAR); N(v)->sym = si;
                    if (any || C.nfix) {
                        u32 ic = init_const_emit_later(t->size);
                        u32 cp = mk_un(ND_MEMCPY, v);
                        N(cp)->aux = ic;
                        N(cp)->val = t->size;
                        block_append(b, &tail, cp);
                    } else {
                        u32 z = mk_un(ND_ZERO, v);
                        N(z)->val = t->size;
                        block_append(b, &tail, z);
                    }
                    for (u32 k = 0; k < C.ndyn; k++) {
                        u32 v2 = mk(ND_VAR); N(v2)->sym = si;
                        u32 addr = mk_un(ND_ADDR, v2);
                        u32 pc = mk_cast(addr, ptr_to(C.t_char));
                        u32 at = mk_bin(ND_ADD, pc, mk_num(C.dyns[k].off));
                        u32 tp = mk_cast(at, ptr_to(C.dyns[k].ty));
                        u32 el = mk_un(ND_DEREF, tp);
                        u32 as = mk_bin(ND_ASSIGN, el, C.dyns[k].expr);
                        block_append(b, &tail, mk_un(ND_EXPR, as));
                    }
                } else {
                    s->val = local_slot(t);
                    u32 v = mk(ND_VAR); N(v)->sym = (u32)(s - C.syms);
                    u32 as = mk_bin(ND_ASSIGN, v, assign());
                    block_append(b, &tail, mk_un(ND_EXPR, as));
                }
            } else {
                if (t->kind == T_ARR && t->len == 0) { fail("an array without a size needs a value", nm); return b; }
                s->val = local_slot(t);
            }
        }
        if (!eat(",")) break;
    }
    expect(";");
    return b;
}

static u32 stmt(void)
{
    if (eat(";")) return 0;
    if (eat("{")) return block();

    if (is_kw("_Static_assert")) {
        advance();
        expect("(");
        i64 v = const_eval(assign());
        char msg[NAME_MAX] = "";
        if (eat(",")) {
            if (C.cur.kind == TK_STR) {
                u32 k = 0;
                while (k < NAME_MAX - 1 && C.spool[C.strs[C.cur.str].at + k]) { msg[k] = (char)C.spool[C.strs[C.cur.str].at + k]; k++; }
                msg[k] = 0;
                advance();
            }
        }
        expect(")");
        expect(";");
        if (!v) fail("the static assertion does not hold:", msg);
        return 0;
    }
    if (is_kw("return")) {
        advance();
        u32 n = mk(ND_RETURN);
        if (!is(";")) N(n)->lhs = expr();
        expect(";");
        return n;
    }
    if (is_kw("if")) {
        advance();
        expect("(");
        u32 n = mk(ND_IF);
        N(n)->lhs = expr();
        expect(")");
        N(n)->rhs = stmt();
        if (is_kw("else")) { advance(); N(n)->third = stmt(); }
        return n;
    }
    if (is_kw("while")) {
        advance();
        expect("(");
        u32 n = mk(ND_WHILE);
        N(n)->lhs = expr();
        expect(")");
        N(n)->rhs = stmt();
        return n;
    }
    if (is_kw("do")) {
        advance();
        u32 n = mk(ND_DO);
        N(n)->rhs = stmt();
        if (!is_kw("while")) { fail("do wants its while", C.cur.text); return n; }
        advance();
        expect("(");
        N(n)->lhs = expr();
        expect(")");
        expect(";");
        return n;
    }
    if (is_kw("for")) {
        advance();
        expect("(");
        u32 n = mk(ND_FOR);
        scope_in();
        if (!is(";")) {
            if (is_typename()) N(n)->lhs = local_decl();
            else { N(n)->lhs = mk_un(ND_EXPR, expr()); expect(";"); }
        } else expect(";");
        if (!is(";")) N(n)->rhs = expr();
        expect(";");
        if (!is(")")) N(n)->third = expr();
        expect(")");
        N(n)->aux = stmt();
        scope_out();
        return n;
    }
    if (is_kw("break"))    { advance(); expect(";"); return mk(ND_BREAK); }
    if (is_kw("continue")) { advance(); expect(";"); return mk(ND_CONTINUE); }
    if (is_kw("goto")) {
        advance();
        if (C.cur.kind != TK_IDENT) { fail("goto where?", C.cur.text); return 0; }
        u32 n = mk(ND_GOTO);
        u32 g = 0;
        for (; g < C.ngotos; g++) if (same(C.gotos[g], C.cur.text)) break;
        if (g == C.ngotos) {
            if (C.ngotos >= NGOTO) { fail("too many labels", NULL); return 0; }
            cpy(C.gotos[C.ngotos++], C.cur.text);
        }
        N(n)->aux = g;
        advance();
        expect(";");
        return n;
    }
    if (is_kw("switch")) {
        advance();
        expect("(");
        u32 n = mk(ND_SWITCH);
        N(n)->lhs = expr();
        expect(")");
        if (C.nsw >= 32) { fail("switches nest too deep", NULL); return n; }
        C.sw_case[C.nsw++] = 0;
        N(n)->rhs = stmt();
        N(n)->aux = C.sw_case[--C.nsw];
        return n;
    }
    if (is_kw("case") || is_kw("default")) {
        bool dflt = is_kw("default");
        advance();
        u32 n = mk(dflt ? ND_DEFAULT : ND_CASE);
        if (!dflt) N(n)->val = const_eval(assign());
        expect(":");
        if (!C.nsw) { fail("case outside a switch", NULL); return n; }
        N(n)->third = C.sw_case[C.nsw - 1];
        C.sw_case[C.nsw - 1] = n;
        N(n)->aux = C.label++;
        N(n)->rhs = is("}") ? 0 : stmt();
        return n;
    }
    if (is_kw("asm") || is_kw("__asm__")) {
        advance();
        while (is_kw("volatile") || is_kw("__volatile__") || is_kw("inline") || is_kw("goto")) advance();
        expect("(");
        if (C.cur.kind != TK_STR) { fail("asm wants its text in quotes", C.cur.text); return 0; }
        if (nasm >= NASM) { fail("too many pieces of inline assembly", NULL); return 0; }

        /* The template: neighbouring strings join. */
        u32 tmpl = C.cur.str;
        advance();
        while (C.cur.kind == TK_STR) {
            u32 a = C.strs[tmpl].at, al = C.strs[tmpl].n - 1;
            u32 bb = C.strs[C.cur.str].at, bl = C.strs[C.cur.str].n;
            if (C.spool_len + al + bl >= STR_POOL || C.nstrs >= NSTR) { fail("the strings are too long together", NULL); return 0; }
            u32 at = C.spool_len;
            memcpy(C.spool + C.spool_len, C.spool + a, al); C.spool_len += al;
            memcpy(C.spool + C.spool_len, C.spool + bb, bl); C.spool_len += bl;
            C.strs[C.nstrs].at = at; C.strs[C.nstrs].n = al + bl;
            tmpl = C.nstrs++;
            advance();
        }

        asm_block *b = &asms[nasm];
        memset(b, 0, sizeof(*b));
        b->tmpl = tmpl;
        u32 n = mk(ND_ASM);
        N(n)->sym = nasm++;
        u32 tail = 0;

        for (u32 side = 0; side < 3 && eat(":"); side++) {
            while (!is(":") && !is(")") && C.cur.kind != TK_EOF && !C.bad) {
                if (side == 2) {                    /* clobbers: heard, not needed */
                    if (C.cur.kind != TK_STR) { fail("a clobber is a name in quotes", C.cur.text); return 0; }
                    advance();
                    if (!eat(",")) break;
                    continue;
                }
                if (b->nops >= ASM_OPS) { fail("too many operands to inline assembly", NULL); return 0; }
                asm_op *op = &b->ops[b->nops];
                if (eat("[")) {
                    if (C.cur.kind == TK_IDENT) { cpy(op->name, C.cur.text); advance(); }
                    expect("]");
                }
                if (C.cur.kind != TK_STR) { fail("a constraint in quotes was expected", C.cur.text); return 0; }
                const u8 *cs = C.spool + C.strs[C.cur.str].at;
                bool plus = false;
                u32 k = 0, ci = 0;
                while (cs[k] == '=' || cs[k] == '+' || cs[k] == '&') { if (cs[k] == '+') plus = true; k++; }
                while (cs[k] && ci < 7) op->cons[ci++] = (char)cs[k++];
                op->cons[ci] = 0;
                advance();
                expect("(");
                op->expr = assign();
                expect(")");
                op->out = side == 0;
                op->in = side == 1 || plus;
                op->mem = same(op->cons, "m") || same(op->cons, "o");
                op->imm = same(op->cons, "i") || same(op->cons, "n") || same(op->cons, "I");
                if (tail) N(tail)->next = op->expr; else N(n)->aux = op->expr;
                tail = op->expr;
                b->nops++;
                if (!eat(",")) break;
            }
        }
        expect(")");
        expect(";");
        return n;
    }
    /* label: */
    if (C.cur.kind == TK_IDENT && nxt_is(":") && !is_typename()) {
        u32 n = mk(ND_LABEL);
        u32 g = 0;
        for (; g < C.ngotos; g++) if (same(C.gotos[g], C.cur.text)) break;
        if (g == C.ngotos) {
            if (C.ngotos >= NGOTO) { fail("too many labels", NULL); return 0; }
            cpy(C.gotos[C.ngotos++], C.cur.text);
        }
        N(n)->aux = g;
        advance();
        advance();
        N(n)->rhs = is("}") ? 0 : stmt();
        return n;
    }
    if (is_typename()) return local_decl();

    u32 n = mk_un(ND_EXPR, expr());
    expect(";");
    return n;
}

/* ------------------------------------------------------------------ */
/* Writing                                                             */
/* ------------------------------------------------------------------ */

static void o_char(char c)
{
    if (C.len + 1 >= C.max) {
        if (!C.bad) {
            char head[81];
            u32 k = 0;
            while (k < 80 && k < C.len) { char h = C.out[k]; head[k] = (h >= 0x20 && h < 0x7F) ? h : '.'; k++; }
            head[k] = 0;
            kprintf("cc:   the assembly overflowed: len %u, room %u, head '%s'\n", (u32)C.len, (u32)C.max, head);
            fail_at(0, NULL, "the assembly exceeds the buffer", NULL);
        }
        return;
    }
    C.out[C.len++] = c;
}

static void o_str(const char *s) { while (*s) o_char(*s++); }

static void o_dec(i64 v)
{
    /* the magnitude in unsigned arithmetic: the most negative number
     * has no positive twin among the signed */
    u64 u = v < 0 ? (u64)0 - (u64)v : (u64)v;
    if (v < 0) o_char('-');
    char d[24];
    u32 n = 0;
    if (u == 0) d[n++] = '0';
    while (u) { d[n++] = (char)('0' + u % 10); u /= 10; }
    while (n) o_char(d[--n]);
}

/* %d a number, %s a string, %l a label number, %c a char. */
static void o(const char *fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    for (u32 i = 0; fmt[i]; i++) {
        if (fmt[i] != '%') { o_char(fmt[i]); continue; }
        i++;
        switch (fmt[i]) {
        case 'd': o_dec(__builtin_va_arg(ap, i64)); break;
        case 's': o_str(__builtin_va_arg(ap, const char *)); break;
        case 'l': o_str(".L"); o_dec((i64)__builtin_va_arg(ap, u32)); break;
        case 'c': o_char((char)__builtin_va_arg(ap, i32)); break;
        default: o_char(fmt[i]); break;
        }
    }
    __builtin_va_end(ap);
}

static const char *const arg64[REGARGS] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
static const char *const arg32[REGARGS] = { "edi", "esi", "edx", "ecx", "r8d", "r9d" };
static const char *const arg16[REGARGS] = { "di", "si", "dx", "cx", "r8w", "r9w" };
static const char *const arg8[REGARGS]  = { "dil", "sil", "dl", "cl", "r8b", "r9b" };
static const char *const argx[REGARGS]  = { "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5" };

static void gen_expr(u32 i);
static void gen_stmt(u32 i);

static void gen_addr(u32 i)
{
    node *n = N(i);
    switch (n->kind) {
    case ND_VAR: {
        sym *s = &C.syms[n->sym];
        if (s->kind == S_LOCAL) {
            /* Below rbp for the function's own; above it for the
             * arguments the caller left on the stack. A struct
             * parameter's slot holds the address of the caller's copy,
             * and that copy is the parameter. */
            if (s->indirect) {
                if (s->val < 0) o("    mov rax, [rbp - %d]\n", -s->val);
                else o("    mov rax, [rbp + %d]\n", s->val);
            }
            else if (s->val < 0) o("    lea rax, [rbp - %d]\n", -s->val);
            else o("    lea rax, [rbp + %d]\n", s->val);
        }
        else o("    lea rax, [%s]\n", symname(s->label));
        return;
    }
    case ND_STR:
        o("    lea rax, [.Ls%d]\n", (i64)n->aux);
        return;
    case ND_DEREF:
        gen_expr(n->lhs);
        return;
    case ND_MEMBER:
        gen_addr(n->lhs);
        if (C.members[n->aux].offset) o("    add rax, %d\n", (i64)C.members[n->aux].offset);
        return;
    case ND_COMMA:
        gen_expr(n->lhs);
        gen_addr(n->rhs);
        return;
    case ND_CAST:
        gen_addr(n->lhs);
        return;
    case ND_CALL:
        if (is_rec(n->ty)) { gen_expr(i); return; }   /* the record's address comes back in rax */
        fail_at(n->line, NULL, "that is not something with an address", NULL);
        return;
    default:
        fail_at(n->line, NULL, "that is not something with an address", NULL);
    }
}

/* What is at the address in rax, into rax (or xmm0 for a double). */
static void load(type *t)
{
    if (t->kind == T_ARR || is_rec(t) || t->kind == T_FUNC) return;
    if (t->kind == T_DOUBLE) { o("    movsd xmm0, [rax]\n"); return; }
    if (t->kind == T_FLOAT)  { o("    movss xmm0, dword [rax]\n    cvtss2sd xmm0, xmm0\n"); return; }
    if (t->size == 1) o(t->uns ? "    movzx rax, byte [rax]\n" : "    movsx rax, byte [rax]\n");
    else if (t->size == 2) o(t->uns ? "    movzx rax, word [rax]\n" : "    movsx rax, word [rax]\n");
    else if (t->size == 4) o(t->uns ? "    mov eax, dword [rax]\n" : "    movsxd rax, dword [rax]\n");
    else o("    mov rax, [rax]\n");
}

/* rax (or xmm0) into the address in rdi. */
static void store(type *t)
{
    if (is_rec(t)) {
        u32 off = 0;
        while (off + 8 <= t->size) { o("    mov rcx, [rax + %d]\n    mov [rdi + %d], rcx\n", (i64)off, (i64)off); off += 8; }
        while (off < t->size) { o("    mov cl, byte [rax + %d]\n    mov byte [rdi + %d], cl\n", (i64)off, (i64)off); off++; }
        return;
    }
    if (t->kind == T_DOUBLE) { o("    movsd [rdi], xmm0\n"); return; }
    if (t->kind == T_FLOAT)  { o("    cvtsd2ss xmm1, xmm0\n    movss dword [rdi], xmm1\n"); return; }
    if (t->size == 1) o("    mov byte [rdi], al\n");
    else if (t->size == 2) o("    mov word [rdi], ax\n");
    else if (t->size == 4) o("    mov dword [rdi], eax\n");
    else o("    mov [rdi], rax\n");
}

/* A bit field's value out of its unit (address in rax), into rax. */
static void load_bits(member *m)
{
    type *u = m->ty;
    if (u->size == 1) o("    movzx rax, byte [rax]\n");
    else if (u->size == 2) o("    movzx rax, word [rax]\n");
    else if (u->size == 4) o("    mov eax, dword [rax]\n");
    else o("    mov rax, [rax]\n");
    u32 left = 64 - m->bit_off - m->bit_width;
    if (left) o("    shl rax, %d\n", (i64)left);
    o(u->uns ? "    shr rax, %d\n" : "    sar rax, %d\n", (i64)(64 - m->bit_width));
}

/* rax into a bit field whose unit's address is in rdi. */
static void store_bits(member *m)
{
    type *u = m->ty;
    u64 mask = m->bit_width == 64 ? ~0ULL : ((1ULL << m->bit_width) - 1);
    o("    mov rcx, %d\n    and rax, rcx\n", (i64)mask);
    if (m->bit_off) o("    shl rax, %d\n", (i64)m->bit_off);
    o("    mov r9, rax\n");
    if (u->size == 1) o("    movzx rax, byte [rdi]\n");
    else if (u->size == 2) o("    movzx rax, word [rdi]\n");
    else if (u->size == 4) o("    mov eax, dword [rdi]\n");
    else o("    mov rax, [rdi]\n");
    o("    mov rcx, %d\n    not rcx\n    and rax, rcx\n    or rax, r9\n", (i64)(mask << m->bit_off));
    if (u->size == 1) o("    mov byte [rdi], al\n");
    else if (u->size == 2) o("    mov word [rdi], ax\n");
    else if (u->size == 4) o("    mov dword [rdi], eax\n");
    else o("    mov [rdi], rax\n");
}

static member *bitfield_of(u32 i)
{
    node *n = N(i);
    if (n->kind != ND_MEMBER) return NULL;
    member *m = &C.members[n->aux];
    return m->bit_width ? m : NULL;
}

/* rax or xmm0, from one type to another. */
static void cast_to(type *from, type *to)
{
    if (to->kind == T_VOID || is_rec(to) || to->kind == T_FUNC || to->kind == T_ARR) return;
    bool ff = is_flt(from), tf = is_flt(to);
    if (ff && !tf) {
        o("    cvttsd2si rax, xmm0\n");
        from = C.t_long;
    } else if (!ff && tf) {
        if (from->kind == T_LONG && from->uns) {
            /* an unsigned long above the sign bit: halve, convert, double */
            o("    test rax, rax\n    js %l\n    cvtsi2sd xmm0, rax\n    jmp %l\n%l:\n    mov rcx, rax\n    shr rcx, 1\n    and rax, 1\n    or rcx, rax\n    cvtsi2sd xmm0, rcx\n    addsd xmm0, xmm0\n%l:\n",
              C.label, C.label + 1, C.label, C.label + 1);
            C.label += 2;
        } else {
            o("    cvtsi2sd xmm0, rax\n");
        }
        return;
    } else if (ff && tf) {
        return;                       /* double and float both travel as double */
    }
    if (to->boolean) {
        /* a _Bool holds 0 or 1, whatever was assigned */
        if (!from->boolean) o("    test rax, rax\n    setne al\n    movzx rax, al\n");
        return;
    }
    if (to->size == 1) o(to->uns ? "    movzx rax, al\n" : "    movsx rax, al\n");
    else if (to->size == 2) o(to->uns ? "    movzx rax, ax\n" : "    movsx rax, ax\n");
    else if (to->size == 4) o(to->uns ? "    mov eax, eax\n" : "    movsxd rax, eax\n");
}

/* rax (or xmm0) as a truth value in rax. */
static void to_bool(type *t)
{
    if (is_flt(t)) o("    xorpd xmm1, xmm1\n    ucomisd xmm0, xmm1\n    setne al\n    movzx rax, al\n");
}

/* An operation on 32-bit operands answers 32 bits, the way c has it:
 * the upper half is cut off, and for a signed int the sign is carried
 * back up. Narrower kinds were promoted to int already. */
static void narrow(type *t)
{
    if (!t || t->size != 4 || is_flt(t) || t->kind == T_PTR) return;
    o(t->uns ? "    mov eax, eax\n" : "    movsxd rax, eax\n");
}

static void emit_binop(u8 kind, type *t)
{
    bool uns = t && t->uns;
    if (t && is_flt(t)) {
        switch (kind) {
        case ND_ADD: o("    addsd xmm0, xmm1\n"); return;
        case ND_SUB: o("    subsd xmm0, xmm1\n"); return;
        case ND_MUL: o("    mulsd xmm0, xmm1\n"); return;
        case ND_DIV: o("    divsd xmm0, xmm1\n"); return;
        case ND_EQ:  o("    ucomisd xmm0, xmm1\n    sete al\n    movzx rax, al\n"); return;
        case ND_NE:  o("    ucomisd xmm0, xmm1\n    setne al\n    movzx rax, al\n"); return;
        case ND_LT:  o("    ucomisd xmm1, xmm0\n    seta al\n    movzx rax, al\n"); return;
        case ND_LE:  o("    ucomisd xmm1, xmm0\n    setae al\n    movzx rax, al\n"); return;
        default: return;
        }
    }
    switch (kind) {
    case ND_ADD: o("    add rax, rdi\n"); break;
    case ND_SUB: o("    sub rax, rdi\n"); break;
    case ND_MUL: o("    imul rax, rdi\n"); break;
    case ND_DIV:
        if (uns) o("    xor edx, edx\n    div rdi\n");
        else o("    cqo\n    idiv rdi\n");
        break;
    case ND_MOD:
        if (uns) o("    xor edx, edx\n    div rdi\n    mov rax, rdx\n");
        else o("    cqo\n    idiv rdi\n    mov rax, rdx\n");
        break;
    case ND_AND: o("    and rax, rdi\n"); break;
    case ND_OR:  o("    or rax, rdi\n"); break;
    case ND_XOR: o("    xor rax, rdi\n"); break;
    case ND_SHL: o("    mov rcx, rdi\n    shl rax, cl\n"); break;
    case ND_SHR: o(uns ? "    mov rcx, rdi\n    shr rax, cl\n" : "    mov rcx, rdi\n    sar rax, cl\n"); break;
    case ND_EQ:  o("    cmp rax, rdi\n    sete al\n    movzx rax, al\n"); break;
    case ND_NE:  o("    cmp rax, rdi\n    setne al\n    movzx rax, al\n"); break;
    case ND_LT:  o(uns ? "    cmp rax, rdi\n    setb al\n    movzx rax, al\n" : "    cmp rax, rdi\n    setl al\n    movzx rax, al\n"); break;
    case ND_LE:  o(uns ? "    cmp rax, rdi\n    setbe al\n    movzx rax, al\n" : "    cmp rax, rdi\n    setle al\n    movzx rax, al\n"); break;
    default: break;
    }
}

/* Pushes a value: an integer from rax, a double from xmm0. */
static void push_val(type *t)
{
    if (is_flt(t)) o("    sub rsp, 8\n    movsd [rsp], xmm0\n");
    else o("    push rax\n");
}

/* ------------------------------------------------------------------ */
/* Simple operands                                                     */
/* ------------------------------------------------------------------ */

#define OPT_SIMPLE 1                  /* plain operands without push and pop */
#define OPT_PEEP   1                  /* the line-level tidying afterwards */
#define OPT_P1     1                  /* lea + load through rax -> one load */
#define OPT_P2     1                  /* push rax + pop R -> mov */
#define OPT_P3     1                  /* a jump to the next line -> nothing */
#define OPT_F4     1                  /* set, movzx, test, je -> one jump */

bool cc_peep_off = false;             /* set from outside to measure the tidying's worth */
const char *cc_peep_only = NULL;      /* or only in the functions named, comma-separated */

static bool name_listed(const char *list, const char *name)
{
    while (*list) {
        u32 i = 0;
        while (list[i] && list[i] != ',') i++;
        u32 k = 0;
        while (k < i && name[k] && name[k] == list[k]) k++;
        if (k == i && !name[k]) return true;
        list += list[i] == ',' ? i + 1 : i;
    }
    return false;
}

/* A plain scalar variable -- a local with a home below the frame, or
 * a global -- that can be read or written in one instruction, with no
 * address to compute first. */
static bool plain_var(node *n)
{
    if (!OPT_SIMPLE) return false;
    if (n->kind != ND_VAR) return false;
    sym *s = &C.syms[n->sym];
    type *t = s->ty;
    if (!t || is_flt(t) || is_rec(t) || t->kind == T_ARR || t->kind == T_FUNC) return false;
    if (s->reg[0]) return false;
    if (s->kind == S_LOCAL) return !s->indirect;
    return s->kind == S_GLOBAL;
}

/* Where a plain variable lives, as an operand. */
static void place_of(sym *s, char *out, u32 max)
{
    u32 n = 0;
    #define PUTC(ch) do { if (n < max - 1) out[n++] = (ch); } while (0)
    if (s->kind == S_LOCAL) {
        const char *p = s->val < 0 ? "[rbp - " : "[rbp + ";
        while (*p) PUTC(*p++);
        u64 v = s->val < 0 ? (u64)0 - (u64)s->val : (u64)s->val;
        char d[24]; u32 k = 0;
        if (v == 0) d[k++] = '0';
        while (v) { d[k++] = (char)('0' + v % 10); v /= 10; }
        while (k) PUTC(d[--k]);
    } else {
        PUTC('[');
        const char *p = symname(s->label);
        while (*p) PUTC(*p++);
    }
    PUTC(']');
    out[n] = 0;
    #undef PUTC
}

/* A right operand that needs no push and pop around it: a number, a
 * plain variable, or a plain variable under a widening cast. Once the
 * left side stands in rax, it goes straight into rdi. */
static bool simple_int(node *n)
{
    if (!OPT_SIMPLE) return false;
    if (n->kind == ND_NUM || plain_var(n)) return true;
    if (n->kind == ND_CAST && n->lhs) {
        node *in = N(n->lhs);
        type *to = n->ty, *from = in->ty;
        if (!to || !from || is_flt(to) || is_flt(from) || to->boolean) return false;
        if (is_rec(to) || is_rec(from) || to->size == 1) return false;
        return in->kind == ND_NUM || plain_var(in);
    }
    return false;
}

static void load_into_rdi(node *n)
{
    node *v = n->kind == ND_CAST ? N(n->lhs) : n;
    if (v->kind == ND_NUM) {
        o("    mov rdi, %d\n", v->val);
    } else {
        sym *s = &C.syms[v->sym];
        type *t = v->ty;
        char place[NAME_MAX + 24];
        place_of(s, place, sizeof(place));
        if (t->size == 1) o(t->uns ? "    movzx rdi, byte %s\n" : "    movsx rdi, byte %s\n", place);
        else if (t->size == 2) o(t->uns ? "    movzx rdi, word %s\n" : "    movsx rdi, word %s\n", place);
        else if (t->size == 4) o(t->uns ? "    mov edi, dword %s\n" : "    movsxd rdi, dword %s\n", place);
        else o("    mov rdi, %s\n", place);
    }
    if (n->kind == ND_CAST) {
        type *to = n->ty;
        if (to->size == 4) o(to->uns ? "    mov edi, edi\n" : "    movsxd rdi, edi\n");
        else if (to->size == 2) o(to->uns ? "    movzx rdi, di\n" : "    movsx rdi, di\n");
    }
}

/* rax into a plain variable, by its width. */
static void store_plain(sym *s, type *t)
{
    char place[NAME_MAX + 24];
    place_of(s, place, sizeof(place));
    if (t->size == 1) o("    mov byte %s, al\n", place);
    else if (t->size == 2) o("    mov word %s, ax\n", place);
    else if (t->size == 4) o("    mov dword %s, eax\n", place);
    else o("    mov %s, rax\n", place);
}

/* ------------------------------------------------------------------ */
/* Peephole                                                            */
/* ------------------------------------------------------------------ */

/* Line-level tidying of one function's assembly, in place. The
 * generator writes what is simplest to write; what is simplest to run
 * differs in a few fixed ways, each a matter of two adjacent lines:
 *
 *   lea rax, [X] / <load> [rax]   ->  <load> [X]
 *   push rax / pop R              ->  mov R, rax   (nothing, if R is rax)
 *   jmp .LN / .LN:                ->  .LN:
 *
 * Two passes, because a line made by one fusion can take part in the
 * next. Labels break adjacency, so a jump target is never fused over. */
static bool starts(const char *p, u32 len, const char *s)
{
    u32 i = 0;
    while (s[i]) { if (i >= len || p[i] != s[i]) return false; i++; }
    return true;
}

static bool line_is(const char *p, u32 len, const char *s)
{
    u32 i = 0;
    while (s[i]) { if (i >= len || p[i] != s[i]) return false; i++; }
    return i == len;
}

/* Answers the fused line's length in out, -1 when both lines go, 0
 * when nothing fuses. */
static i32 fuse(const char *prev, u32 plen, const char *cur, u32 clen, char *out, u32 max)
{
    u32 n = 0;
    if (OPT_P1 && starts(prev, plen, "    lea rax, [") && clen > 7 &&
        starts(cur + clen - 7, 7, " [rax]\n") &&
        (starts(cur, clen, "    mov ") || starts(cur, clen, "    movsx ") ||
         starts(cur, clen, "    movzx ") || starts(cur, clen, "    movsxd ") ||
         starts(cur, clen, "    movsd xmm0, ") || starts(cur, clen, "    movss xmm0, "))) {
        u32 xs = 14, xe = plen - 2;                 /* X between '[' and "]\n" */
        if (clen - 7 + 2 + (xe - xs) + 2 >= max) return 0;
        for (u32 i = 0; i < clen - 7; i++) out[n++] = cur[i];
        out[n++] = ' '; out[n++] = '[';
        for (u32 i = xs; i < xe; i++) out[n++] = prev[i];
        out[n++] = ']'; out[n++] = '\n';
        return (i32)n;
    }
    if (OPT_P2 && line_is(prev, plen, "    push rax\n") && starts(cur, clen, "    pop ") && clen > 9) {
        if (line_is(cur, clen, "    pop rax\n")) return -1;
        if (clen + 6 >= max) return 0;
        const char *m = "    mov ";
        while (*m) out[n++] = *m++;
        for (u32 i = 8; i < clen - 1; i++) out[n++] = cur[i];
        const char *r = ", rax\n";
        while (*r) out[n++] = *r++;
        return (i32)n;
    }
    if (OPT_P3 && starts(prev, plen, "    jmp .L") && cur[0] == '.' && plen > 9 && clen == plen - 8 + 1) {
        /* prev: "    jmp " LABEL "\n"; cur: LABEL ":\n" */
        u32 ll = plen - 9;
        bool same_label = true;
        for (u32 i = 0; i < ll; i++) if (prev[8 + i] != cur[i]) { same_label = false; break; }
        if (same_label && cur[ll] == ':' && cur[ll + 1] == '\n') {
            for (u32 i = 0; i < clen; i++) out[n++] = cur[i];
            return (i32)n;
        }
    }
    return 0;
}

/* The jump that takes a condition's place: setX al / movzx rax, al /
 * test rax, rax / je L asks whether X was false, which is one jump on
 * the inverse of X; with jne it is X itself. */
static const char *jump_for(const char *set, bool inverse)
{
    static const char *const cc[10][3] = {
        { "sete",  "je",  "jne" }, { "setne", "jne", "je"  },
        { "setl",  "jl",  "jge" }, { "setle", "jle", "jg"  },
        { "setg",  "jg",  "jle" }, { "setge", "jge", "jl"  },
        { "setb",  "jb",  "jae" }, { "setbe", "jbe", "ja"  },
        { "seta",  "ja",  "jbe" }, { "setae", "jae", "jb"  },
    };
    for (u32 i = 0; i < 10; i++) if (same(cc[i][0], set)) return cc[i][inverse ? 2 : 1];
    return NULL;
}

/* Four lines that are one jump. Answers the jump's length in out. */
static i32 fuse4(const char *l0, u32 n0, const char *l1, u32 n1,
                 const char *l2, u32 n2, const char *l3, u32 n3, char *out, u32 max)
{
    if (!OPT_F4) return 0;
    if (!starts(l0, n0, "    set") || n0 < 10 || !starts(l0 + n0 - 4, 4, " al\n")) return 0;
    if (!line_is(l1, n1, "    movzx rax, al\n") || !line_is(l2, n2, "    test rax, rax\n")) return 0;
    bool je = starts(l3, n3, "    je .L"), jne = starts(l3, n3, "    jne .L");
    if (!je && !jne) return 0;

    char set[8];
    u32 sl = n0 - 8;                              /* between the indent and " al\n" */
    if (sl >= sizeof(set)) return 0;
    for (u32 i = 0; i < sl; i++) set[i] = l0[4 + i];
    set[sl] = 0;
    const char *j = jump_for(set, je);
    if (!j) return 0;

    u32 label_at = je ? 7 : 8;                    /* after "    je " / "    jne " */
    u32 n = 0;
    if (4 + 8 + (n3 - label_at) + 1 >= max) return 0;
    out[n++] = ' '; out[n++] = ' '; out[n++] = ' '; out[n++] = ' ';
    while (*j) out[n++] = *j++;
    out[n++] = ' ';
    for (u32 i = label_at; i < n3; i++) out[n++] = l3[i];   /* the label and its newline */
    return (i32)n;
}

static void peephole(u32 from)
{
    char *buf = C.out;
    for (u32 pass = 0; pass < 2; pass++) {
        u32 r = from, w = from, end = C.len;
        u32 hs[3];                                /* starts of the last lines written, oldest first */
        u32 hn = 0;
        while (r < end) {
            u32 ls = r;
            while (r < end && buf[r] != '\n') r++;
            if (r < end) r++;
            u32 clen = r - ls;
            char fused[256];
            i32 fl;

            if (hn == 3 && clen < 200 && w - hs[0] < 600) {
                fl = fuse4(buf + hs[0], hs[1] - hs[0], buf + hs[1], hs[2] - hs[1],
                           buf + hs[2], w - hs[2], buf + ls, clen, fused, sizeof(fused));
                if (fl > 0) {
                    for (i32 i = 0; i < fl; i++) buf[hs[0] + i] = fused[i];
                    w = hs[0] + (u32)fl;
                    hn = 1;                       /* the jump stands where the four stood */
                    continue;
                }
            }
            if (hn && clen < 200 && w - hs[hn - 1] < 200) {
                u32 pw = hs[hn - 1];
                fl = fuse(buf + pw, w - pw, buf + ls, clen, fused, sizeof(fused));
                if (fl > 0) {
                    for (i32 i = 0; i < fl; i++) buf[pw + i] = fused[i];
                    w = pw + (u32)fl;             /* the fused line stands where prev stood */
                    continue;
                }
                if (fl < 0) { w = pw; hn--; continue; }
            }
            if (w != ls) for (u32 i = 0; i < clen; i++) buf[w + i] = buf[ls + i];
            if (hn == 3) { hs[0] = hs[1]; hs[1] = hs[2]; hn = 2; }
            hs[hn++] = w;
            w += clen;
        }
        C.len = w;
    }
}

/* A plain variable into rax, by its width. */
static void load_plain(sym *s, type *t)
{
    char place[NAME_MAX + 24];
    place_of(s, place, sizeof(place));
    if (t->size == 1) o(t->uns ? "    movzx rax, byte %s\n" : "    movsx rax, byte %s\n", place);
    else if (t->size == 2) o(t->uns ? "    movzx rax, word %s\n" : "    movsx rax, word %s\n", place);
    else if (t->size == 4) o(t->uns ? "    mov eax, dword %s\n" : "    movsxd rax, dword %s\n", place);
    else o("    mov rax, %s\n", place);
}

/* The arguments, computed last to first and pushed, then the first
 * six pulled into their registers -- whole numbers to rdi, rsi, ...,
 * doubles to xmm0, xmm1, ..., each kind counted on its own -- and
 * the rest left where they lie, in order, for the callee to find
 * above its frame. A variadic callee gets its doubles as bit
 * patterns in the whole-number registers, so that va_arg can read
 * them all from one place. Answers how many bytes the caller must
 * take back off the stack afterwards. */
static u32 gen_args(u32 first, u32 count, const char *const *regs, bool variadic)
{
    u32 list[NPARAMS];
    u32 n = 0;
    for (u32 a = first; a && n < NPARAMS; a = N(a)->next) list[n++] = a;
    (void)count;

    u8 kinds[NPARAMS];
    for (u32 k = n; k > 0; k--) {
        u32 a = list[k - 1];
        gen_expr(a);
        bool f = is_flt(N(a)->ty);
        if (f && (variadic || k - 1 >= REGARGS)) { o("    movq rax, xmm0\n"); f = false; }
        kinds[k - 1] = f;
        push_val(f ? C.t_double : C.t_long);
    }
    /* A variadic callee takes everything on the stack, in order, so
     * that its va_arg walks one row of arguments however many there
     * are. Everyone else takes the first six in registers. */
    if (variadic) return n * 8;

    u32 gi = 0, xi = 0;
    u32 reg_n = n < REGARGS ? n : REGARGS;
    for (u32 k = 0; k < reg_n; k++) {
        if (kinds[k]) o("    movsd %s, [rsp]\n    add rsp, 8\n", argx[xi++]);
        else o("    pop %s\n", regs[gi++]);
    }
    return n > REGARGS ? (n - REGARGS) * 8 : 0;
}

static void gen_expr(u32 i)
{
    if (!i || C.bad) return;
    node *n = N(i);
    switch (n->kind) {
    case ND_NUM:
        o("    mov rax, %d\n", n->val);
        return;
    case ND_FNUM:
        o("    mov rax, %d\n    movq xmm0, rax\n", (i64)n->fval);
        return;
    case ND_VAR:
        gen_addr(i);
        load(n->ty);
        return;
    case ND_STR:
        gen_addr(i);
        return;
    case ND_DEREF:
        gen_expr(n->lhs);
        load(n->ty);
        return;
    case ND_ADDR:
        gen_addr(n->lhs);
        return;
    case ND_MEMBER: {
        member *m = bitfield_of(i);
        gen_addr(i);
        if (m) load_bits(m); else load(n->ty);
        return;
    }
    case ND_NEG:
        gen_expr(n->lhs);
        if (is_flt(N(n->lhs)->ty)) o("    mov rax, %d\n    movq xmm1, rax\n    xorpd xmm0, xmm1\n", (i64)0x8000000000000000ULL);
        else { o("    neg rax\n"); narrow(n->ty); }
        return;
    case ND_BITNOT:
        gen_expr(n->lhs);
        o("    not rax\n");
        narrow(n->ty);
        return;
    case ND_NOT:
        gen_expr(n->lhs);
        to_bool(N(n->lhs)->ty);
        o("    test rax, rax\n    sete al\n    movzx rax, al\n");
        return;
    case ND_CAST:
        gen_expr(n->lhs);
        cast_to(N(n->lhs)->ty, n->ty);
        return;
    case ND_COMMA:
        gen_expr(n->lhs);
        gen_expr(n->rhs);
        return;
    case ND_MEMCPY: case ND_ZERO:
        gen_stmt(i);                  /* a compound literal's filling */
        return;
    case ND_LAND: {
        u32 l = C.label;
        C.label += 2;
        gen_expr(n->lhs);
        to_bool(N(n->lhs)->ty);
        o("    test rax, rax\n    je %l\n", l);
        gen_expr(n->rhs);
        to_bool(N(n->rhs)->ty);
        o("    test rax, rax\n    je %l\n    mov rax, 1\n    jmp %l\n%l:\n    mov rax, 0\n%l:\n", l, l + 1, l, l + 1);
        return;
    }
    case ND_LOR: {
        u32 l = C.label;
        C.label += 2;
        gen_expr(n->lhs);
        to_bool(N(n->lhs)->ty);
        o("    test rax, rax\n    jne %l\n", l);
        gen_expr(n->rhs);
        to_bool(N(n->rhs)->ty);
        o("    test rax, rax\n    jne %l\n    mov rax, 0\n    jmp %l\n%l:\n    mov rax, 1\n%l:\n", l, l + 1, l, l + 1);
        return;
    }
    case ND_COND: {
        u32 l = C.label;
        C.label += 2;
        gen_expr(n->lhs);
        to_bool(N(n->lhs)->ty);
        o("    test rax, rax\n    je %l\n", l);
        gen_expr(n->rhs);
        o("    jmp %l\n%l:\n", l + 1, l);
        gen_expr(n->third);
        o("%l:\n", l + 1);
        return;
    }
    case ND_ASSIGN: {
        type *t = N(n->lhs)->ty;
        member *m = bitfield_of(n->lhs);
        if (!n->op && !m && plain_var(N(n->lhs))) {
            /* a plain variable takes the value without an address
             * computed and kept around the right side */
            gen_expr(n->rhs);
            cast_to(N(n->rhs)->ty, t);
            store_plain(&C.syms[N(n->lhs)->sym], t);
            return;
        }
        if (n->op && !m && plain_var(N(n->lhs)) && !is_flt(N(n->rhs)->ty)) {
            /* the same for x op= y: the right side, then the variable
             * itself, the operation, and the variable again */
            sym *s = &C.syms[N(n->lhs)->sym];
            gen_expr(n->rhs);
            o("    mov rdi, rax\n");
            load_plain(s, t);
            if (is_ptr(t) && (n->op == ND_ADD || n->op == ND_SUB))
                o("    imul rdi, %d\n", (i64)(t->base->size ? t->base->size : 1));
            emit_binop(n->op, t);
            cast_to(t, t);
            store_plain(s, t);
            return;
        }
        gen_addr(n->lhs);
        o("    push rax\n");
        gen_expr(n->rhs);
        if (n->op) {
            if (is_flt(t)) {
                o("    movsd xmm1, xmm0\n    pop rax\n    mov r8, rax\n");
                load(t);
                emit_binop(n->op, t);
                o("    mov rdi, r8\n");
            } else {
                if (is_flt(N(n->rhs)->ty)) o("    cvttsd2si rax, xmm0\n");
                o("    mov rdi, rax\n    pop rax\n    mov r8, rax\n");
                if (m) load_bits(m); else load(t);
                if (is_ptr(t) && (n->op == ND_ADD || n->op == ND_SUB))
                    o("    imul rdi, %d\n", (i64)(t->base->size ? t->base->size : 1));
                emit_binop(n->op, t);
                o("    mov rdi, r8\n");
            }
        } else {
            o("    pop rdi\n");
        }
        if (!n->op) cast_to(N(n->rhs)->ty, t);
        else cast_to(t, t);
        if (m) store_bits(m); else store(t);
        return;
    }
    case ND_INCDEC: {
        type *t = N(n->lhs)->ty;
        member *m = bitfield_of(n->lhs);
        i64 step = n->val;
        if (is_ptr(t)) step *= (i64)(t->base->size ? t->base->size : 1);
        if (!m && plain_var(N(n->lhs))) {
            /* a plain variable counts up or down in place */
            sym *s = &C.syms[N(n->lhs)->sym];
            load_plain(s, t);
            if (n->post) o("    mov r8, rax\n");
            o("    add rax, %d\n", step);
            cast_to(t, t);
            store_plain(s, t);
            if (n->post) o("    mov rax, r8\n");
            return;
        }
        gen_addr(n->lhs);
        o("    mov rdi, rax\n");
        if (m) load_bits(m); else load(t);
        if (is_flt(t)) {
            if (n->post) o("    movsd xmm2, xmm0\n");
            o("    mov rax, %d\n    movq xmm1, rax\n    addsd xmm0, xmm1\n", (i64)sf_from_int(step));
            store(t);
            if (n->post) o("    movsd xmm0, xmm2\n");
            return;
        }
        if (n->post) o("    mov r8, rax\n");
        o("    add rax, %d\n", step);
        o("    push rdi\n");
        cast_to(t, t);
        o("    pop rdi\n");
        if (m) store_bits(m); else store(t);
        if (n->post) o("    mov rax, r8\n");
        return;
    }
    case ND_CALL: {
        type *ft = n->sym ? C.syms[n->sym].ty
                          : (N(n->lhs)->ty->kind == T_PTR ? N(n->lhs)->ty->base : N(n->lhs)->ty);
        if (n->lhs) { gen_expr(n->lhs); o("    push rax\n"); }
        u32 extra = gen_args(n->aux, (u32)n->val, arg64, ft->variadic);
        if (n->lhs) o("    mov rax, [rsp + %d]\n    call rax\n", (i64)extra);
        else o("    call %s\n", symname(C.syms[n->sym].label));
        if (extra) o("    add rsp, %d\n", (i64)extra);
        if (n->lhs) o("    add rsp, 8\n");
        if (!is_flt(n->ty) && !is_rec(n->ty)) cast_to(n->ty, n->ty);
        return;
    }
    case ND_SYSCALL: {
        static const char *const sregs[6] = { "rax", "rdi", "rsi", "rdx", "r10", "r8" };
        gen_args(n->aux, (u32)n->val, sregs, false);
        o("    syscall\n");
        return;
    }
    case ND_VASTART: {
        /* ap = the argument after the last named one, in the row the
         * caller left above the frame */
        gen_addr(n->lhs);
        o("    lea rcx, [rbp + %d]\n    mov [rax], rcx\n",
          16 + (i64)C.fn_ty->nparams * 8);
        return;
    }
    case ND_VAARG: {
        gen_addr(n->lhs);
        o("    mov rdi, rax\n    mov rax, [rdi]\n    add qword [rdi], 8\n");
        if (is_flt(n->ty)) o("    movsd xmm0, [rax]\n");
        else load(n->ty);
        return;
    }
    case ND_BYVAL: {
        /* The record's address in rax; its bytes into the caller's
         * nameless copy; the copy's address is the argument. */
        i64 off = -C.syms[n->sym].val;
        gen_expr(n->lhs);
        o("    lea rdi, [rbp - %d]\n", off);
        store(N(n->lhs)->ty);
        o("    lea rax, [rbp - %d]\n", off);
        return;
    }
    default:
        break;
    }

    /* The binary ones: left in rax/xmm0, right in rdi/xmm1. A right
     * side that is a number or a plain variable goes into rdi
     * directly; anything else is computed with the left side pushed
     * out of its way. */
    type *lt = N(n->lhs)->ty;
    node *rn = N(n->rhs);
    bool flt = is_flt(lt) || is_flt(rn->ty);
    gen_expr(n->lhs);
    if (!flt && simple_int(rn)) {
        load_into_rdi(rn);
    } else {
        push_val(lt);
        gen_expr(n->rhs);
        if (flt) o("    movsd xmm1, xmm0\n    movsd xmm0, [rsp]\n    add rsp, 8\n");
        else o("    mov rdi, rax\n    pop rax\n");
    }
    if (n->kind == ND_EQ || n->kind == ND_NE || n->kind == ND_LT || n->kind == ND_LE) {
        type *rt = N(n->rhs)->ty;
        emit_binop(n->kind, flt ? C.t_double
                            : (lt->uns || rt->uns || is_ptr(lt) || is_ptr(rt)) ? C.t_ulong : C.t_long);
    } else {
        emit_binop(n->kind, n->ty);
        narrow(n->ty);
    }
}

/* ------------------------------------------------------------------ */
/* Inline assembly, translated                                         */
/* ------------------------------------------------------------------ */

static const char *const gp64[16] = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
                                      "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" };
static const char *const gp32[16] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
                                      "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d" };
static const char *const gp16[16] = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
                                      "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w" };
static const char *const gp8[16]  = { "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
                                      "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b" };

static i32 gp_index(const char *nm)
{
    for (u32 i = 0; i < 16; i++)
        if (same(gp64[i], nm) || same(gp32[i], nm) || same(gp16[i], nm) || same(gp8[i], nm)) return (i32)i;
    return -1;
}

static const char *gp_at(i32 idx, u8 width)
{
    if (idx < 0) return "?";
    if (width == 1) return gp8[idx];
    if (width == 2) return gp16[idx];
    if (width == 4) return gp32[idx];
    return gp64[idx];
}

static asm_op *asm_find_op(asm_block *b, const char *ref, u32 len)
{
    if (len && ref[0] >= '0' && ref[0] <= '9') {
        u32 v = 0;
        for (u32 i = 0; i < len; i++) v = v * 10 + (u32)(ref[i] - '0');
        return v < b->nops ? &b->ops[v] : NULL;
    }
    for (u32 i = 0; i < b->nops; i++) {
        u32 k = 0;
        while (k < len && b->ops[i].name[k] == ref[k]) k++;
        if (k == len && !b->ops[i].name[k]) return &b->ops[i];
    }
    return NULL;
}

/* One AT&T operand into one Intel operand. Answers what kind it
 * became: 'r' register, 'm' memory, 'i' immediate, 'l' label or
 * other text. */
static char att_operand(asm_block *b, u32 bi, const char *s, u32 len, char *out, u32 max)
{
    u32 o = 0;
    #define PUT(str) do { const char *p_ = (str); while (*p_ && o < max - 1) out[o++] = *p_++; } while (0)
    #define PUTN(str, n_) do { for (u32 q_ = 0; q_ < (n_) && o < max - 1; q_++) out[o++] = (str)[q_]; } while (0)

    while (len && s[0] == ' ') { s++; len--; }
    while (len && s[len - 1] == ' ') len--;
    out[0] = 0;
    if (!len) return 0;

    /* $number or $%N: an immediate */
    if (s[0] == '$') {
        s++; len--;
        if (len && s[0] == '%') {
            asm_op *op = asm_find_op(b, s + 1, len - 1);
            if (op && op->imm) { o = 0; if (op->value < 0) { out[o++] = '-'; }
                char d[24]; u32 nd = 0; u64 v = op->value < 0 ? (u64)(-op->value) : (u64)op->value;
                if (v == 0) d[nd++] = '0'; while (v) { d[nd++] = (char)('0' + v % 10); v /= 10; }
                while (nd) out[o++] = d[--nd]; out[o] = 0; return 'i'; }
        }
        PUTN(s, len); out[o] = 0; return 'i';
    }

    /* something(%%reg) or something(%N): memory */
    i32 paren = -1;
    for (u32 i = 0; i < len; i++) if (s[i] == '(') { paren = (i32)i; break; }
    if (paren >= 0 && s[len - 1] == ')') {
        char inner[32];
        u32 il = 0;
        for (u32 i = (u32)paren + 1; i + 1 < len && il < 31; i++) inner[il++] = s[i];
        inner[il] = 0;
        char base[16] = "";
        if (inner[0] == '%' && inner[1] == '%') { u32 k = 0; while (inner[2 + k] && k < 15) { base[k] = inner[2 + k]; k++; } base[k] = 0; }
        else if (inner[0] == '%') {
            asm_op *op = asm_find_op(b, inner + 1, il - 1);
            if (op && op->reg[0]) cpy(base, op->reg);
        }
        if (same(base, "rip")) {
            /* label(%rip): the label itself, rip-relative in the assembler's own way */
            PUT("[");
            if (paren >= 1 && s[0] >= '0' && s[0] <= '9' && (s[paren - 1] == 'f' || s[paren - 1] == 'b')) {
                char lab[32]; u32 ll = 0; const char *pre = ".La"; while (pre[ll]) { lab[ll] = pre[ll]; ll++; }
                u32 v = bi; char d[12]; u32 nd = 0; if (v == 0) d[nd++] = '0'; while (v) { d[nd++] = (char)('0' + v % 10); v /= 10; }
                while (nd) lab[ll++] = d[--nd]; lab[ll++] = '_';
                for (i32 q = 0; q < paren - 1 && ll < 31; q++) lab[ll++] = s[q];
                lab[ll] = 0; PUT(lab);
            } else PUTN(s, (u32)paren);
            PUT("]"); out[o] = 0; return 'm';
        }
        PUT("[");
        PUT(base[0] ? base : "?");
        if (paren > 0) {
            if (s[0] == '-') { PUT(" - "); PUTN(s + 1, (u32)paren - 1); }
            else { PUT(" + "); PUTN(s, (u32)paren); }
        }
        PUT("]"); out[o] = 0; return 'm';
    }

    /* %%reg */
    if (len > 2 && s[0] == '%' && s[1] == '%') { PUTN(s + 2, len - 2); out[o] = 0; return 'r'; }

    /* %N, %[name], with an optional width letter: %b0 %w0 %k0 %q0 */
    if (s[0] == '%') {
        u8 force = 0;
        const char *r = s + 1;
        u32 rl = len - 1;
        if (rl > 1 && (r[0] == 'b' || r[0] == 'w' || r[0] == 'k' || r[0] == 'q' || r[0] == 'h') &&
            (r[1] == '[' || (r[1] >= '0' && r[1] <= '9'))) {
            force = r[0] == 'b' ? 1 : r[0] == 'w' ? 2 : r[0] == 'k' ? 4 : 8;
            r++; rl--;
        }
        if (r[0] == '[') { r++; rl--; if (rl && r[rl - 1] == ']') rl--; }
        asm_op *op = asm_find_op(b, r, rl);
        if (!op) { PUT("?"); out[o] = 0; return 0; }
        if (op->imm) {
            char d[24]; u32 nd = 0; u64 v = op->value < 0 ? (u64)(-op->value) : (u64)op->value;
            if (op->value < 0) out[o++] = '-';
            if (v == 0) d[nd++] = '0'; while (v) { d[nd++] = (char)('0' + v % 10); v /= 10; }
            while (nd) out[o++] = d[--nd]; out[o] = 0; return 'i';
        }
        if (op->mem) { PUT("["); PUT(op->reg); PUT("]"); out[o] = 0; return 'm'; }
        PUT(gp_at(gp_index(op->reg), force ? force : op->width));
        out[o] = 0;
        return 'r';
    }

    /* 1f, 1b: a local label of this block */
    if (len >= 2 && s[0] >= '0' && s[0] <= '9' && (s[len - 1] == 'f' || s[len - 1] == 'b')) {
        PUT(".La");
        char d[12]; u32 nd = 0; u32 v = bi; if (v == 0) d[nd++] = '0'; while (v) { d[nd++] = (char)('0' + v % 10); v /= 10; }
        while (nd) out[o++] = d[--nd];
        PUT("_");
        PUTN(s, len - 1);
        out[o] = 0;
        return 'l';
    }

    PUTN(s, len);
    out[o] = 0;
    return 'l';
    #undef PUT
    #undef PUTN
}

/* One AT&T statement into one Intel line. */
static void att_statement(asm_block *b, u32 bi, const char *s, u32 len)
{
    while (len && (s[0] == ' ' || s[0] == '\t')) { s++; len--; }
    while (len && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r')) len--;
    if (!len) return;

    /* a local label: N: */
    if (s[len - 1] == ':' && s[0] >= '0' && s[0] <= '9') {
        o(".La%d_", (i64)bi);
        for (u32 i = 0; i + 1 < len; i++) o_char(s[i]);
        o(":\n");
        return;
    }

    u32 ml = 0;
    while (ml < len && s[ml] != ' ' && s[ml] != '\t') ml++;
    char mn[24];
    u32 k = 0;
    while (k < ml && k < 23) { mn[k] = s[k]; k++; }
    mn[k] = 0;

    /* the operands, split at commas outside parentheses */
    const char *ops[4];
    u32 ol[4], nops = 0;
    u32 i = ml;
    while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;
    while (i < len && nops < 4) {
        u32 start = i;
        i32 depth = 0;
        while (i < len && !(s[i] == ',' && depth == 0)) { if (s[i] == '(') depth++; if (s[i] == ')') depth--; i++; }
        ops[nops] = s + start;
        ol[nops] = i - start;
        nops++;
        if (i < len) i++;
    }

    /* the mnemonic: AT&T's suffix off, the few renamed */
    char suffix = 0;
    static const char *const stripped[] = { "mov", "push", "pop", "lea", "add", "sub", "and", "or",
        "xor", "cmp", "test", "inc", "dec", "shl", "shr", "sar", "neg", "not", "imul", "mul",
        "div", "idiv", "xchg", "ret", "call", "jmp", NULL };
    u32 mlen = (u32)strlen(mn);
    if (same(mn, "lretq") || same(mn, "lret")) cpy(mn, "retf");
    else if (same(mn, "pushfq") || same(mn, "popfq") || same(mn, "iretq") || same(mn, "syscall") ||
             same(mn, "sysretq")) { /* as they are */ }
    else if (mn[0] == 'i' && mn[1] == 'n' && mlen == 3 && (mn[2] == 'b' || mn[2] == 'w' || mn[2] == 'l')) { suffix = mn[2]; cpy(mn, "in"); }
    else if (mn[0] == 'o' && mn[1] == 'u' && mn[2] == 't' && mlen == 4 && (mn[3] == 'b' || mn[3] == 'w' || mn[3] == 'l')) { suffix = mn[3]; cpy(mn, "out"); }
    else if (mlen > 4 && mn[0] == 'm' && mn[1] == 'o' && mn[2] == 'v' && (mn[3] == 'z' || mn[3] == 's')) {
        /* movzbl, movzwq, movsbq ...: widen without or with sign */
        suffix = mn[4];
        cpy(mn, mn[3] == 'z' ? "movzx" : "movsx");
    } else {
        for (u32 q = 0; stripped[q]; q++) {
            u32 sl = (u32)strlen(stripped[q]);
            if (mlen == sl + 1 && memcmp(mn, stripped[q], sl) == 0 &&
                (mn[sl] == 'b' || mn[sl] == 'w' || mn[sl] == 'l' || mn[sl] == 'q')) {
                suffix = mn[sl];
                mn[sl] = 0;
                break;
            }
        }
    }

    char conv[4][96];
    char kinds[4];
    for (u32 q = 0; q < nops; q++) kinds[q] = att_operand(b, bi, ops[q], ol[q], conv[q], sizeof(conv[q]));

    /* A memory operand beside an immediate, or alone, needs its width
     * said; the suffix says it. */
    const char *width = suffix == 'b' ? "byte " : suffix == 'w' ? "word " : suffix == 'l' ? "dword " : suffix == 'q' ? "qword " : "";
    bool has_reg = false;
    for (u32 q = 0; q < nops; q++) if (kinds[q] == 'r') has_reg = true;
    bool movz = same(mn, "movzx") || same(mn, "movsx");

    o("    %s", mn);
    for (u32 q = 0; q < nops; q++) {
        u32 idx = nops - 1 - q;                    /* AT&T source first; Intel destination first */
        o(q ? ", " : " ");
        if (kinds[idx] == 'm' && (!has_reg || movz) && width[0] && !same(mn, "lea"))
            o("%s", width);
        if (kinds[idx] == 'r' && movz && idx == 0 && suffix) {
            /* the narrow source register, at the suffix's width */
            i32 gi = gp_index(conv[idx]);
            o("%s", gp_at(gi, suffix == 'b' ? 1 : suffix == 'w' ? 2 : 4));
        } else o("%s", conv[idx]);
    }
    o("\n");
}

static void gen_asm(u32 bi)
{
    asm_block *b = &asms[bi];

    /* Widths from the types, numbers for the constants. */
    for (u32 k = 0; k < b->nops; k++) {
        asm_op *op = &b->ops[k];
        type *t = N(op->expr)->ty;
        op->width = (is_ptr(t) || t->kind == T_FUNC || is_rec(t)) ? 8 : (u8)(t->size > 8 ? 8 : t->size);
        if (op->imm) {
            cval c;
            if (!const_val(op->expr, &c) || c.addr) { fail_at(N(op->expr)->line, NULL, "an \"i\" operand has to be a number the compiler can work out", NULL); return; }
            op->value = c.v;
        }
        if (is_flt(t)) { fail_at(N(op->expr)->line, NULL, "floating point does not pass through inline assembly here", NULL); return; }
    }

    /* Registers: the bound ones and the named ones first, then the
     * compiler's choices from what is left. */
    bool used[16] = { false };
    for (u32 k = 0; k < b->nops; k++) {
        asm_op *op = &b->ops[k];
        if (op->imm) continue;
        node *e = N(op->expr);
        if (e->kind == ND_VAR && C.syms[e->sym].reg[0] && !op->mem) {
            cpy(op->reg, C.syms[e->sym].reg);
            i32 gi = gp_index(op->reg);
            if (gi >= 0) used[gi] = true;
            continue;
        }
        i32 fixed = -1;
        for (u32 c = 0; op->cons[c]; c++) {
            switch (op->cons[c]) {
            case 'a': fixed = 0; break;
            case 'c': fixed = 1; break;
            case 'd': fixed = 2; break;
            case 'b': fixed = 3; break;
            case 'S': fixed = 6; break;
            case 'D': fixed = 7; break;
            default: break;
            }
        }
        if (fixed >= 0 && !op->mem) { cpy(op->reg, gp64[fixed]); used[fixed] = true; }
    }
    static const i32 pool[] = { 8, 9, 10, 11, 6, 7, 1, 2, 3, 0 };
    for (u32 k = 0; k < b->nops; k++) {
        asm_op *op = &b->ops[k];
        if (op->imm || op->reg[0]) continue;
        i32 pick = -1;
        for (u32 p = 0; p < sizeof(pool) / sizeof(pool[0]); p++) if (!used[pool[p]]) { pick = pool[p]; break; }
        if (pick < 0) { fail_at(N(op->expr)->line, NULL, "inline assembly wants more registers than there are", NULL); return; }
        used[pick] = true;
        cpy(op->reg, gp64[pick]);
    }

    /* Inputs computed first, all of them, then handed to their
     * registers -- computing one must not disturb another. */
    for (u32 k = 0; k < b->nops; k++) {
        asm_op *op = &b->ops[k];
        if (op->imm) continue;
        if (op->mem) { gen_addr(op->expr); o("    push rax\n"); continue; }
        if (!op->in) continue;
        gen_expr(op->expr);
        o("    push rax\n");
    }
    for (u32 k = b->nops; k > 0; k--) {
        asm_op *op = &b->ops[k - 1];
        if (op->imm) continue;
        if (!op->mem && !op->in) continue;
        o("    pop %s\n", op->reg);
    }

    /* The template, statement by statement. */
    const u8 *t = C.spool + C.strs[b->tmpl].at;
    u32 start = 0;
    for (u32 i = 0; ; i++) {
        if (t[i] == '\n' || t[i] == ';' || t[i] == 0) {
            att_statement(b, bi, (const char *)t + start, i - start);
            if (t[i] == 0) break;
            start = i + 1;
        }
    }

    /* Outputs: every register kept on the stack before any address is
     * worked out, then stored last to first. */
    for (u32 k = 0; k < b->nops; k++) {
        asm_op *op = &b->ops[k];
        if (!op->out || op->mem || op->imm) continue;
        o("    push %s\n", op->reg);
    }
    for (u32 k = b->nops; k > 0; k--) {
        asm_op *op = &b->ops[k - 1];
        if (!op->out || op->mem || op->imm) continue;
        gen_addr(op->expr);
        o("    mov rdi, rax\n    pop rax\n");
        store(N(op->expr)->ty);
    }
}

static void gen_stmt(u32 i)
{
    if (!i || C.bad) return;
    node *n = N(i);
    switch (n->kind) {
    case ND_BLOCK:
        for (u32 s = n->aux; s; s = N(s)->next) gen_stmt(s);
        return;
    case ND_EXPR:
        gen_expr(n->lhs);
        return;
    case ND_RETURN:
        if (n->lhs) {
            gen_expr(n->lhs);
            if (is_rec(C.ret_ty)) {
                /* the record, copied to where the caller asked, and
                 * that address is the value */
                o("    mov rdi, [rbp - %d]\n", -C.ret_slot);
                store(C.ret_ty);
                o("    mov rax, rdi\n");
            }
            else cast_to(N(n->lhs)->ty, C.ret_ty);
        }
        o("    jmp .Lret%d\n", (i64)C.fn_no);
        return;
    case ND_IF: {
        u32 l = C.label;
        C.label += 2;
        gen_expr(n->lhs);
        to_bool(N(n->lhs)->ty);
        o("    test rax, rax\n    je %l\n", l);
        gen_stmt(n->rhs);
        o("    jmp %l\n%l:\n", l + 1, l);
        gen_stmt(n->third);
        o("%l:\n", l + 1);
        return;
    }
    case ND_WHILE: {
        u32 l = C.label;
        C.label += 2;
        C.brk[C.nbrk++] = l + 1;
        C.cont[C.ncont++] = l;
        o("%l:\n", l);
        gen_expr(n->lhs);
        to_bool(N(n->lhs)->ty);
        o("    test rax, rax\n    je %l\n", l + 1);
        gen_stmt(n->rhs);
        o("    jmp %l\n%l:\n", l, l + 1);
        C.nbrk--; C.ncont--;
        return;
    }
    case ND_DO: {
        u32 l = C.label;
        C.label += 3;
        C.brk[C.nbrk++] = l + 2;
        C.cont[C.ncont++] = l + 1;
        o("%l:\n", l);
        gen_stmt(n->rhs);
        o("%l:\n", l + 1);
        gen_expr(n->lhs);
        to_bool(N(n->lhs)->ty);
        o("    test rax, rax\n    jne %l\n%l:\n", l, l + 2);
        C.nbrk--; C.ncont--;
        return;
    }
    case ND_FOR: {
        u32 l = C.label;
        C.label += 3;
        C.brk[C.nbrk++] = l + 2;
        C.cont[C.ncont++] = l + 1;
        gen_stmt(n->lhs);
        o("%l:\n", l);
        if (n->rhs) { gen_expr(n->rhs); to_bool(N(n->rhs)->ty); o("    test rax, rax\n    je %l\n", l + 2); }
        gen_stmt(n->aux);
        o("%l:\n", l + 1);
        gen_expr(n->third);
        o("    jmp %l\n%l:\n", l, l + 2);
        C.nbrk--; C.ncont--;
        return;
    }
    case ND_BREAK:
        if (!C.nbrk) { fail_at(n->line, NULL, "break outside a loop or switch", NULL); return; }
        o("    jmp %l\n", C.brk[C.nbrk - 1]);
        return;
    case ND_CONTINUE:
        if (!C.ncont) { fail_at(n->line, NULL, "continue outside a loop", NULL); return; }
        o("    jmp %l\n", C.cont[C.ncont - 1]);
        return;
    case ND_GOTO:
        o("    jmp .Lg%d_%s\n", (i64)C.fn_no, C.gotos[n->aux]);
        return;
    case ND_LABEL:
        o(".Lg%d_%s:\n", (i64)C.fn_no, C.gotos[n->aux]);
        gen_stmt(n->rhs);
        return;
    case ND_ASM:
        gen_asm(n->sym);
        return;
    case ND_SWITCH: {
        u32 end = C.label++;
        C.brk[C.nbrk++] = end;
        gen_expr(n->lhs);
        u32 dflt = 0;
        for (u32 c = n->aux; c; c = N(c)->third) {
            if (N(c)->kind == ND_DEFAULT) { dflt = c; continue; }
            o("    mov rcx, %d\n    cmp rax, rcx\n    je %l\n", N(c)->val, N(c)->aux);
        }
        if (dflt) o("    jmp %l\n", N(dflt)->aux);
        else o("    jmp %l\n", end);
        gen_stmt(n->rhs);
        o("%l:\n", end);
        C.nbrk--;
        return;
    }
    case ND_CASE:
    case ND_DEFAULT:
        o("%l:\n", n->aux);
        gen_stmt(n->rhs);
        return;
    case ND_MEMCPY: {
        u32 l = C.label++;
        gen_addr(n->lhs);
        o("    mov rdi, rax\n    lea rsi, [.Li%d]\n    mov rcx, %d\n%l:\n    mov al, byte [rsi]\n    mov byte [rdi], al\n    inc rsi\n    inc rdi\n    dec rcx\n    jne %l\n",
          (i64)n->aux, (i64)n->val, l, l);
        return;
    }
    case ND_ZERO: {
        u32 l = C.label++;
        gen_addr(n->lhs);
        o("    mov rdi, rax\n    mov rcx, %d\n%l:\n    mov byte [rdi], 0\n    inc rdi\n    dec rcx\n    jne %l\n",
          (i64)n->val, l, l);
        return;
    }
    default:
        gen_expr(i);
        return;
    }
}

/* ------------------------------------------------------------------ */
/* The top: functions and globals                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    char  label[NAME_MAX];
    type *ty;
    bool  has_init;
    bool  is_extern;
    bool  is_static;                  /* this text's own: private to it */
    u32   align;                      /* asked for with aligned(n), or 0 */
    u8   *bytes;
    fixup *fixes;
    u32   nfix;
} global;

static global globals[NGLOBALS];
static u32 nglobals;
static u8 gpool[262144];
static u32 gpool_used;

static global *global_find(const char *label)
{
    for (u32 i = 0; i < nglobals; i++)
        if (same(globals[i].label, label)) return &globals[i];
    return NULL;
}

/* The values just read into ibuf become the global's own. */
static void global_fill(global *g, type *t)
{
    if (gpool_used + t->size > sizeof(gpool)) { fail("the globals are too large together", NULL); return; }
    g->ty = t;
    g->has_init = true;
    g->is_extern = false;
    g->bytes = gpool + gpool_used;
    memcpy(g->bytes, C.ibuf, t->size);
    gpool_used += t->size;
    g->nfix = C.nfix;
    g->fixes = fixups_kept(C.nfix);
    if (!g->fixes) g->nfix = 0;
}

void global_add(const char *label, type *t, bool has_init, bool is_extern, bool is_static, u32 align);

void global_add(const char *label, type *t, bool has_init, bool is_extern, bool is_static, u32 align)
{
    if (nglobals >= NGLOBALS) { fail("too many globals", NULL); return; }
    global *g = &globals[nglobals];
    memset(g, 0, sizeof(*g));
    cpy(g->label, label);
    g->ty = t;
    g->has_init = has_init;
    g->is_extern = is_extern;
    g->is_static = is_static;
    g->align = align;
    if (has_init) global_fill(g, t);
    nglobals++;
}

static void function(type *ft, const char *name, char pnames[NPARAMS][NAME_MAX], bool is_static)
{
    sym *fs = sym_find(name);
    if (fs && fs->kind == S_FUNC && fs->defined) { fail("that function already has a body", name); return; }
    if (!fs || fs->kind != S_FUNC) fs = sym_add(name, ft, S_FUNC);
    fs->ty = ft;
    fs->defined = true;

    C.nnodes = 1;
    C.frame = 0;
    C.ret_ty = ft->base;
    C.fn_ty = ft;
    C.fn_no++;
    C.nbrk = C.ncont = C.nsw = 0;
    C.ngotos = 0;
    C.va_area = 0;
    u32 fn_mark = C.nsyms;

    scope_in();
    sym *params[NPARAMS];
    /* A function returning a struct is handed the address for it as a
     * first, unnamed argument. */
    u32 hidden = 0;
    C.ret_slot = 0;
    if (is_rec(ft->base)) {
        if (ft->variadic) { fail("a variadic function returning a struct is not here", name); return; }
        type *pt = ptr_to(ft->base);
        C.ret_slot = local_slot(pt);
        hidden = 1;
    }
    for (u32 p = 0; p < ft->nparams; p++) {
        type *pt = ft->params[p];
        params[p] = sym_add(pnames[p][0] ? pnames[p] : "?", pt, S_LOCAL);
        /* A variadic function's arguments all lie above the frame, in
         * one row; anyone else's first six arrive in registers and
         * are given a home below it. A struct arrives as the address
         * of the caller's copy, and the parameter is reached through
         * it. */
        params[p]->indirect = is_rec(pt);
        if (ft->variadic) params[p]->val = 16 + (i64)p * 8;
        else if (p + hidden >= REGARGS) params[p]->val = 16 + (i64)(p + hidden - REGARGS) * 8;
        else params[p]->val = local_slot(is_rec(pt) ? ptr_to(pt) : pt);
    }
    expect("{");
    u32 body = block();
    scope_out();
    if (C.bad) return;
    add_type(body);
    if (C.bad) return;

    u32 frame = (u32)((C.frame + 15) / 16 * 16);
    u32 fn_start = C.len;
    if (is_static) o("\nprivate %s", symname(name));
    /* Functions begin on a sixteen-byte boundary, as processors like
     * their jump targets. */
    o("\nsection %s\nalign 16\n%s:\n    push rbp\n    mov rbp, rsp\n", C.text_section, symname(name));
    if (frame) o("    sub rsp, %d\n", (i64)frame);
    if (!ft->variadic) {
        u32 gi = 0, xi = 0;
        if (hidden) o("    mov [rbp - %d], %s\n", -C.ret_slot, arg64[gi++]);
        for (u32 p = 0; p < ft->nparams && p + hidden < REGARGS; p++) {
            type *pt = ft->params[p];
            i64 off = -params[p]->val;
            if (pt->kind == T_DOUBLE) o("    movsd [rbp - %d], %s\n", off, argx[xi++]);
            else if (pt->kind == T_FLOAT) o("    cvtsd2ss %s, %s\n    movss dword [rbp - %d], %s\n", argx[xi], argx[xi], off, argx[xi]), xi++;
            else if (is_rec(pt)) o("    mov [rbp - %d], %s\n", off, arg64[gi++]);     /* the copy's address */
            else if (pt->size == 1) o("    mov byte [rbp - %d], %s\n", off, arg8[gi++]);
            else if (pt->size == 2) o("    mov word [rbp - %d], %s\n", off, arg16[gi++]);
            else if (pt->size == 4) o("    mov dword [rbp - %d], %s\n", off, arg32[gi++]);
            else o("    mov [rbp - %d], %s\n", off, arg64[gi++]);
        }
    }
    gen_stmt(body);
    o(".Lret%d:\n    mov rsp, rbp\n    pop rbp\n    ret\n", (i64)C.fn_no);
    if (!C.bad && OPT_PEEP && !cc_peep_off &&
        (!cc_peep_only || name_listed(cc_peep_only, name)))
        peephole(fn_start);
    C.nsyms = fn_mark;                /* the function's names, given back at once */
    C.fn_ty = NULL;
}

static void toplevel(void)
{
    C.nnodes = 1;                     /* nothing outside a function keeps its nodes */
    last_aligned = 0;
    if (is_kw("_Static_assert")) { stmt(); return; }
    if (eat(";")) return;
    bool td, ex, st;
    type *base = declspec(&td, &ex, &st);
    if (C.bad) return;
    if (eat(";")) return;

    for (;;) {
        char nm[NAME_MAX];
        char pnames[NPARAMS][NAME_MAX];
        memset(pnames, 0, sizeof(pnames));

        type *b = base;
        for (;;) {
            if (eat("*")) { b = ptr_to(b); continue; }
            if (is_kw("const") || is_kw("volatile") || is_kw("restrict")) { advance(); continue; }
            break;
        }
        type *t;
        if (C.cur.kind == TK_IDENT && !is_typename() && nxt_is("(") && !td) {
            cpy(nm, C.cur.text);
            advance();
            advance();
            t = func_suffix(b, pnames);
        } else {
            t = declarator(b, nm);
        }
        skip_attributes();
        if (C.bad) return;

        if (td) {
            typedef_add(nm, t);
        } else if (t->kind == T_FUNC) {
            if (is("{")) { function(t, nm, pnames, st); return; }
            sym *fs = sym_find(nm);
            if (!fs || fs->kind != S_FUNC) sym_add(nm, t, S_FUNC);
        } else if (nm[0]) {
            sym *old = sym_find(nm);
            if (old && old->kind == S_GLOBAL) {
                /* declared again: fine; and with its values now, if
                 * the first time was only a declaration */
                if (eat("=")) {
                    global *g = global_find(old->label);
                    if (!g || g->has_init) { fail("that global already has its values", nm); return; }
                    memset(C.ibuf, 0, t->size > INIT_MAX ? INIT_MAX : t->size + 8);
                    C.nfix = 0;
                    C.ndyn = 0;
                    C.nnodes = 1;
                    initializer(t, 0, false);
                    if (C.bad) return;
                    if (t->kind == T_ARR && t->len == 0) { fail("an array without a size needs values", nm); return; }
                    old->ty = t;
                    global_fill(g, t);
                } else if (!ex) {
                    /* "extern int x;" in a header and "int x;" in the
                     * text: the second is the definition, without
                     * values -- c calls it tentative -- and lays the
                     * global down in the bss like any other. */
                    global *g = global_find(old->label);
                    if (g && g->is_extern) { g->is_extern = false; g->is_static = st; g->ty = t; old->ty = t; }
                }
            } else {
                sym *s = sym_add(nm, t, S_GLOBAL);
                /* extern char x[] __asm__("y"): known here as x, laid down as y */
                if (last_asm_name[0]) cpy(s->label, last_asm_name);
                memset(C.ibuf, 0, t->size > INIT_MAX ? INIT_MAX : t->size + 8);
                C.nfix = 0;
                C.ndyn = 0;
                bool has = false;
                if (eat("=")) {
                    C.nnodes = 1;
                    initializer(t, 0, false);
                    has = true;
                    if (t->kind == T_ARR && t->len == 0) { fail("an array without a size needs values", nm); return; }
                }
                if (t->kind == T_ARR && t->len == 0 && !ex) { fail("an array without a size needs values", nm); return; }
                s->ty = t;
                global_add(s->label, t, has, ex && !has, st, last_aligned);
            }
        }
        if (!eat(",")) break;
    }
    expect(";");
}

/* Bytes and addresses of one image, as db and dq lines. */
static void emit_image(const u8 *bytes, u32 size, const fixup *fixes, u32 nfix)
{
    u32 at = 0;
    while (at < size) {
        const fixup *f = NULL;
        for (u32 k = 0; k < nfix; k++) if (fixes[k].off == at) { f = &fixes[k]; break; }
        if (f) {
            o("    dq %s", f->label);
            if (f->addend) o(" + %d", f->addend);
            o("\n");
            at += 8;
            continue;
        }
        u32 run = 0;
        while (at + run < size && run < 16) {
            bool fixed = false;
            for (u32 k = 0; k < nfix; k++) if (fixes[k].off == at + run) fixed = true;
            if (fixed) break;
            run++;
        }
        o("    db");
        for (u32 k = 0; k < run; k++) o("%s %d", k ? "," : "", (i64)bytes[at + k]);
        o("\n");
        at += run;
    }
}

/* ------------------------------------------------------------------ */

i64 cc_compile(const u8 *src, u64 len, const char *src_name,
               cc_find_fn find, void *ctx,
               char *out, u64 max, char *err, u32 errmax)
{
    if (!Cp) Cp = (cstate *)lang_big_alloc(sizeof(cstate));
    if (!Cp) {
        if (errmax) { const char *m = "out of memory for the compiler's tables"; u32 k = 0; while (m[k] && k + 1 < errmax) { err[k] = m[k]; k++; } err[k] = 0; }
        return -1;
    }
    memset(&C, 0, sizeof(C));
    cpy(C.text_section, "code");
    nglobals = 0;
    gpool_used = 0;
    iconst_used = 0;
    fixpool_used = 0;
    nasm = 0;
    last_asm_name[0] = 0;
    C.find = find;
    C.ctx = ctx;
    C.out = out;
    C.max = max;
    C.err = err;
    C.errmax = errmax;
    if (errmax) err[0] = 0;
    C.nnodes = 1;
    C.label = 1;

    C.t_void   = new_type(T_VOID, 1, 1);
    C.t_char   = new_type(T_CHAR, 1, 1);
    C.t_uchar  = new_type(T_CHAR, 1, 1);  C.t_uchar->uns = true;
    C.t_bool   = new_type(T_CHAR, 1, 1);  C.t_bool->uns = true; C.t_bool->boolean = true;
    C.t_short  = new_type(T_SHORT, 2, 2);
    C.t_ushort = new_type(T_SHORT, 2, 2); C.t_ushort->uns = true;
    C.t_int    = new_type(T_INT, 4, 4);
    C.t_uint   = new_type(T_INT, 4, 4);   C.t_uint->uns = true;
    C.t_long   = new_type(T_LONG, 8, 8);
    C.t_ulong  = new_type(T_LONG, 8, 8);  C.t_ulong->uns = true;
    C.t_float  = new_type(T_FLOAT, 4, 4);
    C.t_double = new_type(T_DOUBLE, 8, 8);
    C.t_charp  = ptr_to(C.t_char);

    sym_add("", C.t_void, S_ENUM);            /* index zero means no name */
    define_text("__erebus__", "1");
    define_text("__x86_64__", "1");

    if (!push_source(src, len, src_name ? src_name : "the text", false)) return -1;
    lex_raw(&C.nxt);
    advance();

    o("; made by the compiler; the source lies beside this\n");
    o("section code\n");

    while (C.cur.kind != TK_EOF && !C.bad) toplevel();
    if (C.bad) return -1;
    if (C.ncond) { fail_at(0, src_name, "an #if never met its #endif", NULL); return -1; }

    /* A text with a main is a program: it begins at _start, which
     * calls main with what the program starts holding and ends with
     * main's answer. A text without one is a part of something larger,
     * and the linker joins it to the rest. */
    sym *m = sym_find("main");
    if (m && m->kind == S_FUNC && m->defined)
        o("\nsection code\n_start:\n    mov rbp, rsp\n    call main\n    mov rdi, rax\n    mov rax, 0\n    syscall\n");

    /* The data: what has values, then the strings and constants; the
     * room without values goes to bss, which no image carries. */
    o("\nsection data\n");
    for (u32 i = 0; i < nglobals; i++) {
        global *g = &globals[i];
        if (g->is_extern || !g->has_init) continue;
        u32 al = g->ty->align ? g->ty->align : 1;
        if (g->align > al) al = g->align;
        if (al > 4096) al = 4096;
        if (g->is_static) o("private %s\n", symname(g->label));
        o("    align %d\n", (i64)al);
        o("%s:\n", symname(g->label));
        emit_image(g->bytes, g->ty->size, g->fixes, g->nfix);
    }
    for (u32 i = 0; i < C.ninit_consts; i++) {
        o("    align 8\n.Li%d:\n", (i64)iconsts[i].no);
        emit_image(iconsts[i].bytes, iconsts[i].size, iconsts[i].fixes, iconsts[i].nfix);
    }
    for (u32 i = 0; i < C.nstrs; i++) {
        o(".Ls%d: db", (i64)i);
        for (u32 k = 0; k < C.strs[i].n; k++)
            o("%s %d", k ? "," : "", (i64)C.spool[C.strs[i].at + k]);
        o("\n");
    }
    o("\nsection bss\n");
    for (u32 i = 0; i < nglobals; i++) {
        global *g = &globals[i];
        if (g->is_extern || g->has_init) continue;
        u32 al = g->ty->align ? g->ty->align : 1;
        if (g->align > al) al = g->align;
        if (al > 4096) al = 4096;
        if (g->is_static) o("private %s\n", symname(g->label));
        o("    align %d\n", (i64)al);
        o("%s:\n    res %d\n", symname(g->label), (i64)(g->ty->size ? g->ty->size : 1));
    }
    if (C.bad) return -1;
    return (i64)C.len;
}
