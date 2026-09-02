/*
 * cchost.c -- the machine's compiler, assembler and linker, run on
 * the host.
 *
 * The same files the kernel builds, linked against nothing but libc's
 * memcpy and friends. Nothing runs here -- an image is the machine's
 * to run -- but a compile that crashes or refuses shows up in a
 * second instead of a boot, and the kernel can be built with the
 * machine's own tools and booted, to see how far they carry.
 *
 *   cchost <file.c> [texts the #includes may name...]
 *       compiles, assembles, and links the object alone when it can:
 *       writes <file>.asm and <file>.img (or <file>.obj when it uses
 *       other texts' names) beside the source
 *   cchost cc  <file.c> -o <out.obj> [texts...]
 *   cchost as  <file.s> -o <out.obj>          the machine's dialect
 *   cchost gnu <file.S> -o <out.obj>          the gnu dialect
 *   cchost ld  <out> [kernel] <objects...>    an image, or the kernel's ELF
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <eb/cc.h>
#include <eb/asm.h>
#include <eb/ld.h>
#include <eb/lang.h>

void *lang_big_alloc(u64 size) { return calloc(1, (size_t)size); }

/* A text goes by three names: the path as given, the part after an
 * include directory (so <eb/types.h> finds kernel/include/eb/types.h),
 * and the bare file name. */
typedef struct { char name[3][96]; unsigned char *text; unsigned long len; } src_file;
static src_file files[160];
static int nfiles;

static unsigned char *slurp(const char *path, unsigned long *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = (unsigned char *)malloc((size_t)n + 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    b[n] = 0;
    fclose(f);
    *len = (unsigned long)n;
    return b;
}

static void name_file(src_file *f, const char *p)
{
    strncpy(f->name[0], p, 95);
    const char *inc = strstr(p, "include/");
    strncpy(f->name[1], inc ? inc + 8 : p, 95);
    const char *s = strrchr(p, '/');
    strncpy(f->name[2], s ? s + 1 : p, 95);
}

static bool find(void *ctx, const char *name, const u8 **text, u64 *len)
{
    (void)ctx;
    for (int i = 0; i < nfiles; i++)
        for (int k = 0; k < 3; k++)
            if (strcmp(files[i].name[k], name) == 0) { *text = files[i].text; *len = files[i].len; return true; }
    return false;
}

static int load(const char *path)
{
    if (nfiles >= 160) return 0;
    unsigned long len;
    unsigned char *t = slurp(path, &len);
    if (!t) { fprintf(stderr, "cannot read %s\n", path); return 0; }
    name_file(&files[nfiles], path);
    files[nfiles].text = t;
    files[nfiles].len = len;
    nfiles++;
    return 1;
}

static int save(const char *path, const void *d, unsigned long n)
{
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return 0; }
    fwrite(d, 1, (size_t)n, f);
    fclose(f);
    return 1;
}

static char *text;
static u8 *obj;
static char err[200];

