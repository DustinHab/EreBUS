/*
 * asm.c -- the assembler.
 *
 * Intel order, lowercase, one instruction a line, a ';' starts a
 * remark. Labels end in a colon. Numbers are decimal, or hex with
 * 0x, or a letter in single quotes. Memory is written in brackets:
 * [rax], [rbx + 8], [rsp - 16], [name], [name + 8] for something laid
 * down by name, [gs:8] for a fixed place in a segment. When only an
 * immediate says how wide a store is, the width is said first: byte
 * [rdi], dword [rsi + 4], qword [rbp].
 *
 * Sections: "section text" (also "code") is where a text begins;
 * "section rodata", "section data", "section bss" (room without
 * values: res only) and "section user" (the kernel's ring-3
 * programs). db, dw, dd, dq lay down bytes, words, doublewords and
 * quadwords -- strings in double quotes for db, names and name + n
 * for dq -- and res n lays down n zeros.
 *
 * Names are public unless they begin with a dot or are said to be
 * "private name"; "public name" says the opposite for a text in the
 * gnu dialect, where names are private unless said. A name used and
 * never laid down is one another object lays down: the linker finds
 * it, or says that nobody does.
 *
 * What comes out is an object: each section's bytes, the names, and
 * every place that wants a name's address. Two passes: the first
 * learns where the names are, the second writes the bytes; every form
 * that names a label has one fixed length, which is what lets the
 * first pass be right about the second.
 */
#include <eb/asm.h>
#include <eb/lang.h>
#include <eb/string.h>

#define LABELS_MAX 16384              /* a compiled text names a great many places */
#define RELOCS_MAX 131072
#define NAME_MAX   64
#define TEXT_MAX   (2048 * 1024)
#define RODATA_MAX (1024 * 1024)
#define DATA_MAX   (1024 * 1024)          /* bytes that are laid down */
#define USER_MAX   (512 * 1024)
#define ZERO_MAX   (64 * 1024 * 1024)     /* room that is only counted */

enum { VIS_DEFAULT, VIS_PUBLIC, VIS_PRIVATE };

typedef struct {
    char name[NAME_MAX];
    i32  sec;                         /* where it lies, or -1 until it does */
    u64  off;
    u8   vis;
    bool defined;
    bool referenced;
} label;

typedef struct { u32 sec, off, sym, kind; i64 addend; } reloc;

typedef struct {
    u8  *buf;
    u32  cap;
    u32  len;                         /* may pass cap in data and bss: counted room */
    u32  hi_reloc;                    /* data: the end of the last place a name goes */
} section;

/* Operand kinds. */
enum { OP_NONE, OP_REG, OP_IMM, OP_MEM, OP_NAME };

typedef struct {
    u8   kind;
    u8   size;            /* register width, or the stated width of memory: 8, 16, 32, 64; 0 unsaid */
    u8   reg;             /* register number 0..15 */
    i32  base;            /* memory: base register, -1 for [name], -2 for a plain [number] */
    i32  index;           /* memory: an index register, or -1 */
    u8   scale;           /* its scale: 1, 2, 4, 8 */
    i64  disp;            /* memory displacement or a name's addend, or the immediate */
    i32  sym;             /* the name referred to, or -1 */
    u8   seg;             /* a segment prefix byte, or 0 */
} operand;

typedef struct {
    section sec[ASM_NSEC];
    u32     cur;

    label  *labels;
    u32     nlabels;
    reloc  *relocs;
    u32     nrelocs;
    bool    gnu;                      /* names private unless made public */

    u32   pass;
    u32   line;
    bool  bad;
    char *err;
    u32   errmax;
} as;

/* ------------------------------------------------------------------ */
/* Saying what went wrong                                              */
/* ------------------------------------------------------------------ */

static void fail(as *a, const char *why, const char *what)
{
    if (a->bad) return;
    a->bad = true;
    if (!a->errmax) return;
    u32 at = 0;
    const char *pre = "line ";
    while (pre[at] && at < a->errmax - 1) { a->err[at] = pre[at]; at++; }
    char d[12];
    u32 nd = 0;
    u32 v = a->line;
    if (v == 0) d[nd++] = '0';
    while (v) { d[nd++] = (char)('0' + v % 10); v /= 10; }
    while (nd && at < a->errmax - 1) a->err[at++] = d[--nd];
    if (at < a->errmax - 1) a->err[at++] = ':';
    if (at < a->errmax - 1) a->err[at++] = ' ';
    for (u32 i = 0; why[i] && at < a->errmax - 1; i++) a->err[at++] = why[i];
    if (what && what[0]) {
        if (at < a->errmax - 1) a->err[at++] = ' ';
        if (at < a->errmax - 1) a->err[at++] = '\'';
        for (u32 i = 0; what[i] && at < a->errmax - 2; i++) a->err[at++] = what[i];
        if (at < a->errmax - 1) a->err[at++] = '\'';
    }
    a->err[at] = 0;
}

/* ------------------------------------------------------------------ */
/* Bytes out                                                           */
/* ------------------------------------------------------------------ */

static u64 here(as *a)
{
    return a->sec[a->cur].len;
}

static void emit8(as *a, u8 b)
{
    section *s = &a->sec[a->cur];
    if (a->cur == ASM_SEC_BSS) { fail(a, "bss holds only room; bytes go in data", NULL); return; }
    if (s->len >= s->cap) { fail(a, "that section is too large", NULL); return; }
    s->buf[s->len++] = b;
}

/* Zeroed room. In bss it is only counted. In the data it may run
 * past the buffer: what lies beyond is never laid down, only counted,
 * and the loader gives it fresh pages -- so a program's large buffers
 * cost it nothing in the image, as long as nothing else is laid after
 * them. Elsewhere it is bytes like any other. */
static void reserve(as *a, u64 n)
{
    section *s = &a->sec[a->cur];
    if (a->cur == ASM_SEC_BSS) {
        if (s->len + n > ZERO_MAX) { fail(a, "the bss is too large", NULL); return; }
        s->len += n;
        return;
    }
    if (a->cur == ASM_SEC_DATA) {
        if (s->len + n > ZERO_MAX) { fail(a, "the data is too large", NULL); return; }
        u64 lo = s->len < s->cap ? s->len : s->cap;
        u64 hi = s->len + n < s->cap ? s->len + n : s->cap;
        if (hi > lo) memset(s->buf + lo, 0, hi - lo);
        s->len += n;
        return;
    }
    for (u64 k = 0; k < n && !a->bad; k++) emit8(a, 0);
}

static void emit32(as *a, u32 v)
{
    for (u32 i = 0; i < 4; i++) emit8(a, (u8)(v >> (i * 8)));
}

static void emit64(as *a, u64 v)
{
    for (u32 i = 0; i < 8; i++) emit8(a, (u8)(v >> (i * 8)));
}

static void emit16(as *a, u16 v)
{
    emit8(a, (u8)v);
    emit8(a, (u8)(v >> 8));
}

/* An immediate of the operation's width: a word for 16-bit
 * operations, a doubleword otherwise (sign-extended to 64). */
static void emit_imm(as *a, u8 size, i64 v)
{
    if (size == 16) emit16(a, (u16)v);
    else emit32(a, (u32)v);
}

/* ------------------------------------------------------------------ */
/* Names                                                               */
/* ------------------------------------------------------------------ */

static i32 label_lookup(as *a, const char *nm)
{
    for (u32 i = 0; i < a->nlabels; i++)
        if (strcmp(a->labels[i].name, nm) == 0) return (i32)i;
    return -1;
}

/* A name's entry, made on first sight; it is defined when a label
 * lays it down and stays undefined -- another object's -- otherwise. */
static i32 label_index(as *a, const char *nm)
{
    i32 i = label_lookup(a, nm);
    if (i >= 0) return i;
    if (a->nlabels >= LABELS_MAX) { fail(a, "too many names", NULL); return -1; }
    label *l = &a->labels[a->nlabels];
    memset(l, 0, sizeof(*l));
    u32 k = 0;
    while (nm[k] && k < NAME_MAX - 1) { l->name[k] = nm[k]; k++; }
    l->name[k] = 0;
    l->sec = -1;
    return (i32)a->nlabels++;
}

static void label_set(as *a, const char *nm)
{
    if (a->pass == 2) return;                     /* learned already */
    i32 i = label_index(a, nm);
    if (i < 0) return;
    if (a->labels[i].defined) { fail(a, "that name is used twice", nm); return; }
    a->labels[i].defined = true;
    a->labels[i].sec = (i32)a->cur;
    a->labels[i].off = here(a);
}

/* A place in the bytes that wants a name's address, written by the
 * linker. Recorded in the second pass, when the place is final. */
static void relocate(as *a, u32 kind, i32 sym, i64 addend)
{
    if (sym < 0) { fail(a, "that wanted a name", NULL); return; }
    a->labels[sym].referenced = true;
    if (a->pass != 2) return;
    if (a->nrelocs >= RELOCS_MAX) { fail(a, "too many places name others", NULL); return; }
    reloc *r = &a->relocs[a->nrelocs++];
    r->sec = a->cur;
    r->off = (u32)here(a);
    r->sym = (u32)sym;
    r->kind = kind;
    r->addend = addend;
    if (a->cur == ASM_SEC_DATA) {
        u32 end = r->off + (kind == ASM_R_ABS64 ? 8 : 4);
        if (end > a->sec[a->cur].hi_reloc) a->sec[a->cur].hi_reloc = end;
    }
}

static bool is_name_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '.';
}

