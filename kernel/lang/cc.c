/*
 * cc.c -- the compiler: c, the way this machine speaks it.
 *
 * One pass over the text to read it, one walk over each function's
 * tree to say it in the assembler's words. Nothing clever: every
 * value passes through rax, every variable lives in memory, every
 * operator is the handful of instructions it plainly is. What comes
 * out is meant to be read -- it lies beside the source as a text --
 * and the assembler is the only thing that ever encodes a byte.
 *
 * The language is c, in the shape a person writes it: char, int and
 * long, signed or not; pointers, arrays, structs, typedefs, enums;
 * functions with up to six parameters, and pointers to them; the
 * operators with their precedence; if, while, for, do, switch,
 * break, continue, return; sizeof and casts; #define, #include of a
 * text lying beside this one, #ifdef and its kin. Arithmetic is done
 * in 64 bits and cut to size on the way into a variable.
 *
 * Not here, and said so on the compiler's own page: unions, bit
 * fields, floating point, varargs, function-like macros, #if with
 * arithmetic, goto. The kernel needs some of those; that is the
 * honest distance still to go.
 */
#include <eb/cc.h>
#include <eb/string.h>

#define NAME_MAX    32
#define STR_POOL    32768
#define NSTR        256
#define NTYPES      512
#define NMEMBERS    1024
#define NSYMS       2048
#define NNODES      16384
#define NMACROS     128
#define MACRO_TEXT  200
#define NTYPEDEFS   128
#define NTAGS       64
#define NSRC        8
#define NCOND       16
#define NPARAMS     6

/* ------------------------------------------------------------------ */
/* Types                                                               */
/* ------------------------------------------------------------------ */

enum { T_VOID, T_CHAR, T_INT, T_LONG, T_PTR, T_ARR, T_STRUCT, T_FUNC };

typedef struct type type;
struct type {
    u8    kind;
    bool  uns;
    u32   size, align;
    type *base;                       /* ptr, arr: the element; func: what it returns */
    u64   len;                        /* arr */
    u32   mfirst, mcount;             /* struct: its members */
    u32   nparams;
    type *params[NPARAMS];
    bool  defined;                    /* struct: the body has been seen */
};

typedef struct { char name[NAME_MAX]; type *ty; u32 offset; } member;

/* ------------------------------------------------------------------ */
/* Symbols                                                             */
/* ------------------------------------------------------------------ */

enum { S_LOCAL, S_GLOBAL, S_FUNC, S_ENUM };

typedef struct {
    char  name[NAME_MAX];
    type *ty;
    u8    kind;
    i64   val;                        /* local: its offset below rbp; enum: its value */
    bool  defined;                    /* func: a body was seen */
} sym;

/* ------------------------------------------------------------------ */
/* The tree                                                            */
/* ------------------------------------------------------------------ */

enum {
    ND_NONE, ND_NUM, ND_VAR, ND_STR,
    ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_MOD, ND_AND, ND_OR, ND_XOR,
    ND_SHL, ND_SHR, ND_EQ, ND_NE, ND_LT, ND_LE,
    ND_LAND, ND_LOR, ND_NOT, ND_BITNOT, ND_NEG,
    ND_ASSIGN, ND_COND, ND_ADDR, ND_DEREF, ND_MEMBER, ND_CALL,
    ND_CAST, ND_INCDEC, ND_SYSCALL, ND_COMMA,
    ND_EXPR, ND_RETURN, ND_IF, ND_WHILE, ND_DO, ND_FOR, ND_BLOCK,
    ND_BREAK, ND_CONTINUE, ND_SWITCH, ND_CASE, ND_DEFAULT
};

typedef struct {
    u8    kind;
    u8    op;                         /* compound assignment: the ND_ of the operation */
    bool  post;                       /* incdec: after, not before */
    u32   lhs, rhs, third;            /* children; 0 is none */
    u32   next;                       /* the next in a list */
    type *ty;
    i64   val;
    u32   sym;                        /* var, call by name */
    u32   aux;                        /* member, string, a case's chain */
    u32   line;
} node;

/* ------------------------------------------------------------------ */
/* Tokens                                                              */
/* ------------------------------------------------------------------ */

enum { TK_EOF, TK_NUM, TK_IDENT, TK_PUNCT, TK_STR };

typedef struct {
    u8   kind;
    i64  val;
    char text[NAME_MAX];
    u32  str;                         /* TK_STR: which string */
    u32  line;
    char where[NAME_MAX];             /* which text it came from */
} token;

typedef struct {
    const u8 *text;
    u64  len, pos;
    u32  line;
    char name[NAME_MAX];
    bool macro;                       /* a macro's words; no lines of its own */
    bool bol;
} source;

typedef struct { char name[NAME_MAX]; char text[MACRO_TEXT]; } macro;

/* ------------------------------------------------------------------ */
/* The compiler's whole state, static: the kernel has no malloc to    */
/* spend on a compile, and one compile runs at a time.                 */
/* ------------------------------------------------------------------ */

static struct {
    /* reading */
    source src[NSRC];
    u32    nsrc;
    macro  macros[NMACROS];
    u32    nmacros;
    bool   cond[NCOND];
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
    type  *t_void, *t_char, *t_uchar, *t_int, *t_uint, *t_long, *t_ulong;

    node   nodes[NNODES];
    u32    nnodes;

    /* strings */
    u8     spool[STR_POOL];
    u32    spool_len;
    struct { u32 at, len; } strs[NSTR];
    u32    nstrs;

    /* the function being read */
    i64    frame;
    type  *ret_ty;
    u32    fn_no;
    u32    brk[32], cont[32];
    u32    nbrk, ncont;
    u32    sw_case[32];               /* the case chain being gathered */
    u32    nsw;

    /* writing */
    char  *out;
    u64    max, len;
    u32    label;
    char  *err;
    u32    errmax;
    bool   bad;
} C;

