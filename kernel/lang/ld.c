/*
 * ld.c -- the linker.
 *
 * Every object brings its sections, its names and the places that
 * want a name's address. The linker lays the sections one after
 * another -- all the text, then the read-only data, then the data,
 * then the room without values -- gives every public name one
 * address, looks each wanted name up, and writes the addresses in.
 * A name laid down twice, or wanted and laid down by nobody, stops
 * the link with both parties named.
 *
 * Two shapes: the image a program is (asm.h describes its head), and
 * the ELF the boot loader reads for the kernel, with the layout the
 * kernel's own linker script had: linked at -2 GiB, loaded at 2 MiB,
 * three loadable segments, and the names __kernel_start and its kin
 * that the kernel asks the layout for.
 */
#include <eb/ld.h>
#include <eb/asm.h>
#include <eb/lang.h>
#include <eb/string.h>

#define UNITS_MAX  128
#define GSYMS_MAX  65536
#define HASH_SIZE  65536
#define PAGE       4096ULL
#define KERNEL_VMA 0xFFFFFFFF80000000ULL
#define KERNEL_LMA 0x200000ULL
#define OBJ_MAX    (4u * 1024 * 1024)

typedef struct {
    const u8   *d;
    u64         n;
    const char *name;
    u32         nsym, nrel, strn;
    u32         size[ASM_NSEC], laid[ASM_NSEC], align[ASM_NSEC];
    const u8   *bytes[ASM_NSEC];
    const u8   *syms, *rels;
    const char *str;
    u64         base[ASM_NSEC];       /* where each section lands */
} unit;

typedef struct { const char *name; u64 addr; u32 unit; u32 next; } gsym;

static unit *units;
static gsym *gsyms;
static u32  *hash;
static u32   ngsyms;
static u32   last_n;                  /* how many units the last link took */

static char *errp;
static u32   errmax;
static bool  bad;

static u32 rd32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static u64 rd64(const u8 *p) { return (u64)rd32(p) | ((u64)rd32(p + 4) << 32); }
static void wr32(u8 *p, u32 v) { for (u32 i = 0; i < 4; i++) p[i] = (u8)(v >> (i * 8)); }
static void wr64(u8 *p, u64 v) { for (u32 i = 0; i < 8; i++) p[i] = (u8)(v >> (i * 8)); }
static void wr16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }

/* One line, from up to four pieces. */
static void fail(const char *a, const char *b, const char *c, const char *d)
{
    if (bad) return;
    bad = true;
    if (!errmax) return;
    const char *parts[4] = { a, b, c, d };
    u32 at = 0;
    for (u32 k = 0; k < 4; k++) {
        const char *p = parts[k];
        if (!p) continue;
        while (*p && at < errmax - 1) errp[at++] = *p++;
    }
    errp[at] = 0;
}

static u64 align_up(u64 v, u64 a) { return (v + a - 1) & ~(a - 1); }

/* ------------------------------------------------------------------ */
/* Reading an object                                                   */
/* ------------------------------------------------------------------ */

static bool parse_unit(unit *u, const u8 *d, u64 n, const char *name)
{
    memset(u, 0, sizeof(*u));
    u->d = d; u->n = n; u->name = name;
    if (!d || n < 76 || d[0] != 'E' || d[1] != 'B' || d[2] != 'O' || d[3] != '1') return false;
    u->nsym = rd32(d + 4);
    u->nrel = rd32(d + 8);
    u->strn = rd32(d + 12);
    u64 total = 76;
    for (u32 s = 0; s < ASM_NSEC; s++) {
        u->size[s] = rd32(d + 16 + s * 4);
        u->laid[s] = rd32(d + 36 + s * 4);
        u->align[s] = rd32(d + 56 + s * 4);
        if (u->align[s] < 16 || (u->align[s] & (u->align[s] - 1))) u->align[s] = 16;
        if (u->laid[s] > u->size[s]) return false;
        total += u->laid[s];
    }
    if (u->laid[ASM_SEC_BSS]) return false;
    total += (u64)u->nsym * 24 + (u64)u->nrel * 24 + u->strn;
    if (total != n) return false;
    const u8 *p = d + 76;
    for (u32 s = 0; s < ASM_NSEC; s++) { u->bytes[s] = p; p += u->laid[s]; }
    u->syms = p; p += (u64)u->nsym * 24;
    u->rels = p; p += (u64)u->nrel * 24;
    u->str = (const char *)p;
    if (u->strn == 0 || u->str[u->strn - 1] != 0) return u->strn == 0;
    for (u32 i = 0; i < u->nsym; i++) if (rd32(u->syms + i * 24) >= u->strn) return false;
    for (u32 i = 0; i < u->nrel; i++) {
        const u8 *r = u->rels + i * 24;
        if (rd32(r) >= ASM_NSEC || rd32(r + 8) >= u->nsym) return false;
    }
    return true;
}