/* Reads a number: decimal, 0x hex, 'c', with a leading minus. */
static bool number(const char *s, u32 n, i64 *out)
{
    if (n == 0) return false;
    bool neg = false;
    u32 i = 0;
    if (s[0] == '-') { neg = true; i = 1; }
    if (i >= n) return false;
    i64 v = 0;
    if (n - i == 3 && s[i] == '\'' && s[i + 2] == '\'') {
        v = (u8)s[i + 1];
    } else if (n - i > 2 && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
        u64 u = 0;
        for (u32 k = i + 2; k < n; k++) {
            char c = s[k];
            u32 d;
            if (c >= '0' && c <= '9') d = (u32)(c - '0');
            else if (c >= 'a' && c <= 'f') d = (u32)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') d = (u32)(c - 'A' + 10);
            else return false;
            u = (u << 4) | d;
        }
        v = (i64)u;
    } else {
        for (u32 k = i; k < n; k++) {
            if (s[k] < '0' || s[k] > '9') return false;
            v = v * 10 + (s[k] - '0');
        }
    }
    *out = neg ? -v : v;
    return true;
}

static u32 skip_sp(const char *s, u32 n, u32 i)
{
    while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
    return i;
}

/* name, name + n, name - n: the name's entry and the addend. */
static bool parse_name_expr(as *a, const char *s, u32 n, i32 *sym, i64 *addend)
{
    u32 i = skip_sp(s, n, 0);
    u32 j = i;
    while (j < n && is_name_char(s[j])) j++;
    if (j == i) return false;
    char nm[NAME_MAX];
    u32 k = 0;
    while (i + k < j && k < NAME_MAX - 1) { nm[k] = s[i + k]; k++; }
    nm[k] = 0;
    *addend = 0;
    i = skip_sp(s, n, j);
    if (i < n) {
        if (s[i] != '+' && s[i] != '-') return false;
        bool minus = s[i] == '-';
        i = skip_sp(s, n, i + 1);
        u32 e = n;
        while (e > i && (s[e - 1] == ' ' || s[e - 1] == '\t')) e--;
        i64 v;
        if (!number(s + i, e - i, &v)) return false;
        *addend = minus ? -v : v;
    }
    *sym = label_index(a, nm);
    return *sym >= 0;
}

/* ------------------------------------------------------------------ */
/* Registers                                                           */
/* ------------------------------------------------------------------ */

static const char *const regs64[16] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" };
static const char *const regs32[16] = {
    "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
    "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d" };
static const char *const regs16[16] = {
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
    "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w" };
static const char *const regs8[16] = {
    "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b" };
static const char *const regsx[16] = {
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
    "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15" };

/* Size 128 marks a vector register; it takes no part in the width
 * of an integer operation. The kernel's registers come after: the
 * control registers and the segment registers, each a kind of its
 * own that only mov knows. */
#define XMM 128
#define CREG 129
#define SREG 130

static const char *const regscr[16] = {
    "cr0", "", "cr2", "cr3", "cr4", "", "", "", "cr8", "", "", "", "", "", "", "" };
static const char *const regsseg[8] = { "es", "cs", "ss", "ds", "fs", "gs", "", "" };
static const u8 segprefix[8] = { 0x26, 0x2E, 0x36, 0x3E, 0x64, 0x65, 0, 0 };

static bool reg_named(const char *s, u32 n, u8 *num, u8 *size)
{
    for (u32 i = 0; i < 16; i++) {
        if (strlen(regs64[i]) == n && memcmp(regs64[i], s, n) == 0) { *num = (u8)i; *size = 64; return true; }
        if (strlen(regs32[i]) == n && memcmp(regs32[i], s, n) == 0) { *num = (u8)i; *size = 32; return true; }
        if (strlen(regs16[i]) == n && memcmp(regs16[i], s, n) == 0) { *num = (u8)i; *size = 16; return true; }
        if (strlen(regs8[i])  == n && memcmp(regs8[i],  s, n) == 0) { *num = (u8)i; *size = 8;  return true; }
        if (strlen(regsx[i])  == n && memcmp(regsx[i],  s, n) == 0) { *num = (u8)i; *size = XMM; return true; }
        if (regscr[i][0] && strlen(regscr[i]) == n && memcmp(regscr[i], s, n) == 0) { *num = (u8)i; *size = CREG; return true; }
    }
    for (u32 i = 0; i < 6; i++)
        if (strlen(regsseg[i]) == n && memcmp(regsseg[i], s, n) == 0) { *num = (u8)i; *size = SREG; return true; }
    return false;
}

/* ------------------------------------------------------------------ */
/* Operands                                                            */
/* ------------------------------------------------------------------ */

/* [reg], [reg + n], [reg - n], [reg + reg*4 + n], [name], [name + n],
 * [seg:n] */
static bool parse_mem(as *a, const char *s, u32 n, operand *o)
{
    o->kind = OP_MEM;
    o->base = -1;
    o->index = -1;
    o->scale = 0;
    o->disp = 0;
    o->sym = -1;

    u32 i = skip_sp(s, n, 0);

    /* a segment, then a colon */
    u32 j = i;
    while (j < n && is_name_char(s[j])) j++;
    if (j < n && s[j] == ':') {
        u8 rn, rs;
        if (!reg_named(s + i, j - i, &rn, &rs) || rs != SREG) { fail(a, "only a segment register goes before the colon", NULL); return false; }
        o->seg = segprefix[rn];
        i = skip_sp(s, n, j + 1);
        j = i;
        while (j < n && is_name_char(s[j])) j++;
    }
    if (j == i) { fail(a, "what is in the brackets?", NULL); return false; }

    u8 rn, rs;
    i64 v;
    if (reg_named(s + i, j - i, &rn, &rs)) {
        if (rs != 64) { fail(a, "only a 64-bit register can hold an address", NULL); return false; }
        o->base = rn;
    } else if (number(s + i, j - i, &v)) {
        o->base = -2;                 /* a fixed place, as in [gs:8] */
        o->disp = v;
        return true;
    } else {
        i64 addend;
        if (!parse_name_expr(a, s + i, n - i, &o->sym, &addend)) { fail(a, "i cannot read what is in the brackets", NULL); return false; }
        o->disp = addend;
        return true;
    }

    for (;;) {
        i = skip_sp(s, n, j);
        if (i >= n) return true;
        if (s[i] != '+' && s[i] != '-') { fail(a, "only + or - may follow the register", NULL); return false; }
        bool minus = s[i] == '-';
        i = skip_sp(s, n, i + 1);
        j = i;
        while (j < n && is_name_char(s[j])) j++;
        u8 xn, xs;
        if (!minus && j > i && reg_named(s + i, j - i, &xn, &xs)) {
            /* an index register, with * 1, 2, 4 or 8 after it */
            if (xs != 64 || xn == 4) { fail(a, "an index is a 64-bit register other than rsp", NULL); return false; }
            if (o->index >= 0) { fail(a, "one index register at most", NULL); return false; }
            o->index = xn;
            o->scale = 1;
            u32 k = skip_sp(s, n, j);
            if (k < n && s[k] == '*') {
                k = skip_sp(s, n, k + 1);
                u32 e = k;
                while (e < n && s[e] >= '0' && s[e] <= '9') e++;
                if (!number(s + k, e - k, &v) || !(v == 1 || v == 2 || v == 4 || v == 8)) { fail(a, "the scale is 1, 2, 4 or 8", NULL); return false; }
                o->scale = (u8)v;
                j = e;
            }
            continue;
        }
        u32 e = i;
        while (e < n && is_name_char(s[e])) e++;
        if (!number(s + i, e - i, &v)) { fail(a, "a number should follow the sign", NULL); return false; }
        o->disp += minus ? -v : v;
        j = e;
    }
}

static bool parse_operand(as *a, const char *s, u32 n, operand *o)
{
    memset(o, 0, sizeof(*o));
    o->sym = -1;
    o->index = -1;
    u32 i = skip_sp(s, n, 0);
    while (n > i && (s[n - 1] == ' ' || s[n - 1] == '\t')) n--;
    if (i >= n) { o->kind = OP_NONE; return true; }

    /* A stated width, then brackets. */
    u8 size = 0;
    if (n - i > 5 && memcmp(s + i, "byte ", 5) == 0)  { size = 8;  i = skip_sp(s, n, i + 5); }
    else if (n - i > 5 && memcmp(s + i, "word ", 5) == 0)  { size = 16; i = skip_sp(s, n, i + 5); }
    else if (n - i > 6 && memcmp(s + i, "dword ", 6) == 0) { size = 32; i = skip_sp(s, n, i + 6); }
    else if (n - i > 6 && memcmp(s + i, "qword ", 6) == 0) { size = 64; i = skip_sp(s, n, i + 6); }

    if (s[i] == '[') {
        if (s[n - 1] != ']') { fail(a, "the bracket never closes", NULL); return false; }
        if (!parse_mem(a, s + i + 1, n - i - 2, o)) return false;
        o->size = size;
        return true;
    }
    if (size) { fail(a, "a width goes before brackets", NULL); return false; }

    u8 rn, rs;
    if (reg_named(s + i, n - i, &rn, &rs)) {
        o->kind = OP_REG;
        o->reg = rn;
        o->size = rs;
        return true;
    }

    i64 v;
    if (number(s + i, n - i, &v)) {
        o->kind = OP_IMM;
        o->disp = v;
        return true;
    }

    i64 addend;
    if (is_name_char(s[i]) && parse_name_expr(a, s + i, n - i, &o->sym, &addend)) {
        o->kind = OP_NAME;
        o->disp = addend;
        return true;
    }

    char what[NAME_MAX];
    u32 k = 0;
    while (i + k < n && k < NAME_MAX - 1) { what[k] = s[i + k]; k++; }
    what[k] = 0;
    fail(a, "i cannot read", what);
    return false;
}

