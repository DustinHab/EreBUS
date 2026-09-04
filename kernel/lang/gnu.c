/*
 * gnu.c -- AT&T/GNU assembly translated line by line into asm.c's dialect.
 * - handles %reg, $imm, size suffixes, disp(%base), .set .rept .if .align .skip .quad, sections, .globl
 * - .type/.size dropped; numbered labels (1:, 1f, 2b) resolved after the whole text is seen
 */
#include <eb/asm.h>
#include <eb/lang.h>
#include <eb/string.h>

#define TEXT_MAX   (2u * 1024 * 1024)
#define LINES_MAX  65536
#define VARS_MAX   64
#define NUMLAB_MAX 4096
#define WORD_MAX   96

typedef struct { char name[32]; i64 value; } var;
typedef struct { u32 num; u32 seq; u32 at; } numlab;   /* at: offset into mid where it was laid */

typedef struct {
    char *src;                        /* a working copy, comments blanked */
    u64   len;
    u32  *line_at;                    /* offsets of the lines */
    u32  *line_no;                    /* their numbers in the source */
    u32   nlines;

    char *mid;                        /* the translation, numbered labels pending */
    u32   mid_len;
    u32   last_line;                  /* the source line last announced */

    var     vars[VARS_MAX];
    u32     nvars;
    numlab *numlabs;
    u32     nnumlabs;
    u32     seq;

    bool  skipping;                   /* inside a section that does not travel */
    u32   line;                       /* the source line being read */
    bool  bad;
    char *err;
    u32   errmax;
} tr;

static void fail(tr *t, const char *why, const char *what)
{
    if (t->bad) return;
    t->bad = true;
    if (!t->errmax) return;
    u32 at = 0;
    const char *pre = "line ";
    while (pre[at] && at < t->errmax - 1) { t->err[at] = pre[at]; at++; }
    char d[12]; u32 nd = 0; u32 v = t->line;
    if (v == 0) d[nd++] = '0';
    while (v) { d[nd++] = (char)('0' + v % 10); v /= 10; }
    while (nd && at < t->errmax - 1) t->err[at++] = d[--nd];
    const char *mid = ": ";
    while (*mid && at < t->errmax - 1) t->err[at++] = *mid++;
    while (*why && at < t->errmax - 1) t->err[at++] = *why++;
    if (what && what[0]) {
        if (at < t->errmax - 1) t->err[at++] = ' ';
        if (at < t->errmax - 1) t->err[at++] = '\'';
        while (*what && at < t->errmax - 2) t->err[at++] = *what++;
        if (at < t->errmax - 1) t->err[at++] = '\'';
    }
    t->err[at] = 0;
}

/* ------------------------------------------------------------------ */
/* Writing the translation                                             */
/* ------------------------------------------------------------------ */

static void put(tr *t, const char *s)
{
    while (*s) {
        if (t->mid_len + 1 >= TEXT_MAX) { fail(t, "the translation is too large", NULL); return; }
        t->mid[t->mid_len++] = *s++;
    }
}

static void put_n(tr *t, const char *s, u32 n)
{
    for (u32 i = 0; i < n; i++) {
        if (t->mid_len + 1 >= TEXT_MAX) { fail(t, "the translation is too large", NULL); return; }
        t->mid[t->mid_len++] = s[i];
    }
}

static void put_num(tr *t, i64 v)
{
    char d[24]; u32 nd = 0;
    u64 u = v < 0 ? (u64)0 - (u64)v : (u64)v;   /* the most negative has no positive twin */
    if (v < 0) put(t, "-");
    if (u == 0) d[nd++] = '0';
    while (u) { d[nd++] = (char)('0' + u % 10); u /= 10; }
    while (nd) { char c[2] = { d[--nd], 0 }; put(t, c); }
}

/* Every line the assembler will read says which source line it came
 * from, when that changed. */
static void begin_line(tr *t)
{
    if (t->line != t->last_line) {
        put(t, "line ");
        put_num(t, (i64)t->line);
        put(t, "\n");
        t->last_line = t->line;
    }
}

/* ------------------------------------------------------------------ */
/* Small readers                                                       */
/* ------------------------------------------------------------------ */

static bool is_ident_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '$';
}

static u32 skip_sp(const char *s, u32 n, u32 i)
{
    while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
    return i;
}

static bool same_n(const char *s, u32 n, const char *w)
{
    return strlen(w) == n && memcmp(s, w, n) == 0;
}

static void cpy(char *d, const char *s)
{
    while (*s) *d++ = *s++;
    *d = 0;
}