static const char *sym_name(const unit *u, u32 i)  { return u->str + rd32(u->syms + i * 24); }
static i32         sym_sec(const unit *u, u32 i)   { return (i32)rd32(u->syms + i * 24 + 4); }
static u64         sym_off(const unit *u, u32 i)   { return rd64(u->syms + i * 24 + 8); }
static u32         sym_flags(const unit *u, u32 i) { return rd32(u->syms + i * 24 + 16); }

bool ld_object_ok(const u8 *d, u64 n)
{
    unit u;
    return parse_unit(&u, d, n, "");
}

bool ld_object_defines(const u8 *d, u64 n, const char *name)
{
    unit u;
    if (!parse_unit(&u, d, n, "")) return false;
    for (u32 i = 0; i < u.nsym; i++)
        if ((sym_flags(&u, i) & 2) && strcmp(sym_name(&u, i), name) == 0) return true;
    return false;
}

u32 ld_object_wants(const u8 *d, u64 n, char *out, u32 max)
{
    unit u;
    u32 count = 0, at = 0;
    if (max) out[0] = 0;
    if (!parse_unit(&u, d, n, "")) return 0;
    for (u32 i = 0; i < u.nsym; i++) {
        if (sym_flags(&u, i) & 2) continue;
        count++;
        if (!max) continue;
        const char *nm = sym_name(&u, i);
        u32 need = (u32)strlen(nm) + (at ? 2 : 0);
        if (at + need + 4 >= max) { if (at + 4 < max) { out[at++] = '.'; out[at++] = '.'; out[at++] = '.'; out[at] = 0; } max = 0; continue; }
        if (at) { out[at++] = ','; out[at++] = ' '; }
        while (*nm) out[at++] = *nm++;
        out[at] = 0;
    }
    return count;
}

/* ------------------------------------------------------------------ */
/* Names across the units                                              */
/* ------------------------------------------------------------------ */

static u32 hash_of(const char *s)
{
    u32 h = 2166136261u;
    while (*s) { h ^= (u8)*s++; h *= 16777619u; }
    return h & (HASH_SIZE - 1);
}

static gsym *global_find(const char *name)
{
    for (u32 i = hash[hash_of(name)]; i != 0xFFFFFFFF; i = gsyms[i].next)
        if (strcmp(gsyms[i].name, name) == 0) return &gsyms[i];
    return NULL;
}

static bool global_add(const char *name, u64 addr, u32 ui)
{
    gsym *old = global_find(name);
    if (old) {
        fail(name, " is laid down twice: in ", units[old->unit].name, NULL);
        if (errmax) {
            /* and in whom else: room permitting */
            u32 at = (u32)strlen(errp);
            const char *more = " and in ";
            while (*more && at < errmax - 1) errp[at++] = *more++;
            const char *n2 = units[ui].name;
            while (*n2 && at < errmax - 1) errp[at++] = *n2++;
            errp[at] = 0;
        }
        return false;
    }
    if (ngsyms >= GSYMS_MAX) { fail("too many names in all", NULL, NULL, NULL); return false; }
    gsym *g = &gsyms[ngsyms];
    g->name = name; g->addr = addr; g->unit = ui;
    u32 h = hash_of(name);
    g->next = hash[h];
    hash[h] = ngsyms++;
    return true;
}