/* ------------------------------------------------------------------ */
/* Encoding                                                            */
/* ------------------------------------------------------------------ */

static bool fits8(i64 v)  { return v >= -128 && v <= 127; }
static bool fits32(i64 v) { return v >= -2147483648LL && v <= 2147483647LL; }

/* The REX prefix, when anything in it is set -- or when an 8-bit
 * register operand names spl, bpl, sil or dil, which without one
 * would mean ah, ch, dh, bh instead. force says so. */
static void rex(as *a, bool w, u8 reg, u8 index, u8 base, bool force)
{
    u8 r = 0x40;
    if (w) r |= 8;
    if (reg & 8) r |= 4;
    if (index & 8) r |= 2;
    if (base & 8) r |= 1;
    if (r != 0x40 || force) emit8(a, r);
}

/* Whether an 8-bit register operand is one of the four that need the
 * prefix to mean what was written. */
static bool byte_needs_rex(const operand *o, u8 size)
{
    return size == 8 && o && o->kind == OP_REG && o->reg >= 4 && o->reg <= 7;
}

static u8 base_of(const operand *m)
{
    if (m->kind != OP_MEM) return m->reg;
    return m->base < 0 ? 0 : (u8)m->base;
}

static u8 index_of(const operand *m)
{
    return m->kind == OP_MEM && m->index >= 0 ? (u8)m->index : 0;
}

/* modrm and what follows it for a memory operand, with the reg field
 * given. tail is how many immediate bytes will follow, which a
 * rip-relative displacement has to count past. */
static void mem_tail(as *a, u8 regfield, const operand *m, u32 tail)
{
    if (m->base == -1) {
        /* [name]: relative to the next instruction; the linker writes
         * the distance once it knows both ends */
        emit8(a, (u8)(0x05 | ((regfield & 7) << 3)));
        relocate(a, ASM_R_REL32, m->sym, m->disp - 4 - (i64)tail);
        emit32(a, 0);
        return;
    }
    if (m->base == -2) {
        /* [number]: a fixed displacement, no base */
        emit8(a, (u8)(0x04 | ((regfield & 7) << 3)));
        emit8(a, 0x25);
        if (!fits32(m->disp)) { fail(a, "the displacement does not fit", NULL); return; }
        emit32(a, (u32)m->disp);
        return;
    }
    u8 b = (u8)m->base;
    u8 mod;
    if (m->disp == 0 && (b & 7) != 5) mod = 0;
    else if (fits8(m->disp))          mod = 1;
    else                              mod = 2;
    if (m->index >= 0) {
        /* base + index * scale: the sib byte says so */
        u8 ss = m->scale == 8 ? 3 : m->scale == 4 ? 2 : m->scale == 2 ? 1 : 0;
        emit8(a, (u8)((mod << 6) | ((regfield & 7) << 3) | 4));
        emit8(a, (u8)((ss << 6) | (((u8)m->index & 7) << 3) | (b & 7)));
    } else {
        emit8(a, (u8)((mod << 6) | ((regfield & 7) << 3) | (b & 7)));
        if ((b & 7) == 4) emit8(a, 0x24);        /* rsp and r12 want a sib */
    }
    if (mod == 1) emit8(a, (u8)(i8)m->disp);
    if (mod == 2) {
        if (!fits32(m->disp)) { fail(a, "the displacement does not fit", NULL); return; }
        emit32(a, (u32)m->disp);
    }
}

static void modrm_rr(as *a, u8 regfield, u8 rm)
{
    emit8(a, (u8)(0xC0 | ((regfield & 7) << 3) | (rm & 7)));
}

/* reg/mem with a reg: op8 for byte operands, op for wider, the reg
 * operand in the reg field. */
static void rm_reg(as *a, u8 op8, u8 op, const operand *rm, const operand *r, u8 size)
{
    if (size == 16) emit8(a, 0x66);
    rex(a, size == 64, r->reg, index_of(rm), base_of(rm),
        byte_needs_rex(r, size) || byte_needs_rex(rm, size));
    emit8(a, size == 8 ? op8 : op);
    if (rm->kind == OP_MEM) mem_tail(a, r->reg, rm, 0);
    else modrm_rr(a, r->reg, rm->reg);
}

/* A single reg/mem operand with the opcode's own digit in the reg
 * field, and an immediate of imm_bytes after it. */
static void rm_digit(as *a, u8 op8, u8 op, u8 digit, const operand *rm, u8 size, u32 imm_bytes)
{
    if (size == 16) emit8(a, 0x66);
    rex(a, size == 64, 0, index_of(rm), base_of(rm), byte_needs_rex(rm, size));
    emit8(a, size == 8 ? op8 : op);
    if (rm->kind == OP_MEM) mem_tail(a, digit, rm, imm_bytes);
    else modrm_rr(a, digit, rm->reg);
}

/* The vector forms: a mandatory prefix, then REX, then 0F and the
 * opcode. Only the scalar double and single operations a compiler
 * needs; the x in xmm is the whole of the vector unit used here. */
static void sse_op(as *a, u8 prefix, u8 op, bool w, const operand *reg, const operand *rm)
{
    if (prefix) emit8(a, prefix);
    rex(a, w, reg->reg, index_of(rm), base_of(rm), false);
    emit8(a, 0x0F);
    emit8(a, op);
    if (rm->kind == OP_MEM) mem_tail(a, reg->reg, rm, 0);
    else modrm_rr(a, reg->reg, rm->reg);
}

static bool word_is(const char *s, u32 n, const char *w)
{
    return strlen(w) == n && memcmp(s, w, n) == 0;
}

static bool is_xmm(const operand *o) { return o->kind == OP_REG && o->size == XMM; }
static bool is_r64(const operand *o) { return o->kind == OP_REG && o->size == 64; }

/* Answers true when the mnemonic was one of the vector words. */
static bool do_sse(as *a, const char *mn, u32 ml, operand *d, operand *s)
{
    struct { const char *nm; u8 prefix, op; } two[] = {
        { "addsd", 0xF2, 0x58 }, { "subsd", 0xF2, 0x5C }, { "mulsd", 0xF2, 0x59 },
        { "divsd", 0xF2, 0x5E }, { "sqrtsd", 0xF2, 0x51 }, { "ucomisd", 0x66, 0x2E },
        { "addss", 0xF3, 0x58 }, { "subss", 0xF3, 0x5C }, { "mulss", 0xF3, 0x59 },
        { "divss", 0xF3, 0x5E }, { "ucomiss", 0x00, 0x2E },
        { "cvtss2sd", 0xF3, 0x5A }, { "cvtsd2ss", 0xF2, 0x5A },
        { "xorpd", 0x66, 0x57 }, { "xorps", 0x00, 0x57 } };
    for (u32 k = 0; k < sizeof(two) / sizeof(two[0]); k++) {
        if (!word_is(mn, ml, two[k].nm)) continue;
        if (!is_xmm(d) || !(is_xmm(s) || s->kind == OP_MEM)) { fail(a, "that wants an xmm register, then an xmm register or memory", NULL); return true; }
        sse_op(a, two[k].prefix, two[k].op, false, d, s);
        return true;
    }
    if (word_is(mn, ml, "movsd") || word_is(mn, ml, "movss")) {
        u8 p = mn[3] == 's' && mn[4] == 'd' ? 0xF2 : 0xF3;
        if (is_xmm(d) && (is_xmm(s) || s->kind == OP_MEM)) { sse_op(a, p, 0x10, false, d, s); return true; }
        if (d->kind == OP_MEM && is_xmm(s)) { sse_op(a, p, 0x11, false, s, d); return true; }
        fail(a, "movsd moves between an xmm register and memory or another xmm", NULL);
        return true;
    }
    if (word_is(mn, ml, "cvtsi2sd") || word_is(mn, ml, "cvtsi2ss")) {
        if (!is_xmm(d) || !(is_r64(s) || (s->kind == OP_MEM && s->size == 64))) { fail(a, "cvtsi2sd wants an xmm register, then a 64-bit register", NULL); return true; }
        sse_op(a, mn[7] == 'd' ? 0xF2 : 0xF3, 0x2A, true, d, s);
        return true;
    }
    if (word_is(mn, ml, "cvttsd2si") || word_is(mn, ml, "cvttss2si")) {
        if (!is_r64(d) || !(is_xmm(s) || s->kind == OP_MEM)) { fail(a, "cvttsd2si wants a 64-bit register, then an xmm register", NULL); return true; }
        sse_op(a, mn[5] == 'd' ? 0xF2 : 0xF3, 0x2C, true, d, s);
        return true;
    }
    if (word_is(mn, ml, "movq")) {
        if (is_xmm(d) && is_r64(s)) { sse_op(a, 0x66, 0x6E, true, d, s); return true; }
        if (is_r64(d) && is_xmm(s)) { sse_op(a, 0x66, 0x7E, true, s, d); return true; }
        fail(a, "movq moves between an xmm register and a 64-bit register", NULL);
        return true;
    }
    return false;
}

static u8 width_of(as *a, const operand *x, const operand *y)
{
    if (x->kind == OP_REG) return x->size;
    if (y && y->kind == OP_REG) return y->size;
    if (x->kind == OP_MEM && x->size) return x->size;
    if (y && y->kind == OP_MEM && y->size) return y->size;
    fail(a, "say byte, dword or qword", NULL);
    return 0;
}

/* ------------------------------------------------------------------ */
/* The instructions                                                    */
/* ------------------------------------------------------------------ */

