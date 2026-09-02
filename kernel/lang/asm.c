/*
 * asm.c -- the assembler.
 *
 * Intel order, lowercase, one instruction a line, a ';' starts a
 * remark. Labels end in a colon. Numbers are decimal, or hex with
 * 0x, or a letter in single quotes. Memory is written in brackets:
 * [rax], [rbx + 8], [rsp - 16], [name] for something the program laid
 * down itself. When only an immediate says how wide a store is, the
 * width is said first: byte [rdi], dword [rsi + 4], qword [rbp].
 *
 * Two sections, "section code" and "section data"; code is where a
 * text begins. db, dw, dd, dq lay down bytes, words, doublewords and
 * quadwords -- strings in double quotes for db, labels for dq --
 * and res n lays down n zeros.
 *
 * Every jump and call is relative and every reference to a name is
 * either relative to the instruction pointer or the name's absolute
 * address, so the image runs where the loader puts it and nowhere
 * else needs to be known. Two passes: the first learns where the
 * names are, the second writes the bytes; every form that names a
 * label has one fixed length, which is what lets the first pass be
 * right about the second.
 */
#include <eb/asm.h>
#include <eb/string.h>

#define LABELS_MAX 256
#define NAME_MAX   32
#define CODE_MAX   (256 * 1024)
#define DATA_MAX   (256 * 1024)

typedef struct {
    char name[NAME_MAX];
    u64  addr;
} label;

/* Operand kinds. */
enum { OP_NONE, OP_REG, OP_IMM, OP_MEM, OP_NAME };

typedef struct {
    u8   kind;
    u8   size;            /* register width, or the stated width of memory: 8, 32, 64; 0 unsaid */
    u8   reg;             /* register number 0..15 */
    i32  base;            /* memory: base register, or -1 for [name] */
    i64  disp;            /* memory displacement, or the immediate, or a name's address */
    char name[NAME_MAX];  /* for OP_NAME and [name] */
} operand;

typedef struct {
    /* the two sections */
    u8  *code; u32 code_len;
    u8  *data; u32 data_len;
    bool in_data;

    label labels[LABELS_MAX];
    u32   nlabels;

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
    return a->in_data ? USER_LOAD_DATA + a->data_len
                      : USER_LOAD_CODE + a->code_len;
}

static void emit8(as *a, u8 b)
{
    if (a->in_data) {
        if (a->data_len >= DATA_MAX) { fail(a, "the data is too large", NULL); return; }
        a->data[a->data_len++] = b;
    } else {
        if (a->code_len >= CODE_MAX) { fail(a, "the code is too large", NULL); return; }
        a->code[a->code_len++] = b;
    }
}

static void emit32(as *a, u32 v)
{
    for (u32 i = 0; i < 4; i++) emit8(a, (u8)(v >> (i * 8)));
}

static void emit64(as *a, u64 v)
{
    for (u32 i = 0; i < 8; i++) emit8(a, (u8)(v >> (i * 8)));
}

/* Patches a doubleword already emitted, for the rip-relative
 * displacements that depend on how long the instruction turns out. */
static void patch32(as *a, u64 at_addr, u32 v)
{
    u8 *buf = a->in_data ? a->data : a->code;
    u64 base = a->in_data ? USER_LOAD_DATA : USER_LOAD_CODE;
    u64 off = at_addr - base;
    for (u32 i = 0; i < 4; i++) buf[off + i] = (u8)(v >> (i * 8));
}

/* ------------------------------------------------------------------ */
/* Names and numbers                                                   */
/* ------------------------------------------------------------------ */

static bool label_find(as *a, const char *nm, u64 *addr)
{
    for (u32 i = 0; i < a->nlabels; i++)
        if (strcmp(a->labels[i].name, nm) == 0) { *addr = a->labels[i].addr; return true; }
    return false;
}