/* ------------------------------------------------------------------ */
/* The layout                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    u32 layout;
    u64 code_start, code_end;          /* program: text+rodata+user */
    u64 data_start, data_laid_end, data_end;
    u64 text_start, text_end;          /* kernel */
    u64 rodata_start, user_start, user_end, rodata_end;
    u64 bss_start, bss_end;
    u64 names_start, names_end;        /* kernel: the table of the code's names */
    u64 seg_vaddr[3], seg_off[3], seg_filesz[3], seg_memsz[3];
    u32 seg_of[ASM_NSEC];              /* which segment a section is in, 3 = none */
    u64 entry;
} plan;

/* The names of the code, for the kernel's crash report to read an
 * address back: every defined name in text or user that is not a
 * text's own mark. The table's shape is the one kernel/lib/names.c
 * reads and tools/mknames.py writes for the outside build. */
#define NAMES_MAX 65536
static u32 names_bytes;

static bool names_worthy(const unit *u, u32 s)
{
    if ((sym_flags(u, s) & 2) == 0) return false;
    i32 sec = sym_sec(u, s);
    if (sec != ASM_SEC_TEXT && sec != ASM_SEC_USER) return false;
    return sym_name(u, s)[0] != '.';
}

static void names_measure(u32 n)
{
    u32 count = 0;
    u64 letters = 0;
    for (u32 i = 0; i < n; i++)
        for (u32 s = 0; s < units[i].nsym && count < NAMES_MAX; s++)
            if (names_worthy(&units[i], s)) { count++; letters += strlen(sym_name(&units[i], s)) + 1; }
    names_bytes = count ? (u32)align_up(8 + (u64)count * 16 + letters, 8) : 0;
}

static void lay_section(u32 n, u32 s, u64 *at)
{
    for (u32 i = 0; i < n; i++) {
        unit *u = &units[i];
        if (u->size[s] == 0) { u->base[s] = *at; continue; }
        *at = align_up(*at, u->align[s]);
        u->base[s] = *at;
        *at += u->size[s];
    }
}

static u64 laid_end(u32 n, u32 s, u64 from)
{
    u64 end = from;
    for (u32 i = 0; i < n; i++)
        if (units[i].laid[s] && units[i].base[s] + units[i].laid[s] > end) end = units[i].base[s] + units[i].laid[s];
    return end;
}

static void plan_program(plan *p, u32 n)
{
    u64 at = USER_LOAD_CODE;
    lay_section(n, ASM_SEC_TEXT, &at);
    lay_section(n, ASM_SEC_RODATA, &at);
    lay_section(n, ASM_SEC_USER, &at);
    p->code_start = USER_LOAD_CODE;
    p->code_end = at;
    at = USER_LOAD_DATA;
    lay_section(n, ASM_SEC_DATA, &at);
    p->data_laid_end = laid_end(n, ASM_SEC_DATA, USER_LOAD_DATA);
    lay_section(n, ASM_SEC_BSS, &at);
    p->data_start = USER_LOAD_DATA;
    p->data_end = at;

    p->seg_vaddr[0] = p->code_start; p->seg_off[0] = 20;
    p->seg_filesz[0] = p->code_end - p->code_start;
    p->seg_vaddr[1] = p->data_start; p->seg_off[1] = 20 + p->seg_filesz[0];
    p->seg_filesz[1] = p->data_laid_end - p->data_start;
    p->seg_of[ASM_SEC_TEXT] = 0; p->seg_of[ASM_SEC_RODATA] = 0; p->seg_of[ASM_SEC_USER] = 0;
    p->seg_of[ASM_SEC_DATA] = 1; p->seg_of[ASM_SEC_BSS] = 3;
}