/* add or and sub xor cmp: the classic group, one shape each. */
static void arith(as *a, u8 group, operand *d, operand *s)
{
    u8 size = width_of(a, d, s);
    if (!size) return;
    if (d->kind == OP_REG && s->kind == OP_REG && d->size != s->size) {
        fail(a, "the registers differ in width", NULL); return;
    }
    if (s->kind == OP_REG && (d->kind == OP_REG || d->kind == OP_MEM)) {
        rm_reg(a, (u8)(group * 8), (u8)(group * 8 + 1), d, s, size);
        return;
    }
    if (d->kind == OP_REG && s->kind == OP_MEM) {
        rm_reg(a, (u8)(group * 8 + 2), (u8)(group * 8 + 3), s, d, size);
        return;
    }
    if (s->kind == OP_NAME) {
        /* a name's address as the number: a doubleword, sign-extended */
        if (size != 32 && size != 64) { fail(a, "a name's address goes with a 32- or 64-bit operand", NULL); return; }
        rm_digit(a, 0x81, 0x81, group, d, size, 4);
        relocate(a, ASM_R_ABS32S, s->sym, s->disp);
        emit32(a, 0);
        return;
    }
    if (s->kind == OP_IMM) {
        i64 v = s->disp;
        if (size == 8) {
            if (!fits8(v) && !(v >= 0 && v <= 255)) { fail(a, "the number does not fit a byte", NULL); return; }
            rm_digit(a, 0x80, 0x80, group, d, 8, 1);
            emit8(a, (u8)v);
        } else if (fits8(v)) {
            rm_digit(a, 0x83, 0x83, group, d, size, 1);
            emit8(a, (u8)(i8)v);
        } else {
            if (!fits32(v)) { fail(a, "the number does not fit", NULL); return; }
            rm_digit(a, 0x81, 0x81, group, d, size, size == 16 ? 2 : 4);
            emit_imm(a, size, v);
        }
        return;
    }
    fail(a, "those operands do not go together", NULL);
}

static void do_mov(as *a, operand *d, operand *s)
{
    if (d->kind == OP_REG && s->kind == OP_NAME) {
        if (d->size != 64) { fail(a, "a name's address goes into a 64-bit register", NULL); return; }
        rex(a, true, 0, 0, d->reg, false);
        emit8(a, (u8)(0xB8 + (d->reg & 7)));
        relocate(a, ASM_R_ABS64, s->sym, s->disp);
        emit64(a, 0);
        return;
    }
    if (d->kind == OP_REG && s->kind == OP_IMM) {
        i64 v = s->disp;
        if (d->size == 64) {
            if (fits32(v)) {
                rex(a, true, 0, 0, d->reg, false);
                emit8(a, 0xC7);
                modrm_rr(a, 0, d->reg);
                emit32(a, (u32)v);
            } else {
                rex(a, true, 0, 0, d->reg, false);
                emit8(a, (u8)(0xB8 + (d->reg & 7)));
                emit64(a, (u64)v);
            }
        } else if (d->size == 32) {
            if (v < -2147483648LL || v > 4294967295LL) { fail(a, "the number does not fit", NULL); return; }
            rex(a, false, 0, 0, d->reg, false);
            emit8(a, (u8)(0xB8 + (d->reg & 7)));
            emit32(a, (u32)v);
        } else if (d->size == 16) {
            if (v < -32768 || v > 65535) { fail(a, "the number does not fit a word", NULL); return; }
            emit8(a, 0x66);
            rex(a, false, 0, 0, d->reg, false);
            emit8(a, (u8)(0xB8 + (d->reg & 7)));
            emit16(a, (u16)v);
        } else if (d->size == XMM) {
            fail(a, "an xmm register takes no number; movq it from a register", NULL);
            return;
        } else {
            if (v < -128 || v > 255) { fail(a, "the number does not fit a byte", NULL); return; }
            rex(a, false, 0, 0, d->reg, byte_needs_rex(d, 8));
            emit8(a, (u8)(0xB0 + (d->reg & 7)));
            emit8(a, (u8)v);
        }
        return;
    }
    if (d->kind == OP_MEM && s->kind == OP_NAME) {
        u8 size = d->size;
        if (size != 32 && size != 64) { fail(a, "a name's address is stored as a dword or qword", NULL); return; }
        rm_digit(a, 0xC7, 0xC7, 0, d, size, 4);
        relocate(a, ASM_R_ABS32S, s->sym, s->disp);
        emit32(a, 0);
        return;
    }
    if (d->kind == OP_MEM && s->kind == OP_IMM) {
        u8 size = d->size;
        if (!size) { fail(a, "say byte, dword or qword", NULL); return; }
        i64 v = s->disp;
        if (size == 8) {
            if (v < -128 || v > 255) { fail(a, "the number does not fit a byte", NULL); return; }
            rm_digit(a, 0xC6, 0xC6, 0, d, 8, 1);
            emit8(a, (u8)v);
        } else {
            if (!fits32(v)) { fail(a, "the number does not fit; load it into a register first", NULL); return; }
            rm_digit(a, 0xC7, 0xC7, 0, d, size, size == 16 ? 2 : 4);
            emit_imm(a, size, v);
        }
        return;
    }
    if (s->kind == OP_REG && s->size == XMM) { fail(a, "use movsd or movq for an xmm register", NULL); return; }

    /* The kernel's moves: a control register to or from a 64-bit
     * register, a segment register to or from a 16-bit one. */
    if (d->kind == OP_REG && d->size == 64 && s->kind == OP_REG && s->size == CREG) {
        rex(a, false, s->reg, 0, d->reg, false);
        emit8(a, 0x0F); emit8(a, 0x20);
        modrm_rr(a, s->reg, d->reg);
        return;
    }
    if (d->kind == OP_REG && d->size == CREG && s->kind == OP_REG && s->size == 64) {
        rex(a, false, d->reg, 0, s->reg, false);
        emit8(a, 0x0F); emit8(a, 0x22);
        modrm_rr(a, d->reg, s->reg);
        return;
    }
    if (d->kind == OP_REG && d->size == SREG && s->kind == OP_REG && (s->size == 16 || s->size == 32 || s->size == 64)) {
        rex(a, false, 0, 0, s->reg, false);
        emit8(a, 0x8E);
        modrm_rr(a, d->reg, s->reg);
        return;
    }
    if (d->kind == OP_REG && (d->size == 16 || d->size == 32 || d->size == 64) && s->kind == OP_REG && s->size == SREG) {
        rex(a, false, 0, 0, d->reg, false);
        emit8(a, 0x8C);
        modrm_rr(a, s->reg, d->reg);
        return;
    }
    if ((d->kind == OP_REG && (d->size == CREG || d->size == SREG)) ||
        (s->kind == OP_REG && (s->size == CREG || s->size == SREG))) {
        fail(a, "a control or segment register moves to or from a general register only", NULL);
        return;
    }
    if (s->kind == OP_REG && (d->kind == OP_REG || d->kind == OP_MEM)) {
        u8 size = s->size;
        if (d->kind == OP_REG && d->size != size) { fail(a, "the registers differ in width", NULL); return; }
        rm_reg(a, 0x88, 0x89, d, s, size);
        return;
    }
    if (d->kind == OP_REG && s->kind == OP_MEM) {
        rm_reg(a, 0x8A, 0x8B, s, d, d->size);
        return;
    }
    fail(a, "those operands do not go together", NULL);
}

/* movzx and movsx widen a byte or a word into a wider register,
 * without or with its sign. The source's width comes from the byte
 * or word register named, or from the width said before brackets. */
static void do_movzx(as *a, operand *d, operand *s, bool sign)
{
    if (d->kind != OP_REG || d->size == 8 || d->size == XMM) { fail(a, "that wants a wider register first", NULL); return; }
    u8 sw = s->size;
    if (!(sw == 8 || sw == 16) || !(s->kind == OP_REG || s->kind == OP_MEM)) {
        fail(a, "that takes a byte or a word: a byte or word register, or byte/word [..]", NULL);
        return;
    }
    rex(a, d->size == 64, d->reg, index_of(s), base_of(s), byte_needs_rex(s, sw));
    emit8(a, 0x0F);
    emit8(a, (u8)((sign ? 0xBE : 0xB6) + (sw == 16 ? 1 : 0)));
    if (s->kind == OP_MEM) mem_tail(a, d->reg, s, 0);
    else modrm_rr(a, d->reg, s->reg);
}

/* movsxd widens a doubleword with its sign: an int into a full
 * register. */
static void do_movsxd(as *a, operand *d, operand *s)
{
    if (d->kind != OP_REG || d->size != 64) { fail(a, "movsxd wants a 64-bit register first", NULL); return; }
    bool ok = (s->kind == OP_REG && s->size == 32) || (s->kind == OP_MEM && s->size == 32);
    if (!ok) { fail(a, "movsxd takes a doubleword: a 32-bit register, or dword [..]", NULL); return; }
    rex(a, true, d->reg, index_of(s), base_of(s), false);
    emit8(a, 0x63);
    if (s->kind == OP_MEM) mem_tail(a, d->reg, s, 0);
    else modrm_rr(a, d->reg, s->reg);
}