static var *var_find(tr *t, const char *nm, u32 n)
{
    for (u32 i = 0; i < t->nvars; i++)
        if (strlen(t->vars[i].name) == n && memcmp(t->vars[i].name, nm, n) == 0) return &t->vars[i];
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Constant expressions: numbers, .set names, + - * / % ( ) and the
 * comparisons and && || ! that .if wants.                             */
/* ------------------------------------------------------------------ */

typedef struct { const char *s; u32 n; u32 i; tr *t; bool ok; } ex;

static i64 ex_or(ex *e);

static i64 ex_primary(ex *e)
{
    e->i = skip_sp(e->s, e->n, e->i);
    if (e->i >= e->n) { e->ok = false; return 0; }
    char c = e->s[e->i];
    if (c == '(') { e->i++; i64 v = ex_or(e); e->i = skip_sp(e->s, e->n, e->i); if (e->i < e->n && e->s[e->i] == ')') e->i++; else e->ok = false; return v; }
    if (c == '-') { e->i++; return (i64)((u64)0 - (u64)ex_primary(e)); }
    if (c == '!') { e->i++; return !ex_primary(e); }
    if (c == '~') { e->i++; return ~ex_primary(e); }
    if (c >= '0' && c <= '9') {
        u64 v = 0;
        if (c == '0' && e->i + 1 < e->n && (e->s[e->i + 1] == 'x' || e->s[e->i + 1] == 'X')) {
            e->i += 2;
            while (e->i < e->n) {
                char h = e->s[e->i];
                u32 d;
                if (h >= '0' && h <= '9') d = (u32)(h - '0');
                else if (h >= 'a' && h <= 'f') d = (u32)(h - 'a' + 10);
                else if (h >= 'A' && h <= 'F') d = (u32)(h - 'A' + 10);
                else break;
                v = (v << 4) | d;
                e->i++;
            }
            return (i64)v;
        }
        while (e->i < e->n && e->s[e->i] >= '0' && e->s[e->i] <= '9') v = v * 10 + (u64)(e->s[e->i++] - '0');
        return (i64)v;
    }
    if (c == '\'' && e->i + 2 < e->n && e->s[e->i + 2] == '\'') { i64 v = (u8)e->s[e->i + 1]; e->i += 3; return v; }
    if (is_ident_char(c)) {
        u32 j = e->i;
        while (j < e->n && is_ident_char(e->s[j])) j++;
        var *v = var_find(e->t, e->s + e->i, j - e->i);
        e->i = j;
        if (!v) { e->ok = false; return 0; }
        return v->value;
    }
    e->ok = false;
    return 0;
}

static i64 ex_mul(ex *e)
{
    i64 v = ex_primary(e);
    for (;;) {
        e->i = skip_sp(e->s, e->n, e->i);
        if (e->i >= e->n) return v;
        char c = e->s[e->i];
        if (c == '*') { e->i++; v = (i64)((u64)v * (u64)ex_primary(e)); }
        else if (c == '/') { e->i++; i64 d = ex_primary(e); v = (d && !(d == -1 && v == (i64)0x8000000000000000ULL)) ? v / d : 0; }
        else if (c == '%') { e->i++; i64 d = ex_primary(e); v = (d && !(d == -1 && v == (i64)0x8000000000000000ULL)) ? v % d : 0; }
        else return v;
    }
}

static i64 ex_add(ex *e)
{
    i64 v = ex_mul(e);
    for (;;) {
        e->i = skip_sp(e->s, e->n, e->i);
        if (e->i >= e->n) return v;
        char c = e->s[e->i];
        if (c == '+') { e->i++; v = (i64)((u64)v + (u64)ex_mul(e)); }
        else if (c == '-') { e->i++; v = (i64)((u64)v - (u64)ex_mul(e)); }
        else return v;
    }
}

static i64 ex_shift(ex *e)
{
    i64 v = ex_add(e);
    for (;;) {
        e->i = skip_sp(e->s, e->n, e->i);
        if (e->i + 1 < e->n && e->s[e->i] == '<' && e->s[e->i + 1] == '<') { e->i += 2; v = (i64)((u64)v << ((u64)ex_add(e) & 63)); }
        else if (e->i + 1 < e->n && e->s[e->i] == '>' && e->s[e->i + 1] == '>') { e->i += 2; v = (i64)((u64)v >> ((u64)ex_add(e) & 63)); }
        else return v;
    }
}

static i64 ex_rel(ex *e)
{
    i64 v = ex_shift(e);
    for (;;) {
        e->i = skip_sp(e->s, e->n, e->i);
        if (e->i + 1 < e->n && e->s[e->i] == '<' && e->s[e->i + 1] == '=') { e->i += 2; v = v <= ex_shift(e); }
        else if (e->i + 1 < e->n && e->s[e->i] == '>' && e->s[e->i + 1] == '=') { e->i += 2; v = v >= ex_shift(e); }
        else if (e->i < e->n && e->s[e->i] == '<' && !(e->i + 1 < e->n && e->s[e->i + 1] == '<')) { e->i++; v = v < ex_shift(e); }
        else if (e->i < e->n && e->s[e->i] == '>' && !(e->i + 1 < e->n && e->s[e->i + 1] == '>')) { e->i++; v = v > ex_shift(e); }
        else return v;
    }
}

static i64 ex_eq(ex *e)
{
    i64 v = ex_rel(e);
    for (;;) {
        e->i = skip_sp(e->s, e->n, e->i);
        if (e->i + 1 < e->n && e->s[e->i] == '=' && e->s[e->i + 1] == '=') { e->i += 2; v = v == ex_rel(e); }
        else if (e->i + 1 < e->n && e->s[e->i] == '!' && e->s[e->i + 1] == '=') { e->i += 2; v = v != ex_rel(e); }
        else return v;
    }
}

static i64 ex_and(ex *e)
{
    i64 v = ex_eq(e);
    for (;;) {
        e->i = skip_sp(e->s, e->n, e->i);
        if (e->i + 1 < e->n && e->s[e->i] == '&' && e->s[e->i + 1] == '&') { e->i += 2; i64 r = ex_eq(e); v = v && r; }
        else if (e->i < e->n && e->s[e->i] == '&') { e->i++; v &= ex_eq(e); }
        else return v;
    }
}

static i64 ex_or(ex *e)
{
    i64 v = ex_and(e);
    for (;;) {
        e->i = skip_sp(e->s, e->n, e->i);
        if (e->i + 1 < e->n && e->s[e->i] == '|' && e->s[e->i + 1] == '|') { e->i += 2; i64 r = ex_and(e); v = v || r; }
        else if (e->i < e->n && e->s[e->i] == '|') { e->i++; v |= ex_and(e); }
        else return v;
    }
}

/* The whole of s as a constant, or false when a name in it is not a
 * .set name -- a label, say. */
static bool constant(tr *t, const char *s, u32 n, i64 *out)
{
    ex e = { s, n, 0, t, true };
    i64 v = ex_or(&e);
    e.i = skip_sp(s, n, e.i);
    if (!e.ok || e.i != n) return false;
    *out = v;
    return true;
}

/* ------------------------------------------------------------------ */
/* Operands                                                            */
/* ------------------------------------------------------------------ */

/* A numbered reference, 1f or 2b: a mark the second pass replaces. */
static bool numbered_ref(const char *s, u32 n)
{
    if (n < 2) return false;
    for (u32 i = 0; i + 1 < n; i++) if (s[i] < '0' || s[i] > '9') return false;
    return s[n - 1] == 'f' || s[n - 1] == 'b';
}

/* A value that is a constant, a numbered label, or a name: written
 * as a number or as the name itself. */
static void put_value(tr *t, const char *s, u32 n)
{
    i64 v;
    while (n && (s[0] == ' ' || s[0] == '\t')) { s++; n--; }
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t')) n--;
    if (constant(t, s, n, &v)) { put_num(t, v); return; }
    if (numbered_ref(s, n)) { put(t, "#"); put_n(t, s, n); put(t, "#"); return; }
    /* a name, perhaps with + n after it */
    put_n(t, s, n);
}

/* One AT&T operand into one of the assembler's. Answers 'r' for a
 * register, 'm' memory, 'i' immediate, 'n' a name or label. */
static char operand(tr *t, const char *s, u32 n)
{
    while (n && (s[0] == ' ' || s[0] == '\t')) { s++; n--; }
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t')) n--;
    if (!n) return 0;

    if (s[0] == '*') { s++; n--; }                         /* an indirect jump's star */

    if (s[0] == '$') { put_value(t, s + 1, n - 1); return 'i'; }

    /* %seg:disp */
    if (s[0] == '%' && n > 3 && s[3] == ':') {
        put(t, "[");
        put_n(t, s + 1, 2);
        put(t, ":");
        put_value(t, s + 4, n - 4);
        put(t, "]");
        return 'm';
    }

    /* disp(%base) */
    i32 paren = -1;
    for (u32 i = 0; i < n; i++) if (s[i] == '(') { paren = (i32)i; break; }
    if (paren >= 0 && s[n - 1] == ')') {
        const char *in = s + paren + 1;
        u32 il = n - (u32)paren - 2;
        /* %base, or %base,%index,scale */
        u32 c1 = il, c2 = il;
        for (u32 i = 0; i < il; i++) if (in[i] == ',') { if (c1 == il) c1 = i; else if (c2 == il) c2 = i; }
        const char *base = in;
        u32 bl = c1;
        const char *index = c1 < il ? in + c1 + 1 : NULL;
        u32 xl = c1 < il ? (c2 < il ? c2 - c1 - 1 : il - c1 - 1) : 0;
        const char *scale = c2 < il ? in + c2 + 1 : NULL;
        u32 sl = c2 < il ? il - c2 - 1 : 0;
        while (bl && base[0] == ' ') { base++; bl--; }
        while (bl && base[bl - 1] == ' ') bl--;
        while (xl && index[0] == ' ') { index++; xl--; }
        while (xl && index[xl - 1] == ' ') xl--;
        while (sl && scale[0] == ' ') { scale++; sl--; }
        while (sl && scale[sl - 1] == ' ') sl--;
        if (bl < 2 || base[0] != '%') { fail(t, index ? "an index without a base is not here" : "what is in the parentheses?", NULL); return 0; }
        base++; bl--;
        if (index && (xl < 2 || index[0] != '%')) { fail(t, "the index is a register", NULL); return 0; }
        if (index) { index++; xl--; }
        put(t, "[");
        if (same_n(base, bl, "rip")) {
            put_value(t, s, (u32)paren);
        } else {
            put_n(t, base, bl);
            if (index) {
                put(t, " + ");
                put_n(t, index, xl);
                if (scale && sl) { put(t, "*"); put_n(t, scale, sl); }
            }
            if (paren > 0) {
                const char *d = s;
                u32 dl = (u32)paren;
                i64 v;
                if (constant(t, d, dl, &v)) {
                    if (v < 0) { put(t, " - "); put_num(t, -v); }
                    else if (v > 0) { put(t, " + "); put_num(t, v); }
                } else { fail(t, "a displacement is a number here", NULL); return 0; }
            }
        }
        put(t, "]");
        return 'm';
    }

    if (s[0] == '%') { put_n(t, s + 1, n - 1); return 'r'; }

    put_value(t, s, n);
    return 'n';
}

/* ------------------------------------------------------------------ */
/* Instructions                                                        */
/* ------------------------------------------------------------------ */

static const char *const stripped[] = {
    "mov", "push", "pop", "lea", "add", "sub", "and", "or", "xor", "cmp", "test",
    "inc", "dec", "shl", "shr", "sar", "rol", "ror", "neg", "not", "imul", "mul",
    "div", "idiv", "xchg", "ret", "call", "jmp", "adc", "sbb", "shrd", "shld",
    "bt", "bts", "btr", NULL };

static void instruction(tr *t, const char *s, u32 n)
{
    u32 ml = 0;
    while (ml < n && s[ml] != ' ' && s[ml] != '\t') ml++;
    char mn[24];
    u32 k = 0;
    while (k < ml && k < 23) { mn[k] = s[k]; k++; }
    mn[k] = 0;
    u32 mlen = k;

    /* the operands, split at commas outside parentheses */
    const char *ops[4];
    u32 ol[4], nops = 0;
    u32 i = skip_sp(s, n, ml);
    while (i < n && nops < 4) {
        u32 start = i;
        i32 depth = 0;
        while (i < n && !(s[i] == ',' && depth == 0)) { if (s[i] == '(') depth++; if (s[i] == ')') depth--; i++; }
        ops[nops] = s + start;
        ol[nops] = i - start;
        nops++;
        if (i < n) i++;
    }

    char suffix = 0;
    bool movz = false;
    if (same_n(mn, mlen, "movabsq") || same_n(mn, mlen, "movabs")) { cpy(mn, "mov"); }
    else if (same_n(mn, mlen, "lretq") || same_n(mn, mlen, "lret")) cpy(mn, "retf");
    else if (same_n(mn, mlen, "cltq")) cpy(mn, "cdqe");
    else if (same_n(mn, mlen, "cqto")) cpy(mn, "cqo");
    else if (same_n(mn, mlen, "cltd")) cpy(mn, "cdq");
    else if (same_n(mn, mlen, "movslq")) cpy(mn, "movsxd");
    else if (mlen == 3 && mn[0] == 'i' && mn[1] == 'n' && (mn[2] == 'b' || mn[2] == 'w' || mn[2] == 'l')) { suffix = mn[2]; cpy(mn, "in"); }
    else if (mlen == 4 && mn[0] == 'o' && mn[1] == 'u' && mn[2] == 't' && (mn[3] == 'b' || mn[3] == 'w' || mn[3] == 'l')) { suffix = mn[3]; cpy(mn, "out"); }
    else if (mlen >= 5 && mn[0] == 'm' && mn[1] == 'o' && mn[2] == 'v' && (mn[3] == 'z' || mn[3] == 's') &&
             (mn[4] == 'b' || mn[4] == 'w')) {
        suffix = mn[4];
        movz = true;
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

    /* Each operand once, into its own words; kinds decide the width. */
    char conv[4][WORD_MAX];
    char kinds[4];
    for (u32 q = 0; q < nops; q++) {
        u32 mark = t->mid_len;
        kinds[q] = operand(t, ops[q], ol[q]);
        u32 got = t->mid_len - mark;
        if (got >= WORD_MAX) { fail(t, "an operand is too long", NULL); return; }
        memcpy(conv[q], t->mid + mark, got);
        conv[q][got] = 0;
        t->mid_len = mark;
        if (t->bad) return;
    }

    const char *width = suffix == 'b' ? "byte " : suffix == 'w' ? "word " : suffix == 'l' ? "dword " : suffix == 'q' ? "qword " : "";
    bool has_reg = false;
    for (u32 q = 0; q < nops; q++) if (kinds[q] == 'r') has_reg = true;

    begin_line(t);
    put(t, "    ");
    put(t, mn);
    for (u32 q = 0; q < nops; q++) {
        u32 idx = nops - 1 - q;                /* source first there; destination first here */
        put(t, q ? ", " : " ");
        if (kinds[idx] == 'm' && (!has_reg || movz) && width[0] && !same_n(mn, (u32)strlen(mn), "lea"))
            put(t, width);
        if (kinds[idx] == 'r' && movz && idx == 0 && suffix) {
            /* the narrow source register, at the suffix's width: movzbl %al is movzx eax, al already */
            put(t, conv[idx]);
        } else put(t, conv[idx]);
    }
    put(t, "\n");
}

/* ------------------------------------------------------------------ */
/* Directives                                                          */
/* ------------------------------------------------------------------ */

/* A quoted string's letters as db values. */
static void put_string_bytes(tr *t, const char *s, u32 n, bool zero)
{
    bool first = true;
    u32 i = 0;
    if (i < n && s[i] == '"') i++;
    while (i < n && s[i] != '"') {
        char c = s[i++];
        if (c == '\\' && i < n) {
            char e = s[i++];
            c = e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e == '0' ? 0 : e;
        }
        if (!first) put(t, ", ");
        put_num(t, (u8)c);
        first = false;
    }
    if (zero) { if (!first) put(t, ", "); put(t, "0"); }
}

static void directive(tr *t, const char *w, u32 wl, const char *rest, u32 rl)
{
    if (same_n(w, wl, ".section")) {
        u32 j = 0;
        while (j < rl && rest[j] != ',' && rest[j] != ' ' && rest[j] != '\t') j++;
        const char *nm = rest;
        u32 nl = j;
        t->skipping = false;
        const char *sec = NULL;
        if (nl >= 5 && memcmp(nm, ".text", 5) == 0) sec = "text";
        else if (nl >= 7 && memcmp(nm, ".rodata", 7) == 0) sec = "rodata";
        else if (nl >= 5 && memcmp(nm, ".data", 5) == 0) sec = "data";
        else if (nl >= 4 && memcmp(nm, ".bss", 4) == 0) sec = "bss";
        else if (nl >= 5 && memcmp(nm, ".user", 5) == 0) sec = "user";
        else if (nl >= 5 && memcmp(nm, ".note", 5) == 0) { t->skipping = true; return; }
        else { fail(t, "i do not know the section", NULL); return; }
        begin_line(t);
        put(t, "section ");
        put(t, sec);
        put(t, "\n");
        return;
    }
    if (same_n(w, wl, ".text")) { t->skipping = false; begin_line(t); put(t, "section text\n"); return; }
    if (same_n(w, wl, ".data")) { t->skipping = false; begin_line(t); put(t, "section data\n"); return; }
    if (same_n(w, wl, ".bss"))  { t->skipping = false; begin_line(t); put(t, "section bss\n"); return; }
    if (t->skipping) return;
    if (same_n(w, wl, ".globl") || same_n(w, wl, ".global")) {
        begin_line(t);
        put(t, "public ");
        put_n(t, rest, rl);
        put(t, "\n");
        return;
    }
    if (same_n(w, wl, ".type") || same_n(w, wl, ".size") || same_n(w, wl, ".file") ||
        same_n(w, wl, ".ident") || same_n(w, wl, ".code64") || same_n(w, wl, ".local")) return;
    if (same_n(w, wl, ".align") || same_n(w, wl, ".balign") || same_n(w, wl, ".p2align")) {
        i64 v;
        u32 j = 0;
        while (j < rl && rest[j] != ',') j++;
        if (!constant(t, rest, j, &v)) { fail(t, ".align wants a number", NULL); return; }
        if (w[1] == 'p') v = (i64)1 << v;
        begin_line(t);
        put(t, "    align ");
        put_num(t, v);
        put(t, "\n");
        return;
    }
    if (same_n(w, wl, ".skip") || same_n(w, wl, ".zero") || same_n(w, wl, ".space")) {
        i64 v;
        u32 j = 0;
        while (j < rl && rest[j] != ',') j++;
        if (!constant(t, rest, j, &v) || v < 0) { fail(t, ".skip wants a count", NULL); return; }
        begin_line(t);
        put(t, "    res ");
        put_num(t, v);
        put(t, "\n");
        return;
    }
    const char *lay = same_n(w, wl, ".quad") ? "dq" : same_n(w, wl, ".long") || same_n(w, wl, ".int") ? "dd" :
                      same_n(w, wl, ".word") || same_n(w, wl, ".short") ? "dw" : same_n(w, wl, ".byte") ? "db" : NULL;
    if (lay) {
        begin_line(t);
        put(t, "    ");
        put(t, lay);
        put(t, " ");
        u32 i = 0;
        bool first = true;
        while (i < rl) {
            u32 j = i;
            i32 depth = 0;
            while (j < rl && !(rest[j] == ',' && depth == 0)) { if (rest[j] == '(') depth++; if (rest[j] == ')') depth--; j++; }
            if (!first) put(t, ", ");
            put_value(t, rest + i, j - i);
            first = false;
            i = j + 1;
        }
        put(t, "\n");
        return;
    }
    if (same_n(w, wl, ".ascii") || same_n(w, wl, ".asciz") || same_n(w, wl, ".string")) {
        begin_line(t);
        put(t, "    db ");
        put_string_bytes(t, rest, rl, w[3] != 'c' || w[5] == 'z');
        put(t, "\n");
        return;
    }
    if (same_n(w, wl, ".set") || same_n(w, wl, ".equ")) {
        u32 j = 0;
        while (j < rl && rest[j] != ',') j++;
        u32 nl = j;
        while (nl && (rest[nl - 1] == ' ' || rest[nl - 1] == '\t')) nl--;
        if (j >= rl || nl == 0 || nl > 31) { fail(t, ".set wants a name, a comma and a value", NULL); return; }
        i64 v;
        if (!constant(t, rest + j + 1, rl - j - 1, &v)) { fail(t, ".set wants a value it can work out", NULL); return; }
        var *x = var_find(t, rest, nl);
        if (!x) {
            if (t->nvars >= VARS_MAX) { fail(t, "too many .set names", NULL); return; }
            x = &t->vars[t->nvars++];
            memcpy(x->name, rest, nl);
            x->name[nl] = 0;
        }
        x->value = v;
        return;
    }
    char what[24];
    u32 k = 0;
    while (k < wl && k < 23) { what[k] = w[k]; k++; }
    what[k] = 0;
    fail(t, "i do not know the directive", what);
}

/* ------------------------------------------------------------------ */
/* Lines, with .rept and .if carried out                               */
/* ------------------------------------------------------------------ */

static void run(tr *t, u32 from, u32 to);

/* The line's first word, when it is a directive. */
static bool line_directive(const char *s, u32 n, const char *w)
{
    u32 i = skip_sp(s, n, 0);
    u32 j = i;
    while (j < n && is_ident_char(s[j])) j++;
    return same_n(s + i, j - i, w);
}

static u32 find_end(tr *t, u32 from, u32 to, const char *open, const char *close, const char *middle, u32 *mid_at)
{
    u32 depth = 0;
    if (mid_at) *mid_at = 0;
    for (u32 k = from; k < to; k++) {
        const char *s = t->src + t->line_at[k];
        u32 n = (k + 1 < t->nlines ? t->line_at[k + 1] : (u32)t->len) - t->line_at[k];
        if (line_directive(s, n, open)) depth++;
        else if (line_directive(s, n, close)) { if (depth == 0) return k; depth--; }
        else if (middle && depth == 0 && line_directive(s, n, middle) && mid_at && !*mid_at) *mid_at = k;
    }
    return to;
}

static void one_line(tr *t, u32 k)
{
    const char *s = t->src + t->line_at[k];
    u32 n = (k + 1 < t->nlines ? t->line_at[k + 1] : (u32)t->len) - t->line_at[k];
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t')) n--;
    t->line = t->line_no[k];
    u32 i = skip_sp(s, n, 0);
    if (i >= n) return;

    /* labels at the front, numbered or named */
    for (;;) {
        u32 j = i;
        while (j < n && is_ident_char(s[j])) j++;
        if (j < n && s[j] == ':' && j > i) {
            bool numeric = true;
            for (u32 q = i; q < j; q++) if (s[q] < '0' || s[q] > '9') numeric = false;
            if (!t->skipping) {
                begin_line(t);
                if (numeric) {
                    if (t->nnumlabs >= NUMLAB_MAX) { fail(t, "too many numbered labels", NULL); return; }
                    u32 num = 0;
                    for (u32 q = i; q < j; q++) num = num * 10 + (u32)(s[q] - '0');
                    numlab *l = &t->numlabs[t->nnumlabs++];
                    l->num = num; l->seq = ++t->seq; l->at = t->mid_len;
                    put(t, ".Lg");
                    put_num(t, (i64)l->seq);
                    put(t, ":\n");
                } else {
                    put_n(t, s + i, j - i);
                    put(t, ":\n");
                }
            }
            i = skip_sp(s, n, j + 1);
            if (i >= n) return;
            continue;
        }
        break;
    }

    if (s[i] == '.') {
        u32 j = i;
        while (j < n && is_ident_char(s[j])) j++;
        u32 r = skip_sp(s, n, j);
        directive(t, s + i, j - i, s + r, n - r);
        return;
    }
    if (t->skipping) return;
    instruction(t, s + i, n - i);
}

static void run(tr *t, u32 from, u32 to)
{
    u32 k = from;
    while (k < to && !t->bad) {
        const char *s = t->src + t->line_at[k];
        u32 n = (k + 1 < t->nlines ? t->line_at[k + 1] : (u32)t->len) - t->line_at[k];
        if (line_directive(s, n, ".rept")) {
            u32 i = skip_sp(s, n, 0) + 5;
            u32 e = n;
            while (e > i && (s[e - 1] == '\n' || s[e - 1] == '\r' || s[e - 1] == ' ' || s[e - 1] == '\t')) e--;
            i64 count;
            t->line = t->line_no[k];
            if (!constant(t, s + i, e - i, &count) || count < 0 || count > 65536) { fail(t, ".rept wants a count", NULL); return; }
            u32 end = find_end(t, k + 1, to, ".rept", ".endr", NULL, NULL);
            if (end >= to) { fail(t, ".rept without .endr", NULL); return; }
            for (i64 c = 0; c < count && !t->bad; c++) run(t, k + 1, end);
            k = end + 1;
            continue;
        }
        if (line_directive(s, n, ".if")) {
            u32 i = skip_sp(s, n, 0) + 3;
            u32 e = n;
            while (e > i && (s[e - 1] == '\n' || s[e - 1] == '\r' || s[e - 1] == ' ' || s[e - 1] == '\t')) e--;
            i64 v;
            t->line = t->line_no[k];
            if (!constant(t, s + i, e - i, &v)) { fail(t, ".if wants a condition it can work out", NULL); return; }
            u32 mid;
            u32 end = find_end(t, k + 1, to, ".if", ".endif", ".else", &mid);
            if (end >= to) { fail(t, ".if without .endif", NULL); return; }
            if (v) run(t, k + 1, mid ? mid : end);
            else if (mid) run(t, mid + 1, end);
            k = end + 1;
            continue;
        }
        if (line_directive(s, n, ".endr") || line_directive(s, n, ".endif") || line_directive(s, n, ".else")) {
            t->line = t->line_no[k];
            fail(t, "an end without a beginning", NULL);
            return;
        }
        one_line(t, k);
        k++;
    }
}

/* ------------------------------------------------------------------ */
/* Getting the text ready: comments blanked, continued lines joined   */
/* ------------------------------------------------------------------ */

static void prepare(tr *t, const u8 *src, u64 len)
{
    u64 o = 0;
    bool in_block = false, in_str = false;
    for (u64 i = 0; i < len && o + 1 < TEXT_MAX; i++) {
        char c = (char)src[i];
        if (in_block) {
            if (c == '*' && i + 1 < len && src[i + 1] == '/') { in_block = false; i++; t->src[o++] = ' '; continue; }
            t->src[o++] = c == '\n' ? '\n' : ' ';
            continue;
        }
        if (in_str) { t->src[o++] = c; if (c == '"') in_str = false; continue; }
        if (c == '"') { in_str = true; t->src[o++] = c; continue; }
        if (c == '/' && i + 1 < len && src[i + 1] == '*') { in_block = true; i++; t->src[o++] = ' '; continue; }
        if (c == '#' || (c == '/' && i + 1 < len && src[i + 1] == '/')) {
            while (i < len && src[i] != '\n') i++;
            if (i < len) t->src[o++] = '\n';
            continue;
        }
        if (c == '\\' && i + 1 < len && src[i + 1] == '\n') { i++; t->src[o++] = ' '; t->src[o++] = 1; continue; }
        t->src[o++] = c;
    }
    t->src[o] = 0;
    t->len = o;

    /* the lines; a joined line keeps the number it began on, and the
     * mark 1 counts the lines it swallowed */
    u32 no = 1;
    t->nlines = 0;
    u64 start = 0;
    for (u64 i = 0; i <= o; i++) {
        if (i < o && t->src[i] != '\n') continue;
        if (t->nlines < LINES_MAX) {
            t->line_at[t->nlines] = (u32)start;
            t->line_no[t->nlines] = no;
            t->nlines++;
        }
        for (u64 q = start; q < i; q++) if (t->src[q] == 1) { t->src[q] = ' '; no++; }
        no++;
        start = i + 1;
    }
}

/* ------------------------------------------------------------------ */
/* The numbered labels, resolved                                       */
/* ------------------------------------------------------------------ */

static u32 resolve_marks(tr *t, u8 *out, u32 max)
{
    u32 o = 0;
    for (u32 i = 0; i < t->mid_len && o + 16 < max; i++) {
        char c = t->mid[i];
        if (c != '#') { out[o++] = (u8)c; continue; }
        u32 j = i + 1;
        u32 num = 0;
        while (j < t->mid_len && t->mid[j] >= '0' && t->mid[j] <= '9') num = num * 10 + (u32)(t->mid[j++] - '0');
        char dir = j < t->mid_len ? t->mid[j] : 0;
        if (j + 1 >= t->mid_len || t->mid[j + 1] != '#') { out[o++] = '#'; continue; }
        numlab *best = NULL;
        for (u32 k = 0; k < t->nnumlabs; k++) {
            numlab *l = &t->numlabs[k];
            if (l->num != num) continue;
            if (dir == 'f' && l->at > i && (!best || l->at < best->at)) best = l;
            if (dir == 'b' && l->at < i && (!best || l->at > best->at)) best = l;
        }
        if (!best) { fail(t, "a numbered label is referred to and never set", NULL); return 0; }
        const char *pre = ".Lg";
        while (*pre) out[o++] = (u8)*pre++;
        char d[12]; u32 nd = 0; u32 v = best->seq;
        while (v) { d[nd++] = (char)('0' + v % 10); v /= 10; }
        while (nd) out[o++] = (u8)d[--nd];
        i = j + 1;
    }
    return o;
}

/* ------------------------------------------------------------------ */

/* Whether a text reads like the gnu dialect: registers with a % in
 * front, or lines that begin with one of its directives. The
 * machine's own dialect has neither. */
bool gnu_looks(const u8 *src, u64 len)
{
    u64 n = len < 8192 ? len : 8192;
    for (u64 i = 0; i + 8 < n; i++) {
        if (src[i] == '%' && (src[i + 1] == 'r' || src[i + 1] == 'e' || src[i + 1] == 'a' ||
                              src[i + 1] == 'g' || src[i + 1] == 'x' || src[i + 1] == 'd' || src[i + 1] == 'c'))
            return true;
        if (src[i] == '.' && (i == 0 || src[i - 1] == '\n' || src[i - 1] == ' ' || src[i - 1] == '\t')) {
            if (memcmp(src + i, ".section", 8) == 0 || memcmp(src + i, ".globl", 6) == 0 ||
                memcmp(src + i, ".global", 7) == 0 || memcmp(src + i, ".text", 5) == 0 ||
                memcmp(src + i, ".align", 6) == 0 || memcmp(src + i, ".set", 4) == 0)
                return true;
        }
    }
    return false;
}

static tr T;
static u8 *final_buf;

i64 asm_assemble_gnu(const u8 *src, u64 len, u8 *out, u64 max,
                     char *err, u32 errmax)
{
    tr *t = &T;
    char *keep_src = t->src;
    u32 *keep_at = t->line_at, *keep_no = t->line_no;
    char *keep_mid = t->mid;
    numlab *keep_nl = t->numlabs;
    memset(t, 0, sizeof(*t));
    t->src = keep_src; t->line_at = keep_at; t->line_no = keep_no; t->mid = keep_mid; t->numlabs = keep_nl;
    t->err = err; t->errmax = errmax;
    if (errmax) err[0] = 0;

    if (!t->src) t->src = (char *)lang_big_alloc(TEXT_MAX);
    if (!t->line_at) t->line_at = (u32 *)lang_big_alloc(sizeof(u32) * LINES_MAX);
    if (!t->line_no) t->line_no = (u32 *)lang_big_alloc(sizeof(u32) * LINES_MAX);
    if (!t->mid) t->mid = (char *)lang_big_alloc(TEXT_MAX);
    if (!t->numlabs) t->numlabs = (numlab *)lang_big_alloc(sizeof(numlab) * NUMLAB_MAX);
    if (!final_buf) final_buf = (u8 *)lang_big_alloc(TEXT_MAX);
    if (!t->src || !t->line_at || !t->line_no || !t->mid || !t->numlabs || !final_buf) {
        t->line = 0;
        fail(t, "there is no room for the translation", NULL);
        return -1;
    }
    if (len >= TEXT_MAX / 2) { t->line = 0; fail(t, "the text is too large to translate", NULL); return -1; }

    prepare(t, src, len);
    put(t, "; translated from the gnu dialect; the source lies beside this\n");
    run(t, 0, t->nlines);
    if (t->bad) return -1;
    u32 n = resolve_marks(t, final_buf, TEXT_MAX);
    if (t->bad) return -1;
    return asm_assemble_dialect(final_buf, n, true, out, max, err, errmax);
}
