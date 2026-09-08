/*
 * erebus-task.c -- turn a program into a distributable EreBUS task package.
 *
 * A task package is a text: a "key | value" manifest, a "--" line, then
 * the payload. The manifest names the whole policy (kind, split, pieces,
 * across, combine, budget, input); the payload is c source the worker
 * nodes compile and run, or a recipe in the little language. This tool
 * builds the package and, for c, checks the source compiles for the node
 * with the node's own compiler -- so a mistake shows here, not on a boot.
 *
 * The package goes to a node's terminal (over ssh or the desktop): the
 * node's desk splits it, deals the pieces to the willing machines and
 * folds their answers by the combine rule. It does not run foreign
 * binaries: the payload is for the EreBUS compiler and the far-task ABI,
 * not a host executable.
 *
 *   erebus-task <file.c> [options]        > job.ebtask
 *   erebus-task <file>   --recipe [opts]  > job.ebtask   (a little-language recipe, not compiled)
 *
 * Options:
 *   --name <name>          a name for the task            (default: the file's)
 *   --split <lo> <hi>      divide the range lo..hi into pieces
 *   --pieces <n>           how many pieces                (default 4 when split)
 *   --across <n>           a quorum: every piece on n machines, majority taken
 *   --combine sum|concat|min|max|count|first             (default sum)
 *   --budget <seconds>     time each worker is granted
 *   --input <petname>      an object held on the node, sent ahead to each worker
 *   --recipe               the payload is a recipe, not c (no compile check)
 *   --ssh                  emit the door commands that feed and submit it:
 *                          erebus-task prog.c --split 1 1000000 --ssh | ssh node
 *   -o <file>              write here instead of standard output
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <eb/cc.h>
#include <eb/asm.h>
#include <eb/ld.h>
#include <eb/lang.h>

/* The wire carries a task's payload in one sealed datagram; keep this in
 * step with RECIPE_MAX in kernel/net/pipe.c. */
#define TASK_SOURCE_MAX 1024

void *lang_big_alloc(u64 size) { return calloc(1, (size_t)size); }

#include <stdarg.h>
void kprintf(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}