/* in and out: the accumulator and a port in dx or a small number. */
static void do_inout(as *a, bool in, operand *d, operand *s)
{
    operand *acc = in ? d : s;
    operand *port = in ? s : d;
    if (acc->kind != OP_REG || acc->reg != 0 || !(acc->size == 8 || acc->size == 16 || acc->size == 32)) {
        fail(a, in ? "in wants al, ax or eax first" : "out wants al, ax or eax last", NULL);
        return;
    }
    bool dx = port->kind == OP_REG && port->size == 16 && port->reg == 2;
    bool imm = port->kind == OP_IMM && port->disp >= 0 && port->disp <= 255;
    if (!dx && !imm) { fail(a, "the port is dx or a number up to 255", NULL); return; }
    if (acc->size == 16) emit8(a, 0x66);
    u8 op = in ? (dx ? 0xEC : 0xE4) : (dx ? 0xEE : 0xE6);
    if (acc->size != 8) op += 1;
    emit8(a, op);
    if (imm) emit8(a, (u8)port->disp);
}

/* The kernel's own instructions, most of them without operands. */
static bool do_system(as *a, const char *mn, u32 ml, operand *d, u32 nops)
{
    struct { const char *nm; u8 b[3]; u8 n; } plain[] = {
        { "hlt", { 0xF4 }, 1 }, { "cli", { 0xFA }, 1 }, { "sti", { 0xFB }, 1 },
        { "cld", { 0xFC }, 1 }, { "std", { 0xFD }, 1 },
        { "pause", { 0xF3, 0x90 }, 2 }, { "pushf", { 0x9C }, 1 }, { "popf", { 0x9D }, 1 },
        { "pushfq", { 0x9C }, 1 }, { "popfq", { 0x9D }, 1 },
        { "rdmsr", { 0x0F, 0x32 }, 2 }, { "wrmsr", { 0x0F, 0x30 }, 2 },
        { "cpuid", { 0x0F, 0xA2 }, 2 }, { "rdtsc", { 0x0F, 0x31 }, 2 },
        { "stac", { 0x0F, 0x01, 0xCB }, 3 }, { "clac", { 0x0F, 0x01, 0xCA }, 3 },
        { "swapgs", { 0x0F, 0x01, 0xF8 }, 3 },
        { "retf", { 0x48, 0xCB }, 2 },        /* a far return of quadwords: cs and rip both eight bytes */
        { "iretq", { 0x48, 0xCF }, 2 }, { "sysretq", { 0x48, 0x0F, 0x07 }, 3 },
        { "wbinvd", { 0x0F, 0x09 }, 2 }, { "lfence", { 0x0F, 0xAE, 0xE8 }, 3 },
        { "mfence", { 0x0F, 0xAE, 0xF0 }, 3 }, { "ud2", { 0x0F, 0x0B }, 2 },
        { "int3", { 0xCC }, 1 } };
    for (u32 k = 0; k < sizeof(plain) / sizeof(plain[0]); k++) {
        if (!word_is(mn, ml, plain[k].nm)) continue;
        if (nops) { fail(a, "that takes no operands", NULL); return true; }
        for (u32 i = 0; i < plain[k].n; i++) emit8(a, plain[k].b[i]);
        return true;
    }
    /* one memory operand, a digit in the reg field */
    struct { const char *nm; u8 op; u8 digit; } mem1[] = {
        { "lgdt", 0x01, 2 }, { "lidt", 0x01, 3 }, { "invlpg", 0x01, 7 },
        { "fxsave", 0xAE, 0 }, { "fxrstor", 0xAE, 1 }, { "sgdt", 0x01, 0 }, { "sidt", 0x01, 1 } };
    for (u32 k = 0; k < sizeof(mem1) / sizeof(mem1[0]); k++) {
        if (!word_is(mn, ml, mem1[k].nm)) continue;
        if (nops != 1 || d->kind != OP_MEM) { fail(a, "that wants one memory operand", NULL); return true; }
        rex(a, false, 0, index_of(d), base_of(d), false);
        emit8(a, 0x0F); emit8(a, mem1[k].op);
        mem_tail(a, mem1[k].digit, d, 0);
        return true;
    }
    if (word_is(mn, ml, "ltr")) {
        if (nops != 1 || d->kind != OP_REG || !(d->size == 16 || d->size == 32 || d->size == 64)) { fail(a, "ltr wants a register", NULL); return true; }
        rex(a, false, 0, 0, d->reg, false);
        emit8(a, 0x0F); emit8(a, 0x00);
        modrm_rr(a, 3, d->reg);
        return true;
    }
    if (word_is(mn, ml, "rdrand")) {
        if (nops != 1 || d->kind != OP_REG || !(d->size == 16 || d->size == 32 || d->size == 64)) { fail(a, "rdrand wants a register", NULL); return true; }
        if (d->size == 16) emit8(a, 0x66);
        rex(a, d->size == 64, 0, 0, d->reg, false);
        emit8(a, 0x0F); emit8(a, 0xC7);
        modrm_rr(a, 6, d->reg);
        return true;
    }
    if (word_is(mn, ml, "int")) {
        if (nops != 1 || d->kind != OP_IMM || d->disp < 0 || d->disp > 255) { fail(a, "int wants a number up to 255", NULL); return true; }
        emit8(a, 0xCD); emit8(a, (u8)d->disp);
        return true;
    }
    return false;
}

/* setcc: a byte register becomes 1 or 0 after a compare. */
static void do_setcc(as *a, u8 cc, operand *d)
{
    if (d->kind != OP_REG || d->size != 8) { fail(a, "set.. wants a byte register", NULL); return; }
    rex(a, false, 0, 0, d->reg, byte_needs_rex(d, 8));
    emit8(a, 0x0F);
    emit8(a, (u8)(0x90 + (cc - 0x80)));
    modrm_rr(a, 0, d->reg);
}

/* cmovcc: a register takes another's value after a compare. */
static void do_cmov(as *a, u8 cc, operand *d, operand *s)
{
    if (d->kind != OP_REG || !(d->size == 16 || d->size == 32 || d->size == 64) ||
        !(s->kind == OP_REG || s->kind == OP_MEM)) { fail(a, "cmov.. wants a wide register, then a register or memory", NULL); return; }
    if (d->size == 16) emit8(a, 0x66);
    rex(a, d->size == 64, d->reg, index_of(s), base_of(s), false);
    emit8(a, 0x0F);
    emit8(a, (u8)(0x40 + (cc - 0x80)));
    if (s->kind == OP_MEM) mem_tail(a, d->reg, s, 0);
    else modrm_rr(a, d->reg, s->reg);
}

static void do_lea(as *a, operand *d, operand *s)
{
    if (d->kind != OP_REG || d->size != 64 || s->kind != OP_MEM) {
        fail(a, "lea wants a 64-bit register and brackets", NULL); return;
    }
    rm_reg(a, 0x8D, 0x8D, s, d, 64);
}

static void do_test(as *a, operand *d, operand *s)
{
    u8 size = width_of(a, d, s);
    if (!size) return;
    if (s->kind == OP_REG) { rm_reg(a, 0x84, 0x85, d, s, size); return; }
    if (s->kind == OP_IMM) {
        if (size == 8) { rm_digit(a, 0xF6, 0xF6, 0, d, 8, 1); emit8(a, (u8)s->disp); }
        else { rm_digit(a, 0xF7, 0xF7, 0, d, size, size == 16 ? 2 : 4); emit_imm(a, size, s->disp); }
        return;
    }
    fail(a, "test takes a register or a number second", NULL);
}

/* inc dec neg not mul imul div idiv: one operand, one digit. */
static void unary(as *a, u8 op8, u8 op, u8 digit, operand *d)
{
    u8 size = width_of(a, d, NULL);
    if (!size) return;
    rm_digit(a, op8, op, digit, d, size, 0);
}

static void shift(as *a, u8 digit, operand *d, operand *s)
{
    u8 size = width_of(a, d, NULL);
    if (!size) return;
    if (s->kind == OP_IMM) {
        if (s->disp == 1) { rm_digit(a, 0xD0, 0xD1, digit, d, size, 0); return; }
        rm_digit(a, 0xC0, 0xC1, digit, d, size, 1);
        emit8(a, (u8)s->disp);
    } else if (s->kind == OP_REG && s->size == 8 && s->reg == 1) {
        rm_digit(a, 0xD2, 0xD3, digit, d, size, 0);
    } else {
        fail(a, "a shift count is a number or cl", NULL);
    }
}

/* shrd and shld: two registers and a count. */
static void do_shxd(as *a, bool left, operand *d, operand *s, operand *c)
{
    if (!(d->kind == OP_REG || d->kind == OP_MEM) || s->kind != OP_REG || !(s->size == 16 || s->size == 32 || s->size == 64)) {
        fail(a, "shrd wants a register or memory, a register, and a count", NULL); return;
    }
    u8 size = s->size;
    if (size == 16) emit8(a, 0x66);
    rex(a, size == 64, s->reg, index_of(d), base_of(d), false);
    emit8(a, 0x0F);
    if (c->kind == OP_IMM) {
        emit8(a, left ? 0xA4 : 0xAC);
        if (d->kind == OP_MEM) mem_tail(a, s->reg, d, 1); else modrm_rr(a, s->reg, d->reg);
        emit8(a, (u8)c->disp);
    } else if (c->kind == OP_REG && c->size == 8 && c->reg == 1) {
        emit8(a, left ? 0xA5 : 0xAD);
        if (d->kind == OP_MEM) mem_tail(a, s->reg, d, 0); else modrm_rr(a, s->reg, d->reg);
    } else fail(a, "the count is a number or cl", NULL);
}

static void jump_rel(as *a, u8 op, bool two_byte, operand *t)
{
    if (t->kind != OP_NAME) { fail(a, "jump to a name", NULL); return; }
    if (two_byte) emit8(a, 0x0F);
    emit8(a, op);
    relocate(a, ASM_R_REL32, t->sym, t->disp - 4);
    emit32(a, 0);
}