/* A name into a name-sized place, cut to fit. */
static void cpy(char *d, const char *s)
{
    u32 i = 0;
    while (s[i] && i < NAME_MAX - 1) { d[i] = s[i]; i++; }
    d[i] = 0;
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

static void fail(const char *why, const char *what)
{
    fail_at(C.cur.line, C.cur.where, why, what);
}

/* ------------------------------------------------------------------ */
/* Reading characters                                                  */
/* ------------------------------------------------------------------ */

static source *top(void) { return C.nsrc ? &C.src[C.nsrc - 1] : NULL; }

static i32 peekc(u32 ahead)
{
    /* Only within the current source: a macro's words end where
     * they end, and the text that named it goes on after. */
    source *s = top();
    if (!s || s->pos + ahead >= s->len) return -1;
    return s->text[s->pos + ahead];
}

static i32 getc_(void)
{
    source *s = top();
    if (!s) return -1;
    if (s->pos >= s->len) return -1;
    u8 c = s->text[s->pos++];
    if (c == '\n' && !s->macro) { s->line++; s->bol = true; }
    else if (c != ' ' && c != '\t' && c != '\r') s->bol = false;
    return c;
}

static bool push_source(const u8 *text, u64 len, const char *name, bool is_macro)
{
    if (C.nsrc >= NSRC) { fail("includes nest too deep", NULL); return false; }
    source *s = &C.src[C.nsrc++];
    s->text = text;
    s->len = len;
    s->pos = 0;
    s->line = 1;
    s->macro = is_macro;
    s->bol = true;
    u32 i = 0;
    while (name[i] && i < NAME_MAX - 1) { s->name[i] = name[i]; i++; }
    s->name[i] = 0;
    return true;
}

/* ------------------------------------------------------------------ */
/* Directives                                                          */
/* ------------------------------------------------------------------ */

static bool skipping(void)
{
    for (u32 i = 0; i < C.ncond; i++) if (!C.cond[i]) return true;
    return false;
}

static macro *macro_find(const char *nm)
{
    for (u32 i = 0; i < C.nmacros; i++)
        if (strcmp(C.macros[i].name, nm) == 0) return &C.macros[i];
    return NULL;
}

/* The rest of the current line, trimmed. */
static u32 read_line(char *buf, u32 max)
{
    u32 n = 0;
    for (;;) {
        i32 c = peekc(0);
        if (c < 0 || c == '\n') break;
        getc_();
        if (n < max - 1) buf[n++] = (char)c;
    }
    while (n && (buf[n - 1] == ' ' || buf[n - 1] == '\t' || buf[n - 1] == '\r')) n--;
    buf[n] = 0;
    return n;
}

static void directive(void)
{
    char line[MACRO_TEXT + NAME_MAX + 16];
    read_line(line, sizeof(line));
    u32 i = 0;
    while (line[i] == ' ' || line[i] == '\t') i++;
    char word[16];
    u32 w = 0;
    while (line[i] && line[i] != ' ' && line[i] != '\t' && w < 15) word[w++] = line[i++];
    word[w] = 0;
    while (line[i] == ' ' || line[i] == '\t') i++;

    char name[NAME_MAX];
    u32 n = 0;
    while (line[i] && line[i] != ' ' && line[i] != '\t' && line[i] != '"' && n < NAME_MAX - 1)
        name[n++] = line[i++];
    name[n] = 0;
    while (line[i] == ' ' || line[i] == '\t') i++;

    if (strcmp(word, "ifdef") == 0 || strcmp(word, "ifndef") == 0) {
        if (C.ncond >= NCOND) { fail("conditions nest too deep", NULL); return; }
        bool have = macro_find(name) != NULL;
        C.cond[C.ncond++] = (word[2] == 'n') ? !have : have;
        return;
    }
    if (strcmp(word, "else") == 0) {
        if (!C.ncond) { fail("#else without #ifdef", NULL); return; }
        C.cond[C.ncond - 1] = !C.cond[C.ncond - 1];
        return;
    }
    if (strcmp(word, "endif") == 0) {
        if (!C.ncond) { fail("#endif without #ifdef", NULL); return; }
        C.ncond--;
        return;
    }
    if (skipping()) return;

    if (strcmp(word, "define") == 0) {
        if (!name[0]) { fail("#define what?", NULL); return; }
        macro *m = macro_find(name);
        if (!m) {
            if (C.nmacros >= NMACROS) { fail("too many defines", NULL); return; }
            m = &C.macros[C.nmacros++];
            cpy(m->name, name);
        }
        u32 k = 0;
        while (line[i] && k < MACRO_TEXT - 1) m->text[k++] = line[i++];
        m->text[k] = 0;
        return;
    }
    if (strcmp(word, "undef") == 0) {
        macro *m = macro_find(name);
        if (m) *m = C.macros[--C.nmacros];
        return;
    }
    if (strcmp(word, "include") == 0) {
        if (line[i] != '"') { fail("#include wants a name in double quotes", NULL); return; }
        i++;
        n = 0;
        while (line[i] && line[i] != '"' && n < NAME_MAX - 1) name[n++] = line[i++];
        name[n] = 0;
        const u8 *text;
        u64 len;
        if (!C.find || !C.find(C.ctx, name, &text, &len)) {
            fail("no text of that name lies beside this one:", name);
            return;
        }
        push_source(text, len, name, false);
        return;
    }
    fail("i do not know the directive", word);
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
    case '0': return 0;
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
    default: return c;
    }
}

static void lex_raw(token *t);

static void lex_ident_or_macro(token *t, i32 first)
{
    u32 n = 0;
    t->text[n++] = (char)first;
    for (;;) {
        i32 c = peekc(0);
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_';
        if (!ok) break;
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
        if (C.src[i].macro && strcmp(C.src[i].name, t->text) == 0) return;
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
            C.nsrc--;
            continue;
        }
        t->line = s->line;
        cpy(t->where, s->name);

        i32 c = peekc(0);
        bool at_bol = s->bol;

        if (c == '\n' || c == ' ' || c == '\t' || c == '\r') { getc_(); continue; }

        if (c == '#' && at_bol && !s->macro) {
            getc_();
            directive();
            if (C.bad) { t->kind = TK_EOF; return; }
            continue;
        }

        if (skipping()) {
            /* Inside a false #ifdef: only directives are read. */
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

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            lex_ident_or_macro(t, c);
            return;
        }

        if (c >= '0' && c <= '9') {
            i64 v = 0;
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
                    v = v * 16 + d;
                }
            } else {
                v = c - '0';
                while (peekc(0) >= '0' && peekc(0) <= '9') v = v * 10 + (getc_() - '0');
            }
            /* Suffixes say nothing here: every number is a long. */
            while (peekc(0) == 'u' || peekc(0) == 'U' || peekc(0) == 'l' || peekc(0) == 'L') getc_();
            t->kind = TK_NUM;
            t->val = v;
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
            C.strs[C.nstrs].len = C.spool_len - at;
            t->kind = TK_STR;
            t->str = C.nstrs++;
            return;
        }

        /* Punctuation, longest first. */
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

static bool is(const char *p)
{
    return C.cur.kind == TK_PUNCT && strcmp(C.cur.text, p) == 0;
}

static bool is_kw(const char *w)
{
    return C.cur.kind == TK_IDENT && strcmp(C.cur.text, w) == 0;
}

static bool eat(const char *p)
{
    if (!is(p)) return false;
    advance();
    return true;
}