static void plan_kernel(plan *p, u32 n)
{
    u64 at = KERNEL_VMA + KERNEL_LMA;
    p->text_start = at;
    lay_section(n, ASM_SEC_TEXT, &at);
    at = align_up(at, PAGE);
    p->text_end = at;

    p->rodata_start = at;
    lay_section(n, ASM_SEC_RODATA, &at);
    at = align_up(at, PAGE);
    p->user_start = at;
    lay_section(n, ASM_SEC_USER, &at);
    at = align_up(at, PAGE);
    p->user_end = at;
    p->rodata_end = at;

    p->data_start = at;
    lay_section(n, ASM_SEC_DATA, &at);
    p->data_laid_end = laid_end(n, ASM_SEC_DATA, p->data_start);
    /* the names, after the data and before the bss, where the outside
     * build's linker script keeps them too */
    at = align_up(at, 8);
    p->names_start = at;
    at += names_bytes;
    p->names_end = at;
    if (names_bytes && p->names_end > p->data_laid_end) p->data_laid_end = p->names_end;
    at = align_up(at, 16);
    p->bss_start = at;
    lay_section(n, ASM_SEC_BSS, &at);
    at = align_up(at, PAGE);
    p->bss_end = at;
    p->data_end = at;

    u64 off = 0x1000;
    p->seg_vaddr[0] = p->text_start; p->seg_off[0] = off;
    p->seg_filesz[0] = p->text_end - p->text_start; p->seg_memsz[0] = p->seg_filesz[0];
    off = align_up(off + p->seg_filesz[0], PAGE);
    p->seg_vaddr[1] = p->rodata_start; p->seg_off[1] = off;
    p->seg_filesz[1] = p->rodata_end - p->rodata_start; p->seg_memsz[1] = p->seg_filesz[1];
    off = align_up(off + p->seg_filesz[1], PAGE);
    p->seg_vaddr[2] = p->data_start; p->seg_off[2] = off;
    p->seg_filesz[2] = p->data_laid_end - p->data_start; p->seg_memsz[2] = p->data_end - p->data_start;
    p->seg_of[ASM_SEC_TEXT] = 0; p->seg_of[ASM_SEC_RODATA] = 1; p->seg_of[ASM_SEC_USER] = 1;
    p->seg_of[ASM_SEC_DATA] = 2; p->seg_of[ASM_SEC_BSS] = 3;
}

/* The names the kernel's layout provides. */
static bool layout_symbol(const plan *p, const char *name, u64 *v)
{
    if (p->layout != LD_KERNEL) return false;
    struct { const char *nm; u64 val; } t[] = {
        { "__kernel_start", p->text_start }, { "__text_end", p->text_end },
        { "__rodata_start", p->rodata_start }, { "__user_start", p->user_start },
        { "__user_end", p->user_end }, { "__rodata_end", p->rodata_end },
        { "__data_start", p->data_start }, { "__bss_start", p->bss_start },
        { "__bss_end", p->bss_end }, { "__kernel_end", p->bss_end },
        { "__names_start", p->names_start }, { "__names_end", p->names_end },
        { "__kernel_phys", KERNEL_LMA } };
    for (u32 i = 0; i < sizeof(t) / sizeof(t[0]); i++)
        if (strcmp(t[i].nm, name) == 0) { *v = t[i].val; return true; }
    return false;
}

/* ------------------------------------------------------------------ */
/* Writing                                                             */
/* ------------------------------------------------------------------ */

/* Where in out the byte at address addr of section s lies. */
static u8 *place(u8 *out, const plan *p, u32 s, u64 addr)
{
    u32 seg = p->seg_of[s];
    if (seg > 2) return NULL;
    return out + p->seg_off[seg] + (addr - p->seg_vaddr[seg]);
}

static bool resolve(const plan *p, unit *u, u32 si, u64 *addr)
{
    u32 fl = sym_flags(u, si);
    if (fl & 2) {
        i32 sec = sym_sec(u, si);
        if (sec < 0 || sec >= (i32)ASM_NSEC) return false;
        *addr = u->base[sec] + sym_off(u, si);
        return true;
    }
    const char *nm = sym_name(u, si);
    gsym *g = global_find(nm);
    if (g) { *addr = g->addr; return true; }
    if (layout_symbol(p, nm, addr)) return true;
    fail(u->name, " wants ", nm, ", and no text lays it down");
    return false;
}