typedef struct { const char *nm; u8 cc; } jcc_name;
static const jcc_name jccs[] = {
    { "je", 0x84 }, { "jz", 0x84 }, { "jne", 0x85 }, { "jnz", 0x85 },
    { "jl", 0x8C }, { "jge", 0x8D }, { "jle", 0x8E }, { "jg", 0x8F },
    { "jb", 0x82 }, { "jae", 0x83 }, { "jbe", 0x86 }, { "ja", 0x87 },
    { "js", 0x88 }, { "jns", 0x89 }, { "jc", 0x82 }, { "jnc", 0x83 },
    { "jo", 0x80 }, { "jno", 0x81 }, { "jp", 0x8A }, { "jnp", 0x8B } };

/* ------------------------------------------------------------------ */
/* Data                                                                */
/* ------------------------------------------------------------------ */

/* db and its wider kin: numbers, names (for dq, and dd as a
 * sign-extended doubleword), and for db strings in double quotes,
 * separated by commas. */
static void lay_data(as *a, u8 width, const char *s, u32 n)
{
    u32 i = 0;
    while (i < n) {
        i = skip_sp(s, n, i);
        if (i >= n) break;
        if (s[i] == '"') {
            if (width != 1) { fail(a, "strings go with db", NULL); return; }
            u32 j = i + 1;
            while (j < n && s[j] != '"') { emit8(a, (u8)s[j]); j++; }
            if (j >= n) { fail(a, "the quote never closes", NULL); return; }
            i = j + 1;
        } else {
            u32 j = i;
            while (j < n && s[j] != ',') j++;
            u32 e = j;
            while (e > i && (s[e - 1] == ' ' || s[e - 1] == '\t')) e--;
            i64 v;
            if (number(s + i, e - i, &v)) {
                if (width == 1) emit8(a, (u8)v);
                else if (width == 2) emit16(a, (u16)v);
                else if (width == 4) emit32(a, (u32)v);
                else emit64(a, (u64)v);
            } else {
                i32 sym;
                i64 addend;
                if (!parse_name_expr(a, s + i, e - i, &sym, &addend)) { fail(a, "i cannot read that value", NULL); return; }
                if (width == 8) { relocate(a, ASM_R_ABS64, sym, addend); emit64(a, 0); }
                else if (width == 4) { relocate(a, ASM_R_ABS32S, sym, addend); emit32(a, 0); }
                else { fail(a, "a name goes with dq or dd", NULL); return; }
            }
            i = j;
        }
        i = skip_sp(s, n, i);
        if (i < n) {
            if (s[i] != ',') { fail(a, "a comma should separate the values", NULL); return; }
            i++;
        }
    }
}

/* ------------------------------------------------------------------ */
/* One line                                                            */
/* ------------------------------------------------------------------ */

/* Splits "a, b" at the comma outside brackets and quotes. */
static void split_ops(const char *s, u32 n, u32 *cut)
{
    i32 depth = 0;
    bool quoted = false;
    for (u32 i = 0; i < n; i++) {
        if (s[i] == '"') quoted = !quoted;
        if (quoted) continue;
        if (s[i] == '[') depth++;
        if (s[i] == ']') depth--;
        if (s[i] == ',' && depth == 0) { *cut = i; return; }
    }
    *cut = n;
}

static bool section_named(const char *s, u32 n, u32 *sec)
{
    if (word_is(s, n, "text") || word_is(s, n, "code")) { *sec = ASM_SEC_TEXT; return true; }
    if (word_is(s, n, "rodata")) { *sec = ASM_SEC_RODATA; return true; }
    if (word_is(s, n, "data"))   { *sec = ASM_SEC_DATA; return true; }
    if (word_is(s, n, "bss"))    { *sec = ASM_SEC_BSS; return true; }
    if (n >= 4 && memcmp(s, "user", 4) == 0 && (n == 4 || s[4] == '.')) { *sec = ASM_SEC_USER; return true; }
    return false;
}

