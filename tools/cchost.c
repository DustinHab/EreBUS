/*
 * cchost.c -- the machine's compiler and assembler, run on the host.
 *
 * The same two files the kernel builds, linked against nothing but
 * libc's memcpy and friends: a c text in, the assembly and the image
 * out, and the compiler's own words when something is wrong. Nothing
 * runs here -- the image is the machine's to run -- but a compile
 * that crashes or refuses shows up in a second instead of a boot.
 *
 *   cchost <file.c> [more texts the #includes may name...]
 *
 * Writes <file>.asm and <file>.img beside the source.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <eb/cc.h>
#include <eb/asm.h>

typedef struct { char name[64]; unsigned char *text; unsigned long len; } src_file;
static src_file files[128];
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

/* The name a text goes by: its file name, or -- for a header under
 * an include directory -- the path from there, so <eb/types.h>
 * finds kernel/include/eb/types.h. */
static const char *basename_of(const char *p)
{
    const char *inc = strstr(p, "include/");
    if (inc) return inc + 8;
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

static bool find(void *ctx, const char *name, const u8 **text, u64 *len)
{
    (void)ctx;
    for (int i = 0; i < nfiles; i++)
        if (strcmp(files[i].name, name) == 0) { *text = files[i].text; *len = files[i].len; return true; }
    return false;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "cchost <file.c> [texts...]\n"); return 2; }
    for (int i = 1; i < argc && nfiles < 128; i++) {
        unsigned long len;
        unsigned char *t = slurp(argv[i], &len);
        if (!t) { fprintf(stderr, "cannot read %s\n", argv[i]); return 2; }
        strncpy(files[nfiles].name, basename_of(argv[i]), 63);
        files[nfiles].text = t;
        files[nfiles].len = len;
        nfiles++;
    }

    static char text[1 << 20];
    static u8 image[1 << 18];
    char err[160];

    i64 got = cc_compile(files[0].text, files[0].len, files[0].name, find, NULL,
                         text, sizeof(text), err, sizeof(err));
    if (got < 0) { printf("compile: %s\n", err); return 1; }

    char out[256];
    snprintf(out, sizeof(out), "%s.asm", argv[1]);
    FILE *f = fopen(out, "wb");
    if (f) { fwrite(text, 1, (size_t)got, f); fclose(f); }

    i64 img = asm_assemble((const u8 *)text, (u64)got, image, sizeof(image), err, sizeof(err));
    if (img < 0) { printf("assemble: %s\n", err); return 1; }

    snprintf(out, sizeof(out), "%s.img", argv[1]);
    f = fopen(out, "wb");
    if (f) { fwrite(image, 1, (size_t)img, f); fclose(f); }

    printf("ok: %lld letters of assembly, %lld bytes of image\n",
           (long long)got, (long long)img);
    return 0;
}