static const char *basename_of(const char *p)
{
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

/* c to an object; the assembly beside the object when asked. */
static i64 compile_to_object(const char *src, const char *asm_out)
{
    i64 got = cc_compile(files[0].text, files[0].len, basename_of(src), find, NULL,
                         text, 4u << 20, err, sizeof(err));
    if (got < 0) { printf("compile: %s\n", err); return -1; }
    if (asm_out) save(asm_out, text, (unsigned long)got);
    i64 on = asm_assemble((const u8 *)text, (u64)got, obj, 8u << 20, err, sizeof(err));
    if (on < 0) { printf("assemble: %s\n", err); return -1; }
    return on;
}

int main(int argc, char **argv)
{
    text = (char *)malloc(4u << 20);
    obj = (u8 *)malloc(8u << 20);
    if (argc < 2) { fprintf(stderr, "cchost <file.c> [texts...] | cc|as|gnu <file> -o <out> [texts...] | ld <out> [kernel] <objects...>\n"); return 2; }

    if (strcmp(argv[1], "ld") == 0) {
        if (argc < 4) { fprintf(stderr, "cchost ld <out> [kernel] <objects...>\n"); return 2; }
        const char *outp = argv[2];
        int first = 3;
        u32 layout = LD_PROGRAM;
        if (strcmp(argv[3], "kernel") == 0) { layout = LD_KERNEL; first = 4; }
        static ld_unit units[160];
        u32 n = 0;
        for (int i = first; i < argc && n < 160; i++) {
            unsigned long len;
            unsigned char *d = slurp(argv[i], &len);
            if (!d) { fprintf(stderr, "cannot read %s\n", argv[i]); return 2; }
            units[n].data = d; units[n].len = len; units[n].name = basename_of(argv[i]);
            n++;
        }
        u8 *out = (u8 *)malloc(LANG_OUT_MAX);
        i64 got = ld_link(units, n, layout, out, LANG_OUT_MAX, err, sizeof(err));
        if (got < 0) { printf("link: %s\n", err); return 1; }
        if (!save(outp, out, (unsigned long)got)) return 2;
        /* the map beside it: where every public name landed */
        char mapp[300];
        snprintf(mapp, sizeof(mapp), "%s.map", outp);
        char *map = (char *)malloc(4u << 20);
        u32 nm = ld_map(map, 4u << 20);
        save(mapp, map, (unsigned long)strlen(map));
        printf("ok: %lld bytes of %s, %u names in the map\n", (long long)got, layout == LD_KERNEL ? "kernel" : "image", nm);
        return 0;
    }

    if (strcmp(argv[1], "cc") == 0 || strcmp(argv[1], "as") == 0 || strcmp(argv[1], "gnu") == 0) {
        if (argc < 5 || strcmp(argv[3], "-o") != 0) { fprintf(stderr, "cchost %s <file> -o <out.obj> [texts...]\n", argv[1]); return 2; }
        const char *src = argv[2];
        const char *outp = argv[4];
        if (!load(src)) return 2;
        for (int i = 5; i < argc; i++) if (!load(argv[i])) return 2;
        i64 on;
        if (argv[1][0] == 'c') {
            char asmp[300];
            snprintf(asmp, sizeof(asmp), "%s.asm", outp);
            on = compile_to_object(src, asmp);
        } else if (argv[1][0] == 'g') {
            on = asm_assemble_gnu(files[0].text, files[0].len, obj, 8u << 20, err, sizeof(err));
            if (on < 0) printf("assemble: %s\n", err);
        } else {
            on = asm_assemble(files[0].text, files[0].len, obj, 8u << 20, err, sizeof(err));
            if (on < 0) printf("assemble: %s\n", err);
        }
        if (on < 0) return 1;
        if (!save(outp, obj, (unsigned long)on)) return 2;
        char wants[160];
        u32 nw = ld_object_wants(obj, (u64)on, wants, sizeof(wants));
        printf("ok: %lld bytes of object%s%s\n", (long long)on, nw ? ", wants " : "", nw ? wants : "");
        return 0;
    }

    /* the old way: one text, all the way */
    for (int i = 1; i < argc; i++) if (!load(argv[i])) return 2;
    char asmp[300], outp[300];
    snprintf(asmp, sizeof(asmp), "%s.asm", argv[1]);
    i64 on = compile_to_object(argv[1], asmp);
    if (on < 0) return 1;

    char wants[160];
    u32 nw = ld_object_wants(obj, (u64)on, wants, sizeof(wants));
    if (nw) {
        snprintf(outp, sizeof(outp), "%s.obj", argv[1]);
        save(outp, obj, (unsigned long)on);
        printf("object: %lld bytes; wants %s\n", (long long)on, wants);
        return 0;
    }
    ld_unit u = { obj, (u64)on, basename_of(argv[1]) };
    u8 *out = (u8 *)malloc(LANG_OUT_MAX);
    i64 got = ld_link(&u, 1, LD_PROGRAM, out, LANG_OUT_MAX, err, sizeof(err));
    if (got < 0) { printf("link: %s\n", err); return 1; }
    snprintf(outp, sizeof(outp), "%s.img", argv[1]);
    save(outp, out, (unsigned long)got);
    printf("ok: %lld bytes of image\n", (long long)got);
    return 0;
}