static void assemble_line(as *a, const char *s, u32 n)
{
    /* The remark, off. */
    for (u32 i = 0; i < n; i++) {
        if (s[i] == '"') { u32 j = i + 1; while (j < n && s[j] != '"') j++; i = j; continue; }
        if (s[i] == ';') { n = i; break; }
    }
    u32 i = skip_sp(s, n, 0);
    while (n > i && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) n--;
    if (i >= n) return;

    /* A label at the front. */
    u32 j = i;
    while (j < n && is_name_char(s[j])) j++;
    if (j < n && s[j] == ':') {
        char nm[NAME_MAX];
        u32 k = 0;
        while (i + k < j && k < NAME_MAX - 1) { nm[k] = s[i + k]; k++; }
        nm[k] = 0;
        label_set(a, nm);
        i = skip_sp(s, n, j + 1);
        if (i >= n) return;
        j = i;
        while (j < n && is_name_char(s[j])) j++;
    }

    const char *mn = s + i;
    u32 ml = j - i;
    u32 rest = skip_sp(s, n, j);
    const char *ops = s + rest;
    u32 on = n - rest;

    /* Directives first. */
    if (word_is(mn, ml, "section")) {
        u32 sec;
        if (!section_named(ops, on, &sec)) { fail(a, "section text, rodata, data, bss or user", NULL); return; }
        a->cur = sec;
        return;
    }
    if (word_is(mn, ml, "public") || word_is(mn, ml, "private")) {
        char nm[NAME_MAX];
        u32 k = 0;
        while (k < on && k < NAME_MAX - 1 && is_name_char(ops[k])) { nm[k] = ops[k]; k++; }
        nm[k] = 0;
        if (!k) { fail(a, "which name?", NULL); return; }
        i32 li = label_index(a, nm);
        if (li >= 0) a->labels[li].vis = mn[1] == 'u' ? VIS_PUBLIC : VIS_PRIVATE;
        return;
    }
    if (word_is(mn, ml, "line")) {
        /* a translated text says which line of its source comes next */
        i64 v;
        if (number(ops, on, &v) && v > 0) a->line = (u32)v - 1;
        return;
    }
    if (word_is(mn, ml, "db")) { lay_data(a, 1, ops, on); return; }
    if (word_is(mn, ml, "dw")) { lay_data(a, 2, ops, on); return; }
    if (word_is(mn, ml, "dd")) { lay_data(a, 4, ops, on); return; }
    if (word_is(mn, ml, "dq")) { lay_data(a, 8, ops, on); return; }
    if (word_is(mn, ml, "res")) {
        i64 v;
        if (!number(ops, on, &v) || v < 0 || v > ZERO_MAX) { fail(a, "res wants a count", NULL); return; }
        reserve(a, (u64)v);
        return;
    }
    if (word_is(mn, ml, "align")) {
        i64 v;
        if (!number(ops, on, &v) || v <= 0 || v > 4096 || (v & (v - 1))) { fail(a, "align wants a power of two up to 4096", NULL); return; }
        u64 pad = ((u64)v - here(a) % (u64)v) % (u64)v;
        if (a->cur == ASM_SEC_TEXT || a->cur == ASM_SEC_USER) { while (pad-- && !a->bad) emit8(a, 0x90); }
        else reserve(a, pad);
        return;
    }

    /* Operands. */
    operand d, sr, third;
    u32 cut;
    split_ops(ops, on, &cut);
    if (!parse_operand(a, ops, cut, &d)) return;
    third.kind = OP_NONE;
    if (cut < on) {
        u32 cut2;
        split_ops(ops + cut + 1, on - cut - 1, &cut2);
        if (!parse_operand(a, ops + cut + 1, cut2, &sr)) return;
        if (cut2 < on - cut - 1) {
            if (!parse_operand(a, ops + cut + 1 + cut2 + 1, on - cut - 1 - cut2 - 1, &third)) return;
        }
    }
    else sr.kind = OP_NONE;
    u32 nops = (d.kind != OP_NONE) + (sr.kind != OP_NONE) + (third.kind != OP_NONE);

    /* A segment said in brackets goes ahead of everything. */
    if (d.kind == OP_MEM && d.seg) emit8(a, d.seg);
    else if (sr.kind == OP_MEM && sr.seg) emit8(a, sr.seg);

    if (word_is(mn, ml, "syscall")) { if (nops) goto count; emit8(a, 0x0F); emit8(a, 0x05); return; }
    if (do_system(a, mn, ml, &d, nops)) return;
    if (nops == 2 && word_is(mn, ml, "in"))  { do_inout(a, true, &d, &sr); return; }
    if (nops == 2 && word_is(mn, ml, "out")) { do_inout(a, false, &d, &sr); return; }
    if (word_is(mn, ml, "ret"))     { if (nops) goto count; emit8(a, 0xC3); return; }
    if (word_is(mn, ml, "nop"))     { if (nops) goto count; emit8(a, 0x90); return; }
    if (word_is(mn, ml, "cqo"))     { if (nops) goto count; emit8(a, 0x48); emit8(a, 0x99); return; }
    if (word_is(mn, ml, "cdq"))     { if (nops) goto count; emit8(a, 0x99); return; }
    if (word_is(mn, ml, "cdqe"))    { if (nops) goto count; emit8(a, 0x48); emit8(a, 0x98); return; }
    if (word_is(mn, ml, "rep") || word_is(mn, ml, "repe") || word_is(mn, ml, "repne")) {
        /* rep movsb, rep stosb, repne scasb: the prefix and the string instruction as one word after it */
        emit8(a, mn[3] == 'n' ? 0xF2 : 0xF3);
        if (word_is(ops, on, "movsb")) { emit8(a, 0xA4); return; }
        if (word_is(ops, on, "movsq")) { emit8(a, 0x48); emit8(a, 0xA5); return; }
        if (word_is(ops, on, "stosb")) { emit8(a, 0xAA); return; }
        if (word_is(ops, on, "stosq")) { emit8(a, 0x48); emit8(a, 0xAB); return; }
        if (word_is(ops, on, "scasb")) { emit8(a, 0xAE); return; }
        fail(a, "rep goes with movsb, movsq, stosb, stosq or scasb", NULL);
        return;
    }

    if (nops == 3) {
        if (word_is(mn, ml, "shrd")) { do_shxd(a, false, &d, &sr, &third); return; }
        if (word_is(mn, ml, "shld")) { do_shxd(a, true, &d, &sr, &third); return; }
        if (word_is(mn, ml, "imul") && d.kind == OP_REG && third.kind == OP_IMM) {
            /* imul r, r/m, imm */
            u8 size = d.size;
            if (size == 16) emit8(a, 0x66);
            rex(a, size == 64, d.reg, index_of(&sr), base_of(&sr), false);
            bool small = fits8(third.disp);
            emit8(a, small ? 0x6B : 0x69);
            if (sr.kind == OP_MEM) mem_tail(a, d.reg, &sr, small ? 1 : (size == 16 ? 2 : 4)); else modrm_rr(a, d.reg, sr.reg);
            if (small) emit8(a, (u8)(i8)third.disp); else emit_imm(a, size, third.disp);
            return;
        }
        goto count;
    }

    if (nops == 2) {
        if (word_is(mn, ml, "mov"))   { do_mov(a, &d, &sr); return; }
        if (word_is(mn, ml, "movzx")) { do_movzx(a, &d, &sr, false); return; }
        if (word_is(mn, ml, "movsx")) { do_movzx(a, &d, &sr, true); return; }
        if (word_is(mn, ml, "movsxd")){ do_movsxd(a, &d, &sr); return; }
        if (do_sse(a, mn, ml, &d, &sr)) return;
        if (word_is(mn, ml, "lea"))   { do_lea(a, &d, &sr); return; }
        if (word_is(mn, ml, "add"))   { arith(a, 0, &d, &sr); return; }
        if (word_is(mn, ml, "or"))    { arith(a, 1, &d, &sr); return; }
        if (word_is(mn, ml, "adc"))   { arith(a, 2, &d, &sr); return; }
        if (word_is(mn, ml, "sbb"))   { arith(a, 3, &d, &sr); return; }
        if (word_is(mn, ml, "and"))   { arith(a, 4, &d, &sr); return; }
        if (word_is(mn, ml, "sub"))   { arith(a, 5, &d, &sr); return; }
        if (word_is(mn, ml, "xor"))   { arith(a, 6, &d, &sr); return; }
        if (word_is(mn, ml, "cmp"))   { arith(a, 7, &d, &sr); return; }
        if (word_is(mn, ml, "test"))  { do_test(a, &d, &sr); return; }
        if (word_is(mn, ml, "rol"))   { shift(a, 0, &d, &sr); return; }
        if (word_is(mn, ml, "ror"))   { shift(a, 1, &d, &sr); return; }
        if (word_is(mn, ml, "shl"))   { shift(a, 4, &d, &sr); return; }
        if (word_is(mn, ml, "shr"))   { shift(a, 5, &d, &sr); return; }
        if (word_is(mn, ml, "sar"))   { shift(a, 7, &d, &sr); return; }
        if (word_is(mn, ml, "imul")) {
            if (d.kind != OP_REG || d.size == 8) { fail(a, "imul wants a wide register first", NULL); return; }
            if (sr.kind == OP_IMM) {
                /* imul r, n is imul r, r, n */
                if (d.size == 16) emit8(a, 0x66);
                rex(a, d.size == 64, d.reg, 0, d.reg, false);
                bool small = fits8(sr.disp);
                if (!small && !fits32(sr.disp)) { fail(a, "the number does not fit", NULL); return; }
                emit8(a, small ? 0x6B : 0x69);
                modrm_rr(a, d.reg, d.reg);
                if (small) emit8(a, (u8)(i8)sr.disp); else emit_imm(a, d.size, sr.disp);
                return;
            }
            if (sr.kind != OP_REG && sr.kind != OP_MEM) { fail(a, "imul takes a register, memory or a number second", NULL); return; }
            if (d.size == 16) emit8(a, 0x66);
            rex(a, d.size == 64, d.reg, index_of(&sr), base_of(&sr), false);
            emit8(a, 0x0F); emit8(a, 0xAF);
            if (sr.kind == OP_MEM) mem_tail(a, d.reg, &sr, 0);
            else modrm_rr(a, d.reg, sr.reg);
            return;
        }
        if (word_is(mn, ml, "xchg")) {
            if (d.kind != OP_REG || sr.kind != OP_REG || d.size != sr.size) { fail(a, "xchg wants two registers alike", NULL); return; }
            rm_reg(a, 0x86, 0x87, &d, &sr, d.size);
            return;
        }
        if (word_is(mn, ml, "bt") || word_is(mn, ml, "bts") || word_is(mn, ml, "btr")) {
            u8 digit = ml == 2 ? 4 : mn[2] == 's' ? 5 : 6;
            u8 size = width_of(a, &d, NULL);
            if (!size || sr.kind != OP_IMM) { fail(a, "bt wants a register or memory and a bit number", NULL); return; }
            if (size == 16) emit8(a, 0x66);
            rex(a, size == 64, 0, index_of(&d), base_of(&d), false);
            emit8(a, 0x0F); emit8(a, 0xBA);
            if (d.kind == OP_MEM) mem_tail(a, digit, &d, 1); else modrm_rr(a, digit, d.reg);
            emit8(a, (u8)sr.disp);
            return;
        }
        if (ml > 4 && mn[0] == 'c' && mn[1] == 'm' && mn[2] == 'o' && mn[3] == 'v') {
            for (u32 k = 0; k < sizeof(jccs) / sizeof(jccs[0]); k++)
                if (word_is(mn + 4, ml - 4, jccs[k].nm + 1)) { do_cmov(a, jccs[k].cc, &d, &sr); return; }
        }
    }

    if (nops == 1) {
        if (word_is(mn, ml, "inc"))  { unary(a, 0xFE, 0xFF, 0, &d); return; }
        if (word_is(mn, ml, "dec"))  { unary(a, 0xFE, 0xFF, 1, &d); return; }
        if (word_is(mn, ml, "not"))  { unary(a, 0xF6, 0xF7, 2, &d); return; }
        if (word_is(mn, ml, "neg"))  { unary(a, 0xF6, 0xF7, 3, &d); return; }
        if (word_is(mn, ml, "mul"))  { unary(a, 0xF6, 0xF7, 4, &d); return; }
        if (word_is(mn, ml, "imul")) { unary(a, 0xF6, 0xF7, 5, &d); return; }
        if (word_is(mn, ml, "div"))  { unary(a, 0xF6, 0xF7, 6, &d); return; }
        if (word_is(mn, ml, "idiv")) { unary(a, 0xF6, 0xF7, 7, &d); return; }
        if (word_is(mn, ml, "push")) {
            if (d.kind == OP_REG && d.size == 64) { rex(a, false, 0, 0, d.reg, false); emit8(a, (u8)(0x50 + (d.reg & 7))); return; }
            if (d.kind == OP_IMM && fits32(d.disp)) { emit8(a, 0x68); emit32(a, (u32)d.disp); return; }
            if (d.kind == OP_NAME) { emit8(a, 0x68); relocate(a, ASM_R_ABS32S, d.sym, d.disp); emit32(a, 0); return; }
            if (d.kind == OP_MEM) { rex(a, false, 0, index_of(&d), base_of(&d), false); emit8(a, 0xFF); mem_tail(a, 6, &d, 0); return; }
            fail(a, "push takes a 64-bit register, memory or a number", NULL); return;
        }
        if (word_is(mn, ml, "pop")) {
            if (d.kind == OP_REG && d.size == 64) { rex(a, false, 0, 0, d.reg, false); emit8(a, (u8)(0x58 + (d.reg & 7))); return; }
            if (d.kind == OP_MEM) { rex(a, false, 0, index_of(&d), base_of(&d), false); emit8(a, 0x8F); mem_tail(a, 0, &d, 0); return; }
            fail(a, "pop takes a 64-bit register or memory", NULL); return;
        }
        if (word_is(mn, ml, "call")) {
            if (d.kind == OP_NAME) { jump_rel(a, 0xE8, false, &d); return; }
            if (d.kind == OP_REG && d.size == 64) { rex(a, false, 0, 0, d.reg, false); emit8(a, 0xFF); modrm_rr(a, 2, d.reg); return; }
            if (d.kind == OP_MEM) { rex(a, false, 0, index_of(&d), base_of(&d), false); emit8(a, 0xFF); mem_tail(a, 2, &d, 0); return; }
            fail(a, "call a name, a 64-bit register or memory", NULL); return;
        }
        if (word_is(mn, ml, "jmp")) {
            if (d.kind == OP_NAME) { jump_rel(a, 0xE9, false, &d); return; }
            if (d.kind == OP_REG && d.size == 64) { rex(a, false, 0, 0, d.reg, false); emit8(a, 0xFF); modrm_rr(a, 4, d.reg); return; }
            if (d.kind == OP_MEM) { rex(a, false, 0, index_of(&d), base_of(&d), false); emit8(a, 0xFF); mem_tail(a, 4, &d, 0); return; }
            fail(a, "jmp to a name, a 64-bit register or memory", NULL); return;
        }
        for (u32 k = 0; k < sizeof(jccs) / sizeof(jccs[0]); k++)
            if (word_is(mn, ml, jccs[k].nm)) { jump_rel(a, jccs[k].cc, true, &d); return; }

        /* set.. shares the jump's condition names: sete is je's. */
        if (ml > 3 && mn[0] == 's' && mn[1] == 'e' && mn[2] == 't') {
            for (u32 k = 0; k < sizeof(jccs) / sizeof(jccs[0]); k++)
                if (word_is(mn + 3, ml - 3, jccs[k].nm + 1)) { do_setcc(a, jccs[k].cc, &d); return; }
        }
    }

count:
    {
        char what[NAME_MAX];
        u32 k = 0;
        while (k < ml && k < NAME_MAX - 1) { what[k] = mn[k]; k++; }
        what[k] = 0;
        fail(a, "i do not know", what);
    }
}