static bool apply(u8 *out, const plan *p, u32 n)
{
    for (u32 i = 0; i < n && !bad; i++) {
        unit *u = &units[i];
        for (u32 r = 0; r < u->nrel; r++) {
            const u8 *rr = u->rels + r * 24;
            u32 sec = rd32(rr), off = rd32(rr + 4), si = rd32(rr + 8), kind = rd32(rr + 12);
            i64 addend = (i64)rd64(rr + 16);
            if (off + (kind == ASM_R_ABS64 ? 8 : 4) > u->laid[sec]) { fail(u->name, ": a place that wants a name lies past its bytes", NULL, NULL); return false; }
            u64 P = u->base[sec] + off;
            u64 S;
            if (!resolve(p, u, si, &S)) return false;
            u8 *at = place(out, p, sec, P);
            if (!at) { fail(u->name, ": a place that wants a name lies where nothing is written", NULL, NULL); return false; }
            if (kind == ASM_R_REL32) {
                i64 v = (i64)(S + (u64)addend - P);
                if (v < -2147483648LL || v > 2147483647LL) { fail(u->name, ": ", sym_name(u, si), " is too far away for a relative reference"); return false; }
                wr32(at, (u32)(i32)v);
            } else if (kind == ASM_R_ABS64) {
                wr64(at, S + (u64)addend);
            } else if (kind == ASM_R_ABS32S) {
                i64 v = (i64)(S + (u64)addend);
                if (v < -2147483648LL || v > 2147483647LL) { fail(u->name, ": ", sym_name(u, si), "'s address does not fit a doubleword"); return false; }
                wr32(at, (u32)(i32)v);
            } else { fail(u->name, ": a place wants a name in a way i do not know", NULL, NULL); return false; }
        }
    }
    return !bad;
}

static void copy_bytes(u8 *out, const plan *p, u32 n)
{
    for (u32 i = 0; i < n; i++) {
        unit *u = &units[i];
        for (u32 s = 0; s < ASM_NSEC; s++) {
            if (!u->laid[s]) continue;
            u8 *at = place(out, p, s, u->base[s]);
            if (at) memcpy(at, u->bytes[s], u->laid[s]);
        }
    }
}

static i64 write_program(u8 *out, u64 max, plan *p, u32 n)
{
    u64 code_len = p->code_end - p->code_start;
    u64 data_len = p->data_laid_end - p->data_start;
    u64 zero_len = p->data_end - p->data_laid_end;
    if (code_len == 0) { fail("there is no code in it", NULL, NULL, NULL); return -1; }
    u64 total = 20 + code_len + data_len;
    if (total > max) { fail("the image is larger than the room for it", NULL, NULL, NULL); return -1; }
    memset(out, 0, total);
    copy_bytes(out, p, n);
    if (!apply(out, p, n)) return -1;

    gsym *start = global_find("_start");
    u64 entry = start ? start->addr - p->code_start : 0;
    if (entry >= code_len) { fail("_start lies outside the code", NULL, NULL, NULL); return -1; }
    out[0] = 'E'; out[1] = 'B'; out[2] = 'X'; out[3] = '2';
    wr32(out + 4, (u32)code_len);
    wr32(out + 8, (u32)data_len);
    wr32(out + 12, (u32)zero_len);
    wr32(out + 16, (u32)entry);
    return (i64)total;
}

/* Writes the table of names into its place: entries in address
 * order, then the letters. */
static void write_names(u8 *out, const plan *p, u32 n)
{
    if (!names_bytes) return;
    typedef struct { u64 addr; const char *name; } ent;
    static ent *ents;
    if (!ents) ents = (ent *)lang_big_alloc(sizeof(ent) * NAMES_MAX);
    if (!ents) return;
    u32 count = 0;
    for (u32 i = 0; i < n; i++) {
        unit *u = &units[i];
        for (u32 s = 0; s < u->nsym && count < NAMES_MAX; s++) {
            if (!names_worthy(u, s)) continue;
            ents[count].addr = u->base[sym_sec(u, s)] + sym_off(u, s);
            ents[count].name = sym_name(u, s);
            count++;
        }
    }
    for (u32 gap = count / 2; gap > 0; gap /= 2)
        for (u32 i = gap; i < count; i++) {
            ent t = ents[i];
            u32 j = i;
            while (j >= gap && ents[j - gap].addr > t.addr) { ents[j] = ents[j - gap]; j -= gap; }
            ents[j] = t;
        }
    u8 *at = place(out, p, ASM_SEC_DATA, p->names_start);
    if (!at) return;
    wr32(at, count);
    wr32(at + 4, 8 + count * 16);
    u32 soff = 0;
    u8 *letters = at + 8 + (u64)count * 16;
    for (u32 i = 0; i < count; i++) {
        u8 *e = at + 8 + (u64)i * 16;
        wr64(e, ents[i].addr);
        wr32(e + 8, soff);
        wr32(e + 12, 0);
        const char *s = ents[i].name;
        while (*s) letters[soff++] = (u8)*s++;
        letters[soff++] = 0;
    }
}