static unsigned char *slurp(const char *path, unsigned long *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = (unsigned char *)malloc((size_t)n + 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    b[n] = 0; fclose(f); *len = (unsigned long)n; return b;
}

static const char *basename_of(const char *p)
{
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

/* Compile, assemble and link the source exactly as a worker node does
 * (self-contained, no includes), to prove it builds. Returns 0 on
 * success, prints the compiler's word and returns 1 on failure. */
static int check_compiles(const unsigned char *src, unsigned long len, const char *name)
{
    static char asmtext[4u << 20];
    static u8   obj[8u << 20];
    char err[200];

    i64 al = cc_compile(src, len, name, NULL, NULL, asmtext, sizeof(asmtext), err, sizeof(err));
    if (al < 0) { fprintf(stderr, "compile: %s\n", err); return 1; }
    i64 on = asm_assemble((const u8 *)asmtext, (u64)al, obj, sizeof(obj), err, sizeof(err));
    if (on < 0) { fprintf(stderr, "assemble: %s\n", err); return 1; }
    ld_unit u = { obj, (u64)on, name };
    u8 *img = (u8 *)malloc(LANG_OUT_MAX);
    i64 gn = ld_link(&u, 1, LD_PROGRAM, img, LANG_OUT_MAX, err, sizeof(err));
    free(img);
    if (gn < 0) { fprintf(stderr, "link: %s\n", err); return 1; }
    return 0;
}

int main(int argc, char **argv)
{
    const char *src_path = NULL, *out_path = NULL, *name = NULL, *input = NULL;
    const char *combine = NULL;
    long lo = 0, hi = 0, pieces = 0, across = 0, budget = 0;
    int split = 0, recipe = 0, ssh = 0;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if      (!strcmp(a, "--ssh")) ssh = 1;
        else if (!strcmp(a, "-o") && i + 1 < argc) out_path = argv[++i];
        else if (!strcmp(a, "--name") && i + 1 < argc) name = argv[++i];
        else if (!strcmp(a, "--input") && i + 1 < argc) input = argv[++i];
        else if (!strcmp(a, "--combine") && i + 1 < argc) combine = argv[++i];
        else if (!strcmp(a, "--pieces") && i + 1 < argc) pieces = strtol(argv[++i], NULL, 10);
        else if (!strcmp(a, "--across") && i + 1 < argc) across = strtol(argv[++i], NULL, 10);
        else if (!strcmp(a, "--budget") && i + 1 < argc) budget = strtol(argv[++i], NULL, 10);
        else if (!strcmp(a, "--split") && i + 2 < argc) { split = 1; lo = strtol(argv[++i], NULL, 10); hi = strtol(argv[++i], NULL, 10); }
        else if (!strcmp(a, "--recipe")) recipe = 1;
        else if (a[0] == '-') { fprintf(stderr, "unknown option %s\n", a); return 2; }
        else if (!src_path) src_path = a;
        else { fprintf(stderr, "one source file at a time\n"); return 2; }
    }
    if (!src_path) {
        fprintf(stderr, "erebus-task <file.c> [--name n] [--split lo hi] [--pieces n]"
                        " [--across n] [--combine r] [--budget s] [--input name]"
                        " [--recipe] [-o out]\n");
        return 2;
    }
    if (combine && strcmp(combine, "sum") && strcmp(combine, "concat") &&
        strcmp(combine, "min") && strcmp(combine, "max") &&
        strcmp(combine, "count") && strcmp(combine, "first")) {
        fprintf(stderr, "combine is one of: sum concat min max count first\n");
        return 2;
    }

    unsigned long len;
    unsigned char *src = slurp(src_path, &len);
    if (!src) { fprintf(stderr, "cannot read %s\n", src_path); return 2; }
    if (len > TASK_SOURCE_MAX) {
        fprintf(stderr, "the payload is %lu bytes; the wire carries at most %d "
                        "(a larger task is a later milestone)\n", len, TASK_SOURCE_MAX);
        return 1;
    }
    if (!recipe && check_compiles(src, len, basename_of(src_path))) {
        fprintf(stderr, "the task does not build for the node; not packaged\n");
        return 1;
    }

    /* Build the whole package: the manifest, a "--" line, then the payload. */
    char m[2048];
    int at = 0;
    at += snprintf(m + at, sizeof(m) - at, "task | %s\n", name ? name : basename_of(src_path));
    at += snprintf(m + at, sizeof(m) - at, "kind | %s\n", recipe ? "recipe" : "code");
    if (split)   at += snprintf(m + at, sizeof(m) - at, "split | %ld %ld\n", lo, hi);
    if (pieces)  at += snprintf(m + at, sizeof(m) - at, "pieces | %ld\n", pieces);
    if (across)  at += snprintf(m + at, sizeof(m) - at, "across | %ld\n", across);
    if (combine) at += snprintf(m + at, sizeof(m) - at, "combine | %s\n", combine);
    if (budget)  at += snprintf(m + at, sizeof(m) - at, "budget | %ld\n", budget);
    if (input)   at += snprintf(m + at, sizeof(m) - at, "input | %s\n", input);
    at += snprintf(m + at, sizeof(m) - at, "--\n");

    char *pkg = (char *)malloc((size_t)at + len + 2);
    unsigned long pn = 0;
    memcpy(pkg, m, (size_t)at); pn = (unsigned long)at;
    memcpy(pkg + pn, src, len); pn += len;
    if (len == 0 || src[len - 1] != '\n') pkg[pn++] = '\n';

    FILE *out = stdout;
    if (out_path) { out = fopen(out_path, "wb"); if (!out) { fprintf(stderr, "cannot write %s\n", out_path); return 2; } }
    if (ssh) {
        /* the door reads exactly pn bytes into 'job', then submits it */
        fprintf(out, "receive %lu bytes as job\n", pn);
        fwrite(pkg, 1, (size_t)pn, out);
        fprintf(out, "submit job\n");
    } else {
        fwrite(pkg, 1, (size_t)pn, out);
    }
    if (out_path) { fclose(out); fprintf(stderr, "wrote %s\n", out_path); }
    return 0;
}