static void label_set(as *a, const char *nm, u64 addr)
{
    if (a->pass == 2) return;                     /* learned already */
    u64 old;
    if (label_find(a, nm, &old)) { fail(a, "that name is used twice", nm); return; }
    if (a->nlabels >= LABELS_MAX) { fail(a, "too many names", NULL); return; }
    u32 i = 0;
    while (nm[i] && i < NAME_MAX - 1) { a->labels[a->nlabels].name[i] = nm[i]; i++; }
    a->labels[a->nlabels].name[i] = 0;
    a->labels[a->nlabels].addr = addr;
    a->nlabels++;
}

/* A name's address: in the first pass an unknown name is simply not
 * known yet, and answers zero; in the second it is a mistake. */
static u64 name_addr(as *a, const char *nm)
{
    u64 v;
    if (label_find(a, nm, &v)) return v;
    if (a->pass == 2) fail(a, "that is not a name laid down anywhere:", nm);
    return 0;
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
        for (u32 k = i + 2; k < n; k++) {
            char c = s[k];
            u32 d;
            if (c >= '0' && c <= '9') d = (u32)(c - '0');
            else if (c >= 'a' && c <= 'f') d = (u32)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') d = (u32)(c - 'A' + 10);
            else return false;
            v = (v << 4) | d;
        }
    } else {
        for (u32 k = i; k < n; k++) {
            if (s[k] < '0' || s[k] > '9') return false;
            v = v * 10 + (s[k] - '0');
        }
    }
    *out = neg ? -v : v;
    return true;
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
static const char *const regs8[16] = {
    "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b" };