static i64 write_kernel(u8 *out, u64 max, plan *p, u32 n)
{
    u64 total = p->seg_off[2] + p->seg_filesz[2];
    if (total > max) { fail("the kernel is larger than the room for it", NULL, NULL, NULL); return -1; }
    if (p->text_end == p->text_start) { fail("there is no code in it", NULL, NULL, NULL); return -1; }
    memset(out, 0, total);
    copy_bytes(out, p, n);
    if (!apply(out, p, n)) return -1;
    write_names(out, p, n);

    gsym *start = global_find("_start");
    if (!start) { fail("a kernel begins at _start, and nothing lays that name down", NULL, NULL, NULL); return -1; }

    /* The ELF head: 64-bit, little-endian, an executable for x86-64,
     * three program headers right behind it, no section headers. */
    u8 *e = out;
    e[0] = 0x7F; e[1] = 'E'; e[2] = 'L'; e[3] = 'F';
    e[4] = 2; e[5] = 1; e[6] = 1; e[7] = 0;
    wr16(e + 16, 2);                      /* ET_EXEC */
    wr16(e + 18, 0x3E);                   /* x86-64 */
    wr32(e + 20, 1);
    wr64(e + 24, start->addr);            /* entry */
    wr64(e + 32, 64);                     /* program headers */
    wr64(e + 40, 0);
    wr32(e + 48, 0);
    wr16(e + 52, 64);
    wr16(e + 54, 56);
    wr16(e + 56, 3);
    wr16(e + 58, 64);
    wr16(e + 60, 0);
    wr16(e + 62, 0);
    static const u32 flags[3] = { 5, 4, 6 };
    for (u32 s = 0; s < 3; s++) {
        u8 *ph = out + 64 + s * 56;
        wr32(ph, 1);                      /* PT_LOAD */
        wr32(ph + 4, flags[s]);
        wr64(ph + 8, p->seg_off[s]);
        wr64(ph + 16, p->seg_vaddr[s]);
        wr64(ph + 24, p->seg_vaddr[s] - KERNEL_VMA);
        wr64(ph + 32, p->seg_filesz[s]);
        wr64(ph + 40, p->seg_memsz[s]);
        wr64(ph + 48, PAGE);
    }
    return (i64)total;
}

/* ------------------------------------------------------------------ */
/* The link                                                            */
/* ------------------------------------------------------------------ */

i64 ld_link(const ld_unit *in, u32 n, u32 layout, u8 *out, u64 max,
            char *err, u32 errmax_)
{
    errp = err; errmax = errmax_; bad = false;
    if (errmax) err[0] = 0;
    if (!units) units = (unit *)lang_big_alloc(sizeof(unit) * UNITS_MAX);
    if (!gsyms) gsyms = (gsym *)lang_big_alloc(sizeof(gsym) * GSYMS_MAX);
    if (!hash)  hash  = (u32 *)lang_big_alloc(sizeof(u32) * HASH_SIZE);
    if (!units || !gsyms || !hash) { fail("there is no room for the linker's tables", NULL, NULL, NULL); return -1; }
    if (n == 0) { fail("nothing to link", NULL, NULL, NULL); return -1; }
    if (n > UNITS_MAX) { fail("too many objects at once", NULL, NULL, NULL); return -1; }

    for (u32 i = 0; i < n; i++)
        if (!parse_unit(&units[i], in[i].data, in[i].len, in[i].name ? in[i].name : "an object")) {
            fail(in[i].name ? in[i].name : "one of them", " is not an object the assembler made", NULL, NULL);
            return -1;
        }

    plan p;
    memset(&p, 0, sizeof(p));
    p.layout = layout;
    p.seg_of[ASM_SEC_BSS] = 3;
    names_bytes = 0;
    if (layout == LD_KERNEL) { names_measure(n); plan_kernel(&p, n); }
    else plan_program(&p, n);

    /* Every public name, once. */
    ngsyms = 0;
    for (u32 h = 0; h < HASH_SIZE; h++) hash[h] = 0xFFFFFFFF;
    for (u32 i = 0; i < n; i++) {
        unit *u = &units[i];
        for (u32 s = 0; s < u->nsym; s++) {
            u32 fl = sym_flags(u, s);
            if ((fl & 3) != 3) continue;
            i32 sec = sym_sec(u, s);
            if (sec < 0 || sec >= (i32)ASM_NSEC) continue;
            if (!global_add(sym_name(u, s), u->base[sec] + sym_off(u, s), i)) return -1;
        }
    }

    last_n = n;
    if (layout == LD_KERNEL) return write_kernel(out, max, &p, n);
    return write_program(out, max, &p, n);
}