static void expect(const char *p)
{
    if (!eat(p)) {
        char what[NAME_MAX + 8];
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
    type *t = new_type(T_PTR, 8, 8);
    t->base = b;
    return t;
}

static type *array_of(type *b, u64 n)
{
    type *t = new_type(T_ARR, (u32)(b->size * n), b->align);
    t->base = b;
    t->len = n;
    return t;
}

static bool is_int(const type *t)
{
    return t->kind == T_CHAR || t->kind == T_INT || t->kind == T_LONG;
}

static bool is_ptr(const type *t)
{
    return t->kind == T_PTR || t->kind == T_ARR;
}

static type *typedef_find(const char *nm)
{
    for (u32 i = 0; i < C.ntypedefs; i++)
        if (strcmp(C.typedefs[i].name, nm) == 0) return C.typedefs[i].ty;
    return NULL;
}

static type *tag_find(const char *nm)
{
    for (u32 i = 0; i < C.ntags; i++)
        if (strcmp(C.tags[i].name, nm) == 0) return C.tags[i].ty;
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Symbols, scoped                                                     */
/* ------------------------------------------------------------------ */

static sym *sym_find(const char *nm)
{
    for (u32 i = C.nsyms; i > 0; i--)
        if (strcmp(C.syms[i - 1].name, nm) == 0) return &C.syms[i - 1];
    return NULL;
}

static sym *sym_add(const char *nm, type *ty, u8 kind)
{
    if (C.nsyms >= NSYMS) { fail("too many names", NULL); return &C.syms[0]; }
    sym *s = &C.syms[C.nsyms++];
    memset(s, 0, sizeof(*s));
    cpy(s->name, nm);
    s->ty = ty;
    s->kind = kind;
    return s;
}

static void scope_in(void)
{
    if (C.nscope < 64) C.scope[C.nscope++] = C.nsyms;
}

static void scope_out(void)
{
    if (C.nscope) C.nsyms = C.scope[--C.nscope];
}

/* A local's home: below rbp, aligned to what it is. */
static i64 local_slot(type *ty)
{
    u32 al = ty->align ? ty->align : 1;
    C.frame += ty->size;
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

/* ------------------------------------------------------------------ */
/* Giving every node its type                                          */
/* ------------------------------------------------------------------ */

static void add_type(u32 i);

static u32 scaled(u32 n, u64 by)
{
    u32 m = mk_bin(ND_MUL, n, mk_num((i64)by));
    N(m)->ty = C.t_long;
    return m;
}

static void add_type(u32 i)
{
    if (!i) return;
    node *n = N(i);
    /* A cast knows its type from birth, but its child does not. */
    if (n->ty && n->kind != ND_CAST) return;

    add_type(n->lhs);
    add_type(n->rhs);
    add_type(n->third);
    for (u32 a = n->next; a; a = N(a)->next) add_type(a);
    /* aux is a node wherever it is not a string, a member or a label
     * number: a call's arguments, a block's statements, a for's body,
     * a switch's cases. */
    if (n->kind != ND_STR && n->kind != ND_MEMBER &&
        n->kind != ND_CASE && n->kind != ND_DEFAULT)
        for (u32 a = n->aux; a; a = N(a)->next) add_type(a);

    node *l = n->lhs ? N(n->lhs) : NULL;
    node *r = n->rhs ? N(n->rhs) : NULL;

    switch (n->kind) {
    case ND_NUM: n->ty = C.t_long; break;
    case ND_STR: n->ty = array_of(C.t_char, C.strs[n->aux].len); break;
    case ND_VAR: n->ty = C.syms[n->sym].ty; break;

    case ND_ADD:
        if (is_ptr(l->ty) && is_int(r->ty)) {
            n->rhs = scaled(n->rhs, l->ty->base->size);
            n->ty = l->ty->kind == T_ARR ? ptr_to(l->ty->base) : l->ty;
        } else if (is_int(l->ty) && is_ptr(r->ty)) {
            n->lhs = scaled(n->lhs, r->ty->base->size);
            n->ty = r->ty->kind == T_ARR ? ptr_to(r->ty->base) : r->ty;
        } else if (is_int(l->ty) && is_int(r->ty)) {
            n->ty = (l->ty->kind == T_LONG || r->ty->kind == T_LONG) ? C.t_long : C.t_int;
            if (l->ty->uns || r->ty->uns) n->ty = n->ty == C.t_long ? C.t_ulong : C.t_uint;
        } else fail_at(n->line, NULL, "those cannot be added", NULL);
        break;

    case ND_SUB:
        if (is_ptr(l->ty) && is_int(r->ty)) {
            n->rhs = scaled(n->rhs, l->ty->base->size);
            n->ty = l->ty->kind == T_ARR ? ptr_to(l->ty->base) : l->ty;
        } else if (is_ptr(l->ty) && is_ptr(r->ty)) {
            /* The distance, in elements. */
            u32 d = mk_bin(ND_DIV, i, 0);
            node *dn = N(d);
            u32 sub = mk_bin(ND_SUB, n->lhs, n->rhs);
            N(sub)->ty = C.t_long;
            dn->lhs = sub;
            dn->rhs = mk_num((i64)l->ty->base->size);
            dn->ty = C.t_long;
            /* Turn this node into that division. */
            u32 keep = n->next;
            *n = *dn;
            n->next = keep;
        } else if (is_int(l->ty) && is_int(r->ty)) {
            n->ty = (l->ty->kind == T_LONG || r->ty->kind == T_LONG) ? C.t_long : C.t_int;
            if (l->ty->uns || r->ty->uns) n->ty = n->ty == C.t_long ? C.t_ulong : C.t_uint;
        } else fail_at(n->line, NULL, "those cannot be subtracted", NULL);
        break;

    case ND_MUL: case ND_DIV: case ND_MOD:
    case ND_AND: case ND_OR: case ND_XOR:
        if (!is_int(l->ty) || !is_int(r->ty)) { fail_at(n->line, NULL, "that wants numbers on both sides", NULL); break; }
        n->ty = (l->ty->kind == T_LONG || r->ty->kind == T_LONG) ? C.t_long : C.t_int;
        if (l->ty->uns || r->ty->uns) n->ty = n->ty == C.t_long ? C.t_ulong : C.t_uint;
        break;

    case ND_SHL: case ND_SHR:
        n->ty = l->ty;
        break;

    case ND_EQ: case ND_NE: case ND_LT: case ND_LE:
    case ND_LAND: case ND_LOR: case ND_NOT:
        n->ty = C.t_long;
        break;

    case ND_BITNOT: case ND_NEG:
        n->ty = l->ty;
        break;

    case ND_ASSIGN:
        if (l->ty->kind == T_ARR) fail_at(n->line, NULL, "an array is not something to assign to", NULL);
        n->ty = l->ty;
        break;

    case ND_COND:
        n->ty = r->ty->kind == T_VOID ? C.t_void : r->ty;
        break;

    case ND_COMMA:
        n->ty = r->ty;
        break;

    case ND_ADDR:
        n->ty = ptr_to(l->ty->kind == T_ARR ? l->ty->base : l->ty);
        break;

    case ND_DEREF:
        if (!is_ptr(l->ty)) { fail_at(n->line, NULL, "only a pointer can be followed", NULL); break; }
        if (l->ty->base->kind == T_VOID) { fail_at(n->line, NULL, "a pointer to void leads nowhere in particular", NULL); break; }
        n->ty = l->ty->base;
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
        break;
    }

    case ND_CAST:
        break;                        /* set when made */

    case ND_INCDEC:
        n->ty = l->ty;
        break;

    case ND_SYSCALL:
        n->ty = C.t_long;
        break;

    default:
        n->ty = C.t_void;
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Reading declarations                                                */
/* ------------------------------------------------------------------ */

static u32 expr(void);
static u32 assign(void);
static i64 const_eval(u32 i);
static type *declspec(bool *is_typedef);
static type *declarator(type *base, char *name);

static bool is_typename(void)
{
    if (C.cur.kind != TK_IDENT) return false;
    static const char *const kws[] = {
        "void", "char", "int", "long", "short", "unsigned", "signed",
        "struct", "enum", "const", "static", "extern", "volatile",
        "_Bool", "bool", "typedef", NULL };
    for (u32 i = 0; kws[i]; i++) if (strcmp(C.cur.text, kws[i]) == 0) return true;
    return typedef_find(C.cur.text) != NULL;
}

static type *struct_decl(void)
{
    /* struct tag { ... }, struct tag, or struct { ... } */
    char tag[NAME_MAX] = { 0 };
    if (C.cur.kind == TK_IDENT && !is("{")) {
        cpy(tag, C.cur.text);
        advance();
    }
    type *t = tag[0] ? tag_find(tag) : NULL;
    if (!is("{")) {
        if (t) return t;
        if (!tag[0]) { fail("struct wants a name or a body", NULL); return C.t_long; }
        /* A tag before its body: a pointer to it may be declared now. */
        t = new_type(T_STRUCT, 0, 1);
        if (C.ntags < NTAGS) { cpy(C.tags[C.ntags].name, tag); C.tags[C.ntags].ty = t; C.ntags++; }
        return t;
    }
    advance();
    if (!t) {
        t = new_type(T_STRUCT, 0, 1);
        if (tag[0] && C.ntags < NTAGS) { cpy(C.tags[C.ntags].name, tag); C.tags[C.ntags].ty = t; C.ntags++; }
    }
    if (t->defined) { fail("that struct already has a body", tag); return t; }

    t->mfirst = C.nmembers;
    u32 off = 0, al = 1;
    while (!is("}") && C.cur.kind != TK_EOF && !C.bad) {
        bool td;
        type *base = declspec(&td);
        for (;;) {
            char nm[NAME_MAX];
            type *mt = declarator(base, nm);
            if (C.bad) return t;
            if (C.nmembers >= NMEMBERS) { fail("too many members", NULL); return t; }
            member *m = &C.members[C.nmembers++];
            cpy(m->name, nm);
            m->ty = mt;
            u32 ma = mt->align ? mt->align : 1;
            off = (off + ma - 1) / ma * ma;
            m->offset = off;
            off += mt->size;
            if (ma > al) al = ma;
            if (!eat(",")) break;
        }
        expect(";");
    }
    expect("}");
    t->mcount = C.nmembers - t->mfirst;
    t->align = al;
    t->size = (off + al - 1) / al * al;
    t->defined = true;
    return t;
}

static type *enum_decl(void)
{
    if (C.cur.kind == TK_IDENT && !is("{")) advance();      /* the tag, unused */
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

static type *declspec(bool *is_typedef)
{
    *is_typedef = false;
    bool uns = false, sgn = false;
    i32 base = -1;                    /* T_ */
    type *named = NULL;
    for (;;) {
        if (C.cur.kind != TK_IDENT) break;
        const char *w = C.cur.text;
        if (strcmp(w, "typedef") == 0) { *is_typedef = true; advance(); continue; }
        if (strcmp(w, "const") == 0 || strcmp(w, "static") == 0 ||
            strcmp(w, "extern") == 0 || strcmp(w, "volatile") == 0) { advance(); continue; }
        if (strcmp(w, "unsigned") == 0) { uns = true; advance(); continue; }
        if (strcmp(w, "signed") == 0)   { sgn = true; advance(); continue; }
        if (strcmp(w, "void") == 0)  { base = T_VOID; advance(); continue; }
        if (strcmp(w, "char") == 0)  { base = T_CHAR; advance(); continue; }
        if (strcmp(w, "_Bool") == 0 || strcmp(w, "bool") == 0) { base = T_CHAR; uns = true; advance(); continue; }
        if (strcmp(w, "short") == 0) { base = T_INT; advance(); continue; }
        if (strcmp(w, "int") == 0)   { if (base < 0) base = T_INT; advance(); continue; }
        if (strcmp(w, "long") == 0)  { base = T_LONG; advance(); continue; }
        if (strcmp(w, "struct") == 0) { advance(); named = struct_decl(); continue; }
        if (strcmp(w, "enum") == 0)   { advance(); named = enum_decl(); continue; }
        type *td = typedef_find(w);
        if (td && base < 0 && !named) { named = td; advance(); continue; }
        break;
    }
    (void)sgn;
    if (named) return named;
    if (base < 0) {
        if (uns || sgn) base = T_INT;
        else { fail("a type was expected here, not", C.cur.text); return C.t_long; }
    }
    switch (base) {
    case T_VOID: return C.t_void;
    case T_CHAR: return uns ? C.t_uchar : C.t_char;
    case T_INT:  return uns ? C.t_uint : C.t_int;
    default:     return uns ? C.t_ulong : C.t_long;
    }
}

/* ( params ) after a name: the function's shape. Names are kept in
 * pnames for a definition that follows. */
static type *func_suffix(type *ret, char pnames[NPARAMS][NAME_MAX])
{
    type *f = new_type(T_FUNC, 8, 8);
    f->base = ret;
    if (is_kw("void") && strcmp(C.nxt.text, ")") == 0 && C.nxt.kind == TK_PUNCT) {
        advance();
    }
    while (!is(")") && C.cur.kind != TK_EOF && !C.bad) {
        if (f->nparams >= NPARAMS) { fail("six parameters is the most a function takes here", NULL); return f; }
        bool td;
        type *pt = declspec(&td);
        char nm[NAME_MAX];
        pt = declarator(pt, nm);
        if (pt->kind == T_ARR) pt = ptr_to(pt->base);
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
    while (eat("*")) base = ptr_to(base);

    /* ( * name ) ( params ): a pointer to a function. */
    if (is("(") && C.nxt.kind == TK_PUNCT && strcmp(C.nxt.text, "*") == 0) {
        advance();
        advance();
        if (C.cur.kind == TK_IDENT) { cpy(name, C.cur.text); advance(); }
        expect(")");
        expect("(");
        type *f = func_suffix(base, NULL);
        return ptr_to(f);
    }

    if (C.cur.kind == TK_IDENT && !is_typename()) {
        cpy(name, C.cur.text);
        advance();
    }

    if (eat("(")) return func_suffix(base, NULL);

    /* Arrays, outermost first: int a[2][3] is two of three. */
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

/* ------------------------------------------------------------------ */
/* Reading expressions                                                 */
/* ------------------------------------------------------------------ */

static u32 cast_expr(void);

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
    if (C.cur.kind == TK_STR) {
        u32 n = mk(ND_STR);
        N(n)->aux = C.cur.str;
        advance();
        /* Neighbouring strings join. */
        while (C.cur.kind == TK_STR) {
            u32 a = C.strs[N(n)->aux].at;
            u32 al = C.strs[N(n)->aux].len - 1;
            u32 b = C.strs[C.cur.str].at;
            u32 bl = C.strs[C.cur.str].len;
            if (C.spool_len + al + bl >= STR_POOL || C.nstrs >= NSTR) { fail("the strings are too long together", NULL); return n; }
            u32 at = C.spool_len;
            memcpy(C.spool + C.spool_len, C.spool + a, al);
            C.spool_len += al;
            memcpy(C.spool + C.spool_len, C.spool + b, bl);
            C.spool_len += bl;
            C.strs[C.nstrs].at = at;
            C.strs[C.nstrs].len = al + bl;
            N(n)->aux = C.nstrs++;
            advance();
        }
        return n;
    }
    if (is_kw("sizeof")) {
        advance();
        if (is("(") && (C.nxt.kind == TK_IDENT)) {
            /* Maybe a type, maybe an expression in parentheses. */
            token save = C.cur;
            (void)save;
            advance();
            if (is_typename()) {
                bool td;
                type *t = declspec(&td);
                char nm[NAME_MAX];
                t = declarator(t, nm);
                expect(")");
                return mk_num((i64)t->size);
            }
            /* An expression: read it as if the paren were still open. */
            u32 e = expr();
            expect(")");
            add_type(e);
            return mk_num((i64)N(e)->ty->size);
        }
        u32 e = cast_expr();
        add_type(e);
        return mk_num((i64)N(e)->ty->size);
    }
    if (is_kw("syscall")) {
        advance();
        expect("(");
        u32 n = mk(ND_SYSCALL);
        u32 tail = 0, count = 0;
        while (!is(")") && !C.bad) {
            u32 a = assign();
            if (count >= 6) { fail("syscall takes the number and five arguments at most", NULL); return n; }
            if (tail) N(tail)->next = a; else N(n)->aux = a;
            tail = a;
            count++;
            if (!eat(",")) break;
        }
        expect(")");
        N(n)->val = count;
        return n;
    }
    if (C.cur.kind == TK_IDENT) {
        sym *s = sym_find(C.cur.text);
        if (!s) { fail("i do not know", C.cur.text); return mk_num(0); }
        u32 idx = (u32)(s - C.syms);
        advance();
        if (s->kind == S_ENUM) return mk_num(s->val);
        u32 n = mk(ND_VAR);
        N(n)->sym = idx;
        return n;
    }
    fail("i cannot read", C.cur.text);
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
            u32 tail = 0, count = 0;
            while (!is(")") && !C.bad) {
                u32 a = assign();
                if (count >= NPARAMS) { fail("six arguments is the most a call takes here", NULL); return c; }
                if (tail) N(tail)->next = a; else N(c)->aux = a;
                tail = a;
                count++;
                if (!eat(",")) break;
            }
            expect(")");
            N(c)->val = count;
            n = c;
            continue;
        }
        if (is(".") || is("->")) {
            bool arrow = is("->");
            advance();
            if (arrow) n = mk_un(ND_DEREF, n);
            add_type(n);
            type *t = N(n)->ty;
            if (t->kind != T_STRUCT) { fail("only a struct has members", C.cur.text); return n; }
            if (C.cur.kind != TK_IDENT) { fail("a member's name was expected", C.cur.text); return n; }
            i64 found = -1;
            for (u32 i = 0; i < t->mcount; i++)
                if (strcmp(C.members[t->mfirst + i].name, C.cur.text) == 0) { found = (i64)(t->mfirst + i); break; }
            if (found < 0) { fail("the struct has no member called", C.cur.text); return n; }
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

static u32 cast_expr(void)
{
    if (is("(") && C.nxt.kind == TK_IDENT) {
        /* A type in parentheses is a cast; a name is not. Only the
         * next token is known here, so a typedef name or a keyword
         * decides it. */
        const char *w = C.nxt.text;
        bool typeish = typedef_find(w) != NULL;
        static const char *const kws[] = { "void", "char", "int", "long", "short",
            "unsigned", "signed", "struct", "enum", "const", "_Bool", "bool", NULL };
        for (u32 i = 0; kws[i] && !typeish; i++) if (strcmp(w, kws[i]) == 0) typeish = true;
        if (typeish) {
            advance();
            bool td;
            type *t = declspec(&td);
            char nm[NAME_MAX];
            t = declarator(t, nm);
            expect(")");
            u32 n = mk_un(ND_CAST, cast_expr());
            N(n)->ty = t;
            return n;
        }
    }
    return unary();
}

static u32 binary(u32 level);

/* Precedence, from loose to tight. */
typedef struct { const char *op; u8 nd; bool swap; } binop;
static const binop levels[][5] = {
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
        const binop *hit = NULL;
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

/* A number the compiler can work out itself: enum values, array
 * sizes, case labels, what a global starts as. */
static i64 const_eval(u32 i)
{
    if (!i || C.bad) return 0;
    node *n = N(i);
    switch (n->kind) {
    case ND_NUM: return n->val;
    case ND_NEG: return -const_eval(n->lhs);
    case ND_BITNOT: return ~const_eval(n->lhs);
    case ND_NOT: return !const_eval(n->lhs);
    case ND_CAST: return const_eval(n->lhs);
    case ND_ADD: return const_eval(n->lhs) + const_eval(n->rhs);
    case ND_SUB: return const_eval(n->lhs) - const_eval(n->rhs);
    case ND_MUL: return const_eval(n->lhs) * const_eval(n->rhs);
    case ND_DIV: { i64 d = const_eval(n->rhs); return d ? const_eval(n->lhs) / d : 0; }
    case ND_MOD: { i64 d = const_eval(n->rhs); return d ? const_eval(n->lhs) % d : 0; }
    case ND_AND: return const_eval(n->lhs) & const_eval(n->rhs);
    case ND_OR:  return const_eval(n->lhs) | const_eval(n->rhs);
    case ND_XOR: return const_eval(n->lhs) ^ const_eval(n->rhs);
    case ND_SHL: return const_eval(n->lhs) << const_eval(n->rhs);
    case ND_SHR: return const_eval(n->lhs) >> const_eval(n->rhs);
    case ND_EQ:  return const_eval(n->lhs) == const_eval(n->rhs);
    case ND_NE:  return const_eval(n->lhs) != const_eval(n->rhs);
    case ND_LT:  return const_eval(n->lhs) < const_eval(n->rhs);
    case ND_LE:  return const_eval(n->lhs) <= const_eval(n->rhs);
    case ND_COND: return const_eval(n->lhs) ? const_eval(n->rhs) : const_eval(n->third);
    default:
        fail_at(n->line, NULL, "that has to be a number the compiler can work out", NULL);
        return 0;
    }
}

/* ------------------------------------------------------------------ */
/* Reading statements                                                  */
/* ------------------------------------------------------------------ */

static u32 stmt(void);

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

/* A local declaration: room below rbp, and an assignment for what
 * it starts as. Several declarators become a block of them. */
static u32 local_decl(void)
{
    bool td;
    type *base = declspec(&td);
    u32 b = mk(ND_BLOCK);
    u32 tail = 0;
    for (;;) {
        char nm[NAME_MAX];
        type *t = declarator(base, nm);
        if (C.bad) return b;
        if (td) {
            if (C.ntypedefs < NTYPEDEFS) { cpy(C.typedefs[C.ntypedefs].name, nm); C.typedefs[C.ntypedefs].ty = t; C.ntypedefs++; }
        } else if (nm[0]) {
            if (t->kind == T_VOID) { fail("a variable cannot be void", nm); return b; }
            sym *s = sym_add(nm, t, S_LOCAL);
            if (eat("=")) {
                if (t->kind == T_ARR && C.cur.kind == TK_STR && t->base->kind == T_CHAR) {
                    /* char name[] = "..." : the letters, one by one. */
                    u32 sidx = C.cur.str;
                    advance();
                    if (t->len == 0) { t->len = C.strs[sidx].len; t->size = (u32)t->len; }
                    s->val = local_slot(t);
                    for (u32 k = 0; k < C.strs[sidx].len && k < t->len; k++) {
                        u32 v = mk(ND_VAR); N(v)->sym = (u32)(s - C.syms);
                        u32 el = mk_un(ND_DEREF, mk_bin(ND_ADD, v, mk_num(k)));
                        u32 as = mk_bin(ND_ASSIGN, el, mk_num(C.spool[C.strs[sidx].at + k]));
                        u32 st = mk_un(ND_EXPR, as);
                        if (tail) N(tail)->next = st; else N(b)->aux = st;
                        tail = st;
                    }
                } else {
                    s->val = local_slot(t);
                    u32 v = mk(ND_VAR); N(v)->sym = (u32)(s - C.syms);
                    u32 as = mk_bin(ND_ASSIGN, v, assign());
                    u32 st = mk_un(ND_EXPR, as);
                    if (tail) N(tail)->next = st; else N(b)->aux = st;
                    tail = st;
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
    if (is_kw("switch")) {
        advance();
        expect("(");
        u32 n = mk(ND_SWITCH);
        N(n)->lhs = expr();
        expect(")");
        if (C.nsw >= 32) { fail("switches nest too deep", NULL); return n; }
        C.sw_case[C.nsw++] = 0;
        N(n)->rhs = stmt();
        N(n)->aux = C.sw_case[--C.nsw];          /* the chain of cases */
        return n;
    }
    if (is_kw("case") || is_kw("default")) {
        bool dflt = is_kw("default");
        advance();
        u32 n = mk(dflt ? ND_DEFAULT : ND_CASE);
        if (!dflt) N(n)->val = const_eval(assign());
        expect(":");
        if (!C.nsw) { fail("case outside a switch", NULL); return n; }
        N(n)->third = C.sw_case[C.nsw - 1];      /* chain through the switch */
        C.sw_case[C.nsw - 1] = n;
        N(n)->aux = C.label++;                   /* its label, chosen now */
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
    if (C.len + 1 >= C.max) { if (!C.bad) fail_at(0, NULL, "the assembly grew larger than the room for it", NULL); return; }
    C.out[C.len++] = c;
}

static void o_str(const char *s) { while (*s) o_char(*s++); }

static void o_dec(i64 v)
{
    if (v < 0) { o_char('-'); v = -v; }
    char d[24];
    u32 n = 0;
    u64 u = (u64)v;
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

static const char *const arg64[NPARAMS] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
static const char *const arg32[NPARAMS] = { "edi", "esi", "edx", "ecx", "r8d", "r9d" };
static const char *const arg8[NPARAMS]  = { "dil", "sil", "dl", "cl", "r8b", "r9b" };

static void gen_expr(u32 i);
static void gen_stmt(u32 i);

/* The address of an lvalue into rax. */
static void gen_addr(u32 i)
{
    node *n = N(i);
    switch (n->kind) {
    case ND_VAR: {
        sym *s = &C.syms[n->sym];
        if (s->kind == S_LOCAL) o("    lea rax, [rbp - %d]\n", -s->val);
        else if (s->kind == S_FUNC) o("    lea rax, [f_%s]\n", s->name);
        else o("    lea rax, [v_%s]\n", s->name);
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
    default:
        fail_at(n->line, NULL, "that is not something with an address", NULL);
    }
}

/* What is at the address in rax, into rax -- unless it is an array,
 * a struct or a function, whose address is the value. */
static void load(type *t)
{
    if (t->kind == T_ARR || t->kind == T_STRUCT || t->kind == T_FUNC) return;
    if (t->size == 1) o(t->uns ? "    movzx rax, byte [rax]\n" : "    movsx rax, byte [rax]\n");
    else if (t->size == 4) o(t->uns ? "    mov eax, dword [rax]\n" : "    movsxd rax, dword [rax]\n");
    else o("    mov rax, [rax]\n");
}

/* rax into the address in rdi. */
static void store(type *t)
{
    if (t->kind == T_STRUCT) {
        u32 off = 0;
        while (off + 8 <= t->size) { o("    mov rcx, [rax + %d]\n    mov [rdi + %d], rcx\n", (i64)off, (i64)off); off += 8; }
        while (off < t->size) { o("    mov cl, byte [rax + %d]\n    mov byte [rdi + %d], cl\n", (i64)off, (i64)off); off++; }
        return;
    }
    if (t->size == 1) o("    mov byte [rdi], al\n");
    else if (t->size == 4) o("    mov dword [rdi], eax\n");
    else o("    mov [rdi], rax\n");
}

/* rax, cut to a type's width and widened again. */
static void cast_to(type *from, type *to)
{
    if (to->kind == T_VOID || to->kind == T_STRUCT) return;
    if (to->size == 1) o(to->uns ? "    movzx rax, al\n" : "    movsx rax, al\n");
    else if (to->size == 4) o(to->uns ? "    mov eax, eax\n" : "    movsxd rax, eax\n");
    (void)from;
}

static void emit_binop(u8 kind, type *t)
{
    bool uns = t && t->uns;
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

static void gen_args(u32 first, u32 count, const char *const *regs)
{
    for (u32 a = first; a; a = N(a)->next) {
        gen_expr(a);
        o("    push rax\n");
    }
    for (u32 k = count; k > 0; k--) o("    pop %s\n", regs[k - 1]);
}

static void gen_expr(u32 i)
{
    if (!i || C.bad) return;
    node *n = N(i);
    switch (n->kind) {
    case ND_NUM:
        o("    mov rax, %d\n", n->val);
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
    case ND_MEMBER:
        gen_addr(i);
        load(n->ty);
        return;
    case ND_NEG:
        gen_expr(n->lhs);
        o("    neg rax\n");
        return;
    case ND_BITNOT:
        gen_expr(n->lhs);
        o("    not rax\n");
        return;
    case ND_NOT:
        gen_expr(n->lhs);
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
    case ND_LAND: {
        u32 l = C.label++;
        gen_expr(n->lhs);
        o("    test rax, rax\n    je %l\n", l);
        gen_expr(n->rhs);
        o("    test rax, rax\n    je %l\n    mov rax, 1\n    jmp %l\n%l:\n    mov rax, 0\n%l:\n", l, l + 1, l, l + 1);
        C.label++;
        return;
    }
    case ND_LOR: {
        u32 l = C.label++;
        gen_expr(n->lhs);
        o("    test rax, rax\n    jne %l\n", l);
        gen_expr(n->rhs);
        o("    test rax, rax\n    jne %l\n    mov rax, 0\n    jmp %l\n%l:\n    mov rax, 1\n%l:\n", l, l + 1, l, l + 1);
        C.label++;
        return;
    }
    case ND_COND: {
        u32 l = C.label;
        C.label += 2;
        gen_expr(n->lhs);
        o("    test rax, rax\n    je %l\n", l);
        gen_expr(n->rhs);
        o("    jmp %l\n%l:\n", l + 1, l);
        gen_expr(n->third);
        o("%l:\n", l + 1);
        return;
    }
    case ND_ASSIGN: {
        type *t = N(n->lhs)->ty;
        gen_addr(n->lhs);
        o("    push rax\n");
        gen_expr(n->rhs);
        if (n->op) {
            o("    mov rdi, rax\n    pop rax\n    mov r8, rax\n");
            load(t);
            /* A pointer moves by its element. */
            if (is_ptr(t) && (n->op == ND_ADD || n->op == ND_SUB))
                o("    imul rdi, %d\n", (i64)t->base->size);
            emit_binop(n->op, t);
            o("    mov rdi, r8\n");
        } else {
            o("    pop rdi\n");
        }
        cast_to(N(n->rhs)->ty, t);
        store(t);
        return;
    }
    case ND_INCDEC: {
        type *t = N(n->lhs)->ty;
        i64 step = n->val;
        if (is_ptr(t)) step *= (i64)t->base->size;
        gen_addr(n->lhs);
        o("    mov rdi, rax\n");
        load(t);
        if (n->post) o("    mov r8, rax\n");
        o("    add rax, %d\n", step);
        o("    push rdi\n");
        cast_to(t, t);
        o("    pop rdi\n");
        store(t);
        if (n->post) o("    mov rax, r8\n");
        return;
    }
    case ND_CALL: {
        if (n->lhs) { gen_expr(n->lhs); o("    push rax\n"); }
        gen_args(n->aux, (u32)n->val, arg64);
        if (n->lhs) o("    pop rax\n    call rax\n");
        else o("    call f_%s\n", C.syms[n->sym].name);
        cast_to(n->ty, n->ty);
        return;
    }
    case ND_SYSCALL: {
        static const char *const sregs[6] = { "rax", "rdi", "rsi", "rdx", "r10", "r8" };
        gen_args(n->aux, (u32)n->val, sregs);
        o("    syscall\n");
        return;
    }
    default:
        break;
    }

    /* The binary ones: left in rax, right in rdi. */
    gen_expr(n->lhs);
    o("    push rax\n");
    gen_expr(n->rhs);
    o("    mov rdi, rax\n    pop rax\n");
    type *ot = N(n->lhs)->ty;
    if (n->kind == ND_EQ || n->kind == ND_NE || n->kind == ND_LT || n->kind == ND_LE)
        emit_binop(n->kind, (ot->uns || N(n->rhs)->ty->uns || is_ptr(ot)) ? C.t_ulong : C.t_long);
    else
        emit_binop(n->kind, n->ty);
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
            cast_to(N(n->lhs)->ty, C.ret_ty);
        }
        o("    jmp .Lret%d\n", (i64)C.fn_no);
        return;
    case ND_IF: {
        u32 l = C.label;
        C.label += 2;
        gen_expr(n->lhs);
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
        if (n->rhs) { gen_expr(n->rhs); o("    test rax, rax\n    je %l\n", l + 2); }
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
    case ND_SWITCH: {
        u32 end = C.label++;
        C.brk[C.nbrk++] = end;
        gen_expr(n->lhs);
        u32 dflt = 0;
        for (u32 c = n->aux; c; c = N(c)->third) {
            if (N(c)->kind == ND_DEFAULT) { dflt = c; continue; }
            o("    cmp rax, %d\n    je %l\n", N(c)->val, N(c)->aux);
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
    default:
        gen_expr(i);
        return;
    }
}

/* ------------------------------------------------------------------ */
/* The top: functions and globals                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    char  name[NAME_MAX];
    type *ty;
    bool  has_init;
    i64   init;
    u32   str;                        /* char array from a string, or NSTR */
} global;

static global globals[256];
static u32 nglobals;

static void function(type *ft, const char *name, char pnames[NPARAMS][NAME_MAX])
{
    sym *fs = sym_find(name);
    if (fs && fs->kind == S_FUNC && fs->defined) { fail("that function already has a body", name); return; }
    if (!fs) fs = sym_add(name, ft, S_FUNC);
    fs->ty = ft;
    fs->defined = true;

    C.nnodes = 1;
    C.frame = 0;
    C.ret_ty = ft->base;
    C.fn_no++;
    C.nbrk = C.ncont = C.nsw = 0;

    scope_in();
    sym *params[NPARAMS];
    for (u32 p = 0; p < ft->nparams; p++) {
        params[p] = sym_add(pnames[p], ft->params[p], S_LOCAL);
        params[p]->val = local_slot(ft->params[p]);
    }
    expect("{");
    u32 body = block();
    scope_out();
    if (C.bad) return;
    add_type(body);
    if (C.bad) return;

    u32 frame = (u32)((C.frame + 15) / 16 * 16);
    o("\nf_%s:\n    push rbp\n    mov rbp, rsp\n", name);
    if (frame) o("    sub rsp, %d\n", (i64)frame);
    for (u32 p = 0; p < ft->nparams; p++) {
        type *pt = ft->params[p];
        if (pt->size == 1) o("    mov byte [rbp - %d], %s\n", -params[p]->val, arg8[p]);
        else if (pt->size == 4) o("    mov dword [rbp - %d], %s\n", -params[p]->val, arg32[p]);
        else o("    mov [rbp - %d], %s\n", -params[p]->val, arg64[p]);
    }
    gen_stmt(body);
    o(".Lret%d:\n    mov rsp, rbp\n    pop rbp\n    ret\n", (i64)C.fn_no);
}

static void toplevel(void)
{
    bool td;
    type *base = declspec(&td);
    if (C.bad) return;
    if (eat(";")) return;                          /* struct s { ... }; alone */

    for (;;) {
        char nm[NAME_MAX];
        char pnames[NPARAMS][NAME_MAX];
        memset(pnames, 0, sizeof(pnames));

        /* A function's declarator keeps its parameter names, which a
         * plain declarator throws away. */
        type *t;
        if (C.cur.kind == TK_IDENT && !is_typename() && C.nxt.kind == TK_PUNCT &&
            strcmp(C.nxt.text, "(") == 0 && !td) {
            type *b = base;
            cpy(nm, C.cur.text);
            advance();
            advance();
            t = func_suffix(b, pnames);
        } else {
            type *b = base;
            while (is("*")) { advance(); b = ptr_to(b); }
            if (C.cur.kind == TK_IDENT && !is_typename() && C.nxt.kind == TK_PUNCT &&
                strcmp(C.nxt.text, "(") == 0 && !td) {
                cpy(nm, C.cur.text);
                advance();
                advance();
                t = func_suffix(b, pnames);
            } else {
                t = declarator(b, nm);
            }
        }
        if (C.bad) return;

        if (td) {
            if (C.ntypedefs < NTYPEDEFS) { cpy(C.typedefs[C.ntypedefs].name, nm); C.typedefs[C.ntypedefs].ty = t; C.ntypedefs++; }
        } else if (t->kind == T_FUNC) {
            if (is("{")) { function(t, nm, pnames); return; }
            sym *fs = sym_find(nm);
            if (!fs) fs = sym_add(nm, t, S_FUNC);
        } else if (nm[0]) {
            if (nglobals >= 256) { fail("too many globals", NULL); return; }
            global *g = &globals[nglobals];
            memset(g, 0, sizeof(*g));
            cpy(g->name, nm);
            g->ty = t;
            g->str = NSTR;
            if (eat("=")) {
                if (t->kind == T_ARR && C.cur.kind == TK_STR && t->base->kind == T_CHAR) {
                    g->str = C.cur.str;
                    if (t->len == 0) { t->len = C.strs[g->str].len; t->size = (u32)t->len; }
                    advance();
                } else {
                    C.nnodes = 1;
                    g->has_init = true;
                    g->init = const_eval(assign());
                }
            }
            if (t->kind == T_ARR && t->len == 0) { fail("an array without a size needs a value", nm); return; }
            sym_add(nm, t, S_GLOBAL);
            nglobals++;
        }
        if (!eat(",")) break;
    }
    expect(";");
}

/* ------------------------------------------------------------------ */

i64 cc_compile(const u8 *src, u64 len, const char *src_name,
               cc_find_fn find, void *ctx,
               char *out, u64 max, char *err, u32 errmax)
{
    memset(&C, 0, sizeof(C));
    nglobals = 0;
    C.find = find;
    C.ctx = ctx;
    C.out = out;
    C.max = max;
    C.err = err;
    C.errmax = errmax;
    if (errmax) err[0] = 0;
    C.nnodes = 1;
    C.label = 1;

    C.t_void  = new_type(T_VOID, 1, 1);
    C.t_char  = new_type(T_CHAR, 1, 1);
    C.t_uchar = new_type(T_CHAR, 1, 1); C.t_uchar->uns = true;
    C.t_int   = new_type(T_INT, 4, 4);
    C.t_uint  = new_type(T_INT, 4, 4);  C.t_uint->uns = true;
    C.t_long  = new_type(T_LONG, 8, 8);
    C.t_ulong = new_type(T_LONG, 8, 8); C.t_ulong->uns = true;

    /* Index zero means "no name" in a node, so no name may live there. */
    sym_add("", C.t_void, S_ENUM);

    if (!push_source(src, len, src_name ? src_name : "the text", false)) return -1;
    lex_raw(&C.nxt);
    advance();

    /* The way in: the two handles a program starts holding become
     * main's two arguments, and main's answer is the exit code. */
    o("; made by the compiler; the source lies beside this\n");
    o("section code\n");
    o("    mov rbp, rsp\n    call f_main\n    mov rdi, rax\n    mov rax, 0\n    syscall\n");

    while (C.cur.kind != TK_EOF && !C.bad) toplevel();
    if (C.bad) return -1;

    sym *m = sym_find("main");
    if (!m || m->kind != S_FUNC || !m->defined) { fail_at(0, src_name, "there is no main", NULL); return -1; }

    o("\nsection data\n");
    for (u32 i = 0; i < nglobals; i++) {
        global *g = &globals[i];
        o("    align %d\n", (i64)(g->ty->align ? g->ty->align : 1));
        o("v_%s:", g->name);
        if (g->str < NSTR) {
            o(" db");
            for (u32 k = 0; k < g->ty->len; k++)
                o("%s %d", k ? "," : "", (i64)(k < C.strs[g->str].len ? C.spool[C.strs[g->str].at + k] : 0));
            o("\n");
        } else if (g->has_init) {
            if (g->ty->size == 1) o(" db %d\n", g->init & 0xFF);
            else if (g->ty->size == 4) o(" dd %d\n", g->init & 0xFFFFFFFF);
            else o(" dq %d\n", g->init);
        } else {
            o(" res %d\n", (i64)(g->ty->size ? g->ty->size : 1));
        }
    }
    for (u32 i = 0; i < C.nstrs; i++) {
        o(".Ls%d: db", (i64)i);
        for (u32 k = 0; k < C.strs[i].len; k++)
            o("%s %d", k ? "," : "", (i64)C.spool[C.strs[i].at + k]);
        o("\n");
    }
    if (C.bad) return -1;
    return (i64)C.len;
}