static bool reg_named(const char *s, u32 n, u8 *num, u8 *size)
{
    for (u32 i = 0; i < 16; i++) {
        if (strlen(regs64[i]) == n && memcmp(regs64[i], s, n) == 0) { *num = (u8)i; *size = 64; return true; }
        if (strlen(regs32[i]) == n && memcmp(regs32[i], s, n) == 0) { *num = (u8)i; *size = 32; return true; }
        if (strlen(regs8[i])  == n && memcmp(regs8[i],  s, n) == 0) { *num = (u8)i; *size = 8;  return true; }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* Operands                                                            */
/* ------------------------------------------------------------------ */

static u32 skip_sp(const char *s, u32 n, u32 i)
{
    while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
    return i;
}

/* [reg], [reg + n], [reg - n], [name] */
static bool parse_mem(as *a, const char *s, u32 n, operand *o)
{
    o->kind = OP_MEM;
    o->base = -1;
    o->disp = 0;
    o->name[0] = 0;

    u32 i = skip_sp(s, n, 0);
    u32 j = i;
    while (j < n && is_name_char(s[j])) j++;
    if (j == i) { fail(a, "what is in the brackets?", NULL); return false; }

    u8 rn, rs;
    if (reg_named(s + i, j - i, &rn, &rs)) {
        if (rs != 64) { fail(a, "only a 64-bit register can hold an address", NULL); return false; }
        o->base = rn;
    } else {
        u32 k = 0;
        while (i + k < j && k < NAME_MAX - 1) { o->name[k] = s[i + k]; k++; }
        o->name[k] = 0;
        o->disp = (i64)name_addr(a, o->name);
    }

    i = skip_sp(s, n, j);
    if (i >= n) return true;
    if (s[i] != '+' && s[i] != '-') { fail(a, "only + or - may follow the register", NULL); return false; }
    bool minus = s[i] == '-';
    i = skip_sp(s, n, i + 1);
    i64 v;
    if (!number(s + i, n - i, &v)) { fail(a, "a number should follow the sign", NULL); return false; }
    o->disp += minus ? -v : v;
    return true;
}

static bool parse_operand(as *a, const char *s, u32 n, operand *o)
{
    memset(o, 0, sizeof(*o));
    u32 i = skip_sp(s, n, 0);
    while (n > i && (s[n - 1] == ' ' || s[n - 1] == '\t')) n--;
    if (i >= n) { o->kind = OP_NONE; return true; }

    /* A stated width, then brackets. */
    u8 size = 0;
    if (n - i > 5 && memcmp(s + i, "byte ", 5) == 0)  { size = 8;  i = skip_sp(s, n, i + 5); }
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

    u32 j = i;
    while (j < n && is_name_char(s[j])) j++;
    if (j == n && j > i) {
        o->kind = OP_NAME;
        u32 k = 0;
        while (i + k < j && k < NAME_MAX - 1) { o->name[k] = s[i + k]; k++; }
        o->name[k] = 0;
        o->disp = (i64)name_addr(a, o->name);
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

/* modrm and what follows it for a memory operand, with the reg field
 * given. tail is how many immediate bytes will follow, which a
 * rip-relative displacement has to count past. */
static void mem_tail(as *a, u8 regfield, const operand *m, u32 tail)
{
    if (m->base < 0) {
        emit8(a, (u8)(0x05 | ((regfield & 7) << 3)));
        u64 at = here(a);
        emit32(a, 0);
        u64 next = here(a) + tail;
        patch32(a, at, (u32)((u64)m->disp - next));
        return;
    }
    u8 b = (u8)m->base;
    u8 mod;
    if (m->disp == 0 && (b & 7) != 5) mod = 0;
    else if (fits8(m->disp))          mod = 1;
    else                              mod = 2;
    emit8(a, (u8)((mod << 6) | ((regfield & 7) << 3) | (b & 7)));
    if ((b & 7) == 4) emit8(a, 0x24);            /* rsp and r12 want a sib */
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
    u8 base = rm->kind == OP_MEM ? (rm->base < 0 ? 0 : (u8)rm->base) : rm->reg;
    rex(a, size == 64, r->reg, 0, base,
        byte_needs_rex(r, size) || byte_needs_rex(rm, size));
    emit8(a, size == 8 ? op8 : op);
    if (rm->kind == OP_MEM) mem_tail(a, r->reg, rm, 0);
    else modrm_rr(a, r->reg, rm->reg);
}

/* A single reg/mem operand with the opcode's own digit in the reg
 * field, and an immediate of imm_bytes after it. */
static void rm_digit(as *a, u8 op8, u8 op, u8 digit, const operand *rm, u8 size, u32 imm_bytes)
{
    u8 base = rm->kind == OP_MEM ? (rm->base < 0 ? 0 : (u8)rm->base) : rm->reg;
    rex(a, size == 64, 0, 0, base, byte_needs_rex(rm, size));
    emit8(a, size == 8 ? op8 : op);
    if (rm->kind == OP_MEM) mem_tail(a, digit, rm, imm_bytes);
    else modrm_rr(a, digit, rm->reg);
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
    if (s->kind == OP_IMM || s->kind == OP_NAME) {
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
            rm_digit(a, 0x81, 0x81, group, d, size, 4);
            emit32(a, (u32)v);
        }
        return;
    }
    fail(a, "those operands do not go together", NULL);
}

static void do_mov(as *a, operand *d, operand *s)
{
    if (d->kind == OP_REG && (s->kind == OP_IMM || s->kind == OP_NAME)) {
        i64 v = s->disp;
        if (d->size == 64) {
            if (fits32(v) && s->kind == OP_IMM) {
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
        } else {
            if (v < -128 || v > 255) { fail(a, "the number does not fit a byte", NULL); return; }
            rex(a, false, 0, 0, d->reg, byte_needs_rex(d, 8));
            emit8(a, (u8)(0xB0 + (d->reg & 7)));
            emit8(a, (u8)v);
        }
        return;
    }
    if (d->kind == OP_MEM && (s->kind == OP_IMM || s->kind == OP_NAME)) {
        u8 size = d->size;
        if (!size) { fail(a, "say byte, dword or qword", NULL); return; }
        i64 v = s->disp;
        if (size == 8) {
            if (v < -128 || v > 255) { fail(a, "the number does not fit a byte", NULL); return; }
            rm_digit(a, 0xC6, 0xC6, 0, d, 8, 1);
            emit8(a, (u8)v);
        } else {
            if (!fits32(v)) { fail(a, "the number does not fit; load it into a register first", NULL); return; }
            rm_digit(a, 0xC7, 0xC7, 0, d, size, 4);
            emit32(a, (u32)v);
        }
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

static void do_movzx(as *a, operand *d, operand *s)
{
    if (d->kind != OP_REG || d->size == 8) { fail(a, "movzx wants a wider register first", NULL); return; }
    bool ok = (s->kind == OP_REG && s->size == 8) || (s->kind == OP_MEM && s->size == 8);
    if (!ok) { fail(a, "movzx takes a byte: a byte register, or byte [..]", NULL); return; }
    u8 base = s->kind == OP_MEM ? (s->base < 0 ? 0 : (u8)s->base) : s->reg;
    rex(a, d->size == 64, d->reg, 0, base, byte_needs_rex(s, 8));
    emit8(a, 0x0F);
    emit8(a, 0xB6);
    if (s->kind == OP_MEM) mem_tail(a, d->reg, s, 0);
    else modrm_rr(a, d->reg, s->reg);
}

/* movsx widens a byte with its sign, movsxd a doubleword: what a
 * compiler needs to load a char or an int into a full register. */
static void do_movsx(as *a, operand *d, operand *s, bool dword)
{
    if (d->kind != OP_REG || d->size != 64) {
        fail(a, dword ? "movsxd wants a 64-bit register first"
                      : "movsx wants a 64-bit register first", NULL);
        return;
    }
    u8 want = dword ? 32 : 8;
    bool ok = (s->kind == OP_REG && s->size == want) ||
              (s->kind == OP_MEM && s->size == want);
    if (!ok) {
        fail(a, dword ? "movsxd takes a doubleword: a 32-bit register, or dword [..]"
                      : "movsx takes a byte: a byte register, or byte [..]", NULL);
        return;
    }
    u8 base = s->kind == OP_MEM ? (s->base < 0 ? 0 : (u8)s->base) : s->reg;
    rex(a, true, d->reg, 0, base, byte_needs_rex(s, 8));
    if (dword) emit8(a, 0x63);
    else { emit8(a, 0x0F); emit8(a, 0xBE); }
    if (s->kind == OP_MEM) mem_tail(a, d->reg, s, 0);
    else modrm_rr(a, d->reg, s->reg);
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
        else { rm_digit(a, 0xF7, 0xF7, 0, d, size, 4); emit32(a, (u32)s->disp); }
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
        rm_digit(a, 0xC0, 0xC1, digit, d, size, 1);
        emit8(a, (u8)s->disp);
    } else if (s->kind == OP_REG && s->size == 8 && s->reg == 1) {
        rm_digit(a, 0xD2, 0xD3, digit, d, size, 0);
    } else {
        fail(a, "a shift count is a number or cl", NULL);
    }
}

static void jump_rel(as *a, u8 op, bool two_byte, operand *t)
{
    if (t->kind != OP_NAME) { fail(a, "jump to a name", NULL); return; }
    if (two_byte) emit8(a, 0x0F);
    emit8(a, op);
    u64 at = here(a);
    emit32(a, 0);
    u64 next = here(a);
    patch32(a, at, (u32)((u64)t->disp - next));
}

typedef struct { const char *nm; u8 cc; } jcc_name;
static const jcc_name jccs[] = {
    { "je", 0x84 }, { "jz", 0x84 }, { "jne", 0x85 }, { "jnz", 0x85 },
    { "jl", 0x8C }, { "jge", 0x8D }, { "jle", 0x8E }, { "jg", 0x8F },
    { "jb", 0x82 }, { "jae", 0x83 }, { "jbe", 0x86 }, { "ja", 0x87 },
    { "js", 0x88 }, { "jns", 0x89 }, { "jc", 0x82 }, { "jnc", 0x83 } };

/* ------------------------------------------------------------------ */
/* Data                                                                */
/* ------------------------------------------------------------------ */

/* db and its wider kin: numbers, names (for dq), and for db strings
 * in double quotes, separated by commas. */
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
            if (!number(s + i, e - i, &v)) {
                char nm[NAME_MAX];
                u32 k = 0;
                while (i + k < e && k < NAME_MAX - 1) { nm[k] = s[i + k]; k++; }
                nm[k] = 0;
                if (width != 8) { fail(a, "a name goes with dq", NULL); return; }
                v = (i64)name_addr(a, nm);
            }
            if (width == 1) emit8(a, (u8)v);
            else if (width == 2) { emit8(a, (u8)v); emit8(a, (u8)(v >> 8)); }
            else if (width == 4) emit32(a, (u32)v);
            else emit64(a, (u64)v);
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

static bool word_is(const char *s, u32 n, const char *w)
{
    return strlen(w) == n && memcmp(s, w, n) == 0;
}

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
        label_set(a, nm, here(a));
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
        if (word_is(ops, on, "data")) a->in_data = true;
        else if (word_is(ops, on, "code")) a->in_data = false;
        else fail(a, "section code, or section data", NULL);
        return;
    }
    if (word_is(mn, ml, "db")) { lay_data(a, 1, ops, on); return; }
    if (word_is(mn, ml, "dw")) { lay_data(a, 2, ops, on); return; }
    if (word_is(mn, ml, "dd")) { lay_data(a, 4, ops, on); return; }
    if (word_is(mn, ml, "dq")) { lay_data(a, 8, ops, on); return; }
    if (word_is(mn, ml, "res")) {
        i64 v;
        if (!number(ops, on, &v) || v < 0 || v > DATA_MAX) { fail(a, "res wants a count", NULL); return; }
        for (i64 k = 0; k < v && !a->bad; k++) emit8(a, 0);
        return;
    }
    if (word_is(mn, ml, "align")) {
        i64 v;
        if (!number(ops, on, &v) || v <= 0 || v > 4096) { fail(a, "align wants a count", NULL); return; }
        while ((here(a) % (u64)v) != 0 && !a->bad) emit8(a, a->in_data ? 0 : 0x90);
        return;
    }

    /* Operands. */
    operand d, sr;
    u32 cut;
    split_ops(ops, on, &cut);
    if (!parse_operand(a, ops, cut, &d)) return;
    if (cut < on) { if (!parse_operand(a, ops + cut + 1, on - cut - 1, &sr)) return; }
    else sr.kind = OP_NONE;
    u32 nops = (d.kind != OP_NONE) + (sr.kind != OP_NONE);

    if (word_is(mn, ml, "syscall")) { if (nops) goto count; emit8(a, 0x0F); emit8(a, 0x05); return; }
    if (word_is(mn, ml, "ret"))     { if (nops) goto count; emit8(a, 0xC3); return; }
    if (word_is(mn, ml, "nop"))     { if (nops) goto count; emit8(a, 0x90); return; }
    if (word_is(mn, ml, "cqo"))     { if (nops) goto count; emit8(a, 0x48); emit8(a, 0x99); return; }
    if (word_is(mn, ml, "cdq"))     { if (nops) goto count; emit8(a, 0x99); return; }
    if (word_is(mn, ml, "cdqe"))    { if (nops) goto count; emit8(a, 0x48); emit8(a, 0x98); return; }

    if (nops == 2) {
        if (word_is(mn, ml, "mov"))   { do_mov(a, &d, &sr); return; }
        if (word_is(mn, ml, "movzx")) { do_movzx(a, &d, &sr); return; }
        if (word_is(mn, ml, "movsx")) { do_movsx(a, &d, &sr, false); return; }
        if (word_is(mn, ml, "movsxd")){ do_movsx(a, &d, &sr, true); return; }
        if (word_is(mn, ml, "lea"))   { do_lea(a, &d, &sr); return; }
        if (word_is(mn, ml, "add"))   { arith(a, 0, &d, &sr); return; }
        if (word_is(mn, ml, "or"))    { arith(a, 1, &d, &sr); return; }
        if (word_is(mn, ml, "and"))   { arith(a, 4, &d, &sr); return; }
        if (word_is(mn, ml, "sub"))   { arith(a, 5, &d, &sr); return; }
        if (word_is(mn, ml, "xor"))   { arith(a, 6, &d, &sr); return; }
        if (word_is(mn, ml, "cmp"))   { arith(a, 7, &d, &sr); return; }
        if (word_is(mn, ml, "test"))  { do_test(a, &d, &sr); return; }
        if (word_is(mn, ml, "shl"))   { shift(a, 4, &d, &sr); return; }
        if (word_is(mn, ml, "shr"))   { shift(a, 5, &d, &sr); return; }
        if (word_is(mn, ml, "sar"))   { shift(a, 7, &d, &sr); return; }
        if (word_is(mn, ml, "imul")) {
            if (d.kind != OP_REG || d.size == 8) { fail(a, "imul wants a wide register first", NULL); return; }
            u8 base = sr.kind == OP_MEM ? (sr.base < 0 ? 0 : (u8)sr.base) : sr.reg;
            rex(a, d.size == 64, d.reg, 0, base, false);
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
            fail(a, "push takes a 64-bit register or a number", NULL); return;
        }
        if (word_is(mn, ml, "pop")) {
            if (d.kind == OP_REG && d.size == 64) { rex(a, false, 0, 0, d.reg, false); emit8(a, (u8)(0x58 + (d.reg & 7))); return; }
            fail(a, "pop takes a 64-bit register", NULL); return;
        }
        if (word_is(mn, ml, "call")) {
            if (d.kind == OP_NAME) { jump_rel(a, 0xE8, false, &d); return; }
            if (d.kind == OP_REG && d.size == 64) { rex(a, false, 0, 0, d.reg, false); emit8(a, 0xFF); modrm_rr(a, 2, d.reg); return; }
            fail(a, "call a name or a 64-bit register", NULL); return;
        }
        if (word_is(mn, ml, "jmp")) {
            if (d.kind == OP_NAME) { jump_rel(a, 0xE9, false, &d); return; }
            if (d.kind == OP_REG && d.size == 64) { rex(a, false, 0, 0, d.reg, false); emit8(a, 0xFF); modrm_rr(a, 4, d.reg); return; }
            fail(a, "jmp to a name or a 64-bit register", NULL); return;
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

static u8 code_buf[CODE_MAX];
static u8 data_buf[DATA_MAX];

i64 asm_assemble(const u8 *src, u64 len, u8 *out, u64 max,
                 char *err, u32 errmax)
{
    static as a;
    memset(&a, 0, sizeof(a));
    a.code = code_buf;
    a.data = data_buf;
    a.err = err;
    a.errmax = errmax;
    if (errmax) err[0] = 0;

    for (u32 pass = 1; pass <= 2 && !a.bad; pass++) {
        a.pass = pass;
        a.code_len = 0;
        a.data_len = 0;
        a.in_data = false;
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
    if (a.code_len == 0) { fail(&a, "there is no code in it", NULL); return -1; }

    u64 total = 16 + a.code_len + a.data_len;
    if (total > max) { fail(&a, "the image is larger than the room for it", NULL); return -1; }

    out[0] = 'E'; out[1] = 'B'; out[2] = 'X'; out[3] = '1';
    for (u32 i = 0; i < 4; i++) out[4 + i] = (u8)(a.code_len >> (i * 8));
    for (u32 i = 0; i < 4; i++) out[8 + i] = (u8)(a.data_len >> (i * 8));
    for (u32 i = 0; i < 4; i++) out[12 + i] = 0;
    memcpy(out + 16, a.code, a.code_len);
    memcpy(out + 16 + a.code_len, a.data, a.data_len);
    return (i64)total;
}

bool code_image_ok(const u8 *d, u64 n, u32 *code_len, u32 *data_len,
                   u32 *zero_len)
{
    if (!d || n < 16) return false;
    if (d[0] != 'E' || d[1] != 'B' || d[2] != 'X' || d[3] != '1') return false;
    u32 cl = 0, dl = 0, zl = 0;
    for (u32 i = 0; i < 4; i++) cl |= (u32)d[4 + i] << (i * 8);
    for (u32 i = 0; i < 4; i++) dl |= (u32)d[8 + i] << (i * 8);
    for (u32 i = 0; i < 4; i++) zl |= (u32)d[12 + i] << (i * 8);
    if (cl == 0 || cl > CODE_MAX || dl > DATA_MAX || zl > DATA_MAX) return false;
    if (16 + (u64)cl + dl > n) return false;
    if (code_len) *code_len = cl;
    if (data_len) *data_len = dl;
    if (zero_len) *zero_len = zl;
    return true;
}