/* The names of the last link and where they landed, one a line, for
 * reading a fault's address back to a name. */
u32 ld_map(char *out, u64 max)
{
    u64 at = 0;
    u32 n = 0;
    for (u32 ui = 0; ui < last_n && units; ui++) {
        unit *u = &units[ui];
        for (u32 s = 0; s < u->nsym && at + 96 < max; s++) {
            if (!(sym_flags(u, s) & 2)) continue;
            const char *nm = sym_name(u, s);
            if (nm[0] == '.') continue;   /* a text's own marks, not names */
            i32 sec = sym_sec(u, s);
            if (sec < 0 || sec >= (i32)ASM_NSEC) continue;
            char hex[17];
            u64 v = u->base[sec] + sym_off(u, s);
            for (i32 k = 15; k >= 0; k--) { hex[k] = "0123456789abcdef"[v & 15]; v >>= 4; }
            hex[16] = 0;
            for (u32 k = 0; k < 16; k++) out[at++] = hex[k];
            out[at++] = ' ';
            while (*nm && at + 2 < max) out[at++] = *nm++;
            if (!(sym_flags(u, s) & 1)) { out[at++] = ' '; out[at++] = '('; const char *un = u->name; while (*un && at + 3 < max) out[at++] = *un++; out[at++] = ')'; }
            out[at++] = '\n';
            n++;
        }
    }
    if (at < max) out[at] = 0;
    return n;
}

/* ------------------------------------------------------------------ */
/* A text, all the way                                                 */
/* ------------------------------------------------------------------ */

static u8 *objbuf;
static u8 *outbuf;
static char *textbuf;

u8 *lang_out_buffer(void)
{
    if (!outbuf) outbuf = (u8 *)lang_big_alloc(LANG_OUT_MAX);
    return outbuf;
}

char *lang_text_buffer(void)
{
    if (!textbuf) textbuf = (char *)lang_big_alloc(LANG_TEXT_MAX);
    return textbuf;
}

i64 lang_build_text(const u8 *src, u64 len, bool gnu, u8 *out, u64 max,
                    u32 *kind, char *err, u32 errmax_)
{
    if (!objbuf) objbuf = (u8 *)lang_big_alloc(OBJ_MAX);
    if (!objbuf) { if (errmax_) { const char *m = "there is no room for the object"; u32 k = 0; while (m[k] && k + 1 < errmax_) { err[k] = m[k]; k++; } err[k] = 0; } return -1; }
    i64 on = gnu ? asm_assemble_gnu(src, len, objbuf, OBJ_MAX, err, errmax_)
                 : asm_assemble(src, len, objbuf, OBJ_MAX, err, errmax_);
    if (on < 0) return -1;

    if (ld_object_wants(objbuf, (u64)on, NULL, 0) > 0) {
        if ((u64)on > max) { if (errmax_) { const char *m = "the object is larger than the room for it"; u32 k = 0; while (m[k] && k + 1 < errmax_) { err[k] = m[k]; k++; } err[k] = 0; } return -1; }
        memcpy(out, objbuf, (u64)on);
        if (kind) *kind = LANG_OBJECT;
        return on;
    }
    ld_unit u = { objbuf, (u64)on, "it" };
    i64 got = ld_link(&u, 1, LD_PROGRAM, out, max, err, errmax_);
    if (got < 0) return -1;
    if (kind) *kind = LANG_IMAGE;
    return got;
}