/* ------------------------------------------------------------------ */
/* The object                                                          */
/* ------------------------------------------------------------------ */

static void put32(u8 *p, u32 v) { for (u32 i = 0; i < 4; i++) p[i] = (u8)(v >> (i * 8)); }
static void put64(u8 *p, u64 v) { for (u32 i = 0; i < 8; i++) p[i] = (u8)(v >> (i * 8)); }

static bool is_public(const as *a, const label *l)
{
    if (!l->defined) return false;
    if (l->vis == VIS_PUBLIC) return true;
    if (l->vis == VIS_PRIVATE) return false;
    if (l->name[0] == '.') return false;
    return !a->gnu;
}

/* Lays the object out: head, the sections' bytes, the names, the
 * places that want names, the names' letters. */
static i64 write_object(as *a, u8 *out, u64 max)
{
    /* The data's tail of zeros does not travel -- except where a
     * place still waits for the linker's address. */
    section *ds = &a->sec[ASM_SEC_DATA];
    u32 dlaid = ds->len < ds->cap ? ds->len : ds->cap;
    while (dlaid > ds->hi_reloc && dlaid > 0 && ds->buf[dlaid - 1] == 0) dlaid--;

    u32 laid[ASM_NSEC];
    for (u32 s = 0; s < ASM_NSEC; s++) laid[s] = a->sec[s].len;
    laid[ASM_SEC_DATA] = dlaid;
    laid[ASM_SEC_BSS] = 0;

    /* Every name that is laid down or used goes out; a name that is
     * neither is nobody's business. */
    u32 nsym = 0, strn = 0;
    for (u32 i = 0; i < a->nlabels; i++) {
        label *l = &a->labels[i];
        if (!l->defined && !l->referenced) continue;
        if (!l->defined) {
            if (l->name[0] == '.') { a->line = 0; fail(a, "a label is used but never laid down:", l->name); return -1; }
            if (l->vis == VIS_PRIVATE) { a->line = 0; fail(a, "said to be private, but never laid down:", l->name); return -1; }
        }
        nsym++;
        strn += (u32)strlen(l->name) + 1;
    }

    u64 total = 76;
    for (u32 s = 0; s < ASM_NSEC; s++) total += laid[s];
    total += (u64)nsym * 24 + (u64)a->nrelocs * 24 + strn;
    if (total > max) { a->line = 0; fail(a, "the object is larger than the room for it", NULL); return -1; }

    u8 *p = out;
    p[0] = 'E'; p[1] = 'B'; p[2] = 'O'; p[3] = '1';
    put32(p + 4, nsym);
    put32(p + 8, a->nrelocs);
    put32(p + 12, strn);
    for (u32 s = 0; s < ASM_NSEC; s++) {
        put32(p + 16 + s * 4, a->sec[s].len);
        put32(p + 36 + s * 4, laid[s]);
        put32(p + 56 + s * 4, 16);
    }
    p += 76;
    for (u32 s = 0; s < ASM_NSEC; s++) {
        if (laid[s]) memcpy(p, a->sec[s].buf, laid[s]);
        p += laid[s];
    }

    /* The names, and each label's number among them, for the places. */
    static u32 *symno;
    if (!symno) symno = (u32 *)lang_big_alloc(sizeof(u32) * LABELS_MAX);
    if (!symno) { fail(a, "there is no room for the object's tables", NULL); return -1; }
    u32 no = 0, soff = 0;
    for (u32 i = 0; i < a->nlabels; i++) {
        label *l = &a->labels[i];
        if (!l->defined && !l->referenced) { symno[i] = 0xFFFFFFFF; continue; }
        symno[i] = no++;
        put32(p, soff);
        put32(p + 4, l->defined ? (u32)l->sec : 0xFFFFFFFF);
        put64(p + 8, l->off);
        put32(p + 16, (is_public(a, l) ? 1u : 0u) | (l->defined ? 2u : 0u));
        put32(p + 20, 0);
        p += 24;
        soff += (u32)strlen(l->name) + 1;
    }
    for (u32 i = 0; i < a->nrelocs; i++) {
        reloc *r = &a->relocs[i];
        put32(p, r->sec);
        put32(p + 4, r->off);
        put32(p + 8, symno[r->sym]);
        put32(p + 12, r->kind);
        put64(p + 16, (u64)r->addend);
        p += 24;
    }
    for (u32 i = 0; i < a->nlabels; i++) {
        label *l = &a->labels[i];
        if (symno[i] == 0xFFFFFFFF) continue;
        u32 k = 0;
        while (l->name[k]) p[k] = (u8)l->name[k], k++;
        p[k] = 0;
        p += k + 1;
    }
    return (i64)total;
}

/* The buffers and the tables are asked for once, on first use:
 * together they are larger than the kernel image has room to carry. */
static u8    *bufs[ASM_NSEC];
static label *label_buf;
static reloc *reloc_buf;

i64 asm_assemble_dialect(const u8 *src, u64 len, bool gnu_names,
                         u8 *out, u64 max, char *err, u32 errmax)
{
    static as a;
    memset(&a, 0, sizeof(a));
    a.err = err;
    a.errmax = errmax;
    a.gnu = gnu_names;
    if (errmax) err[0] = 0;

    static const u32 caps[ASM_NSEC] = { TEXT_MAX, RODATA_MAX, DATA_MAX, 0, USER_MAX };
    for (u32 s = 0; s < ASM_NSEC; s++) {
        if (caps[s] && !bufs[s]) bufs[s] = (u8 *)lang_big_alloc(caps[s]);
        if (caps[s] && !bufs[s]) { fail(&a, "there is no room for the assembler's tables", NULL); return -1; }
    }
    if (!label_buf) label_buf = (label *)lang_big_alloc(sizeof(label) * LABELS_MAX);
    if (!reloc_buf) reloc_buf = (reloc *)lang_big_alloc(sizeof(reloc) * RELOCS_MAX);
    if (!label_buf || !reloc_buf) { fail(&a, "there is no room for the assembler's tables", NULL); return -1; }
    a.labels = label_buf;
    a.relocs = reloc_buf;

    for (u32 pass = 1; pass <= 2 && !a.bad; pass++) {
        a.pass = pass;
        for (u32 s = 0; s < ASM_NSEC; s++) {
            a.sec[s].buf = bufs[s];
            a.sec[s].cap = caps[s];
            a.sec[s].len = 0;
            a.sec[s].hi_reloc = 0;
        }
        a.cur = ASM_SEC_TEXT;
        a.nrelocs = 0;
        a.line = 0;
        u64 start = 0;
        for (u64 i = 0; i <= len && !a.bad; i++) {
            bool end = (i == len) || src[i] == 0 || src[i] == '\n';
            if (!end) continue;
            a.line++;
            assemble_line(&a, (const char *)src + start, (u32)(i - start));
            if (i == len || src[i] == 0) break;
            start = i + 1;
        }
    }
    if (a.bad) return -1;
    return write_object(&a, out, max);
}

i64 asm_assemble(const u8 *src, u64 len, u8 *out, u64 max,
                 char *err, u32 errmax)
{
    return asm_assemble_dialect(src, len, false, out, max, err, errmax);
}

/* ------------------------------------------------------------------ */
/* The image's head                                                    */
/* ------------------------------------------------------------------ */

bool code_image_read(const u8 *d, u64 n, u32 *head, u32 *code_len,
                     u32 *data_len, u32 *zero_len, u32 *entry)
{
    if (!d || n < 16) return false;
    if (d[0] != 'E' || d[1] != 'B' || d[2] != 'X' || !(d[3] == '1' || d[3] == '2')) return false;
    u32 hd = d[3] == '2' ? 20 : 16;
    if (n < hd) return false;
    u32 cl = 0, dl = 0, zl = 0, en = 0;
    for (u32 i = 0; i < 4; i++) cl |= (u32)d[4 + i] << (i * 8);
    for (u32 i = 0; i < 4; i++) dl |= (u32)d[8 + i] << (i * 8);
    for (u32 i = 0; i < 4; i++) zl |= (u32)d[12 + i] << (i * 8);
    if (hd == 20) for (u32 i = 0; i < 4; i++) en |= (u32)d[16 + i] << (i * 8);
    if (cl == 0 || cl > TEXT_MAX + RODATA_MAX + USER_MAX * 4 || dl > DATA_MAX * 4 || zl > ZERO_MAX || (u64)dl + zl > ZERO_MAX) return false;
    if (hd + (u64)cl + dl > n) return false;
    if (en >= cl) return false;
    if (head) *head = hd;
    if (code_len) *code_len = cl;
    if (data_len) *data_len = dl;
    if (zero_len) *zero_len = zl;
    if (entry) *entry = en;
    return true;
}

bool code_image_ok(const u8 *d, u64 n, u32 *code_len, u32 *data_len,
                   u32 *zero_len)
{
    return code_image_read(d, n, NULL, code_len, data_len, zero_len, NULL);
}
