/*
 * lang_fuzz.c -- the language tools under libFuzzer.
 *
 * The compiler, the assembler in both dialects and the linker each
 * take arbitrary bytes here, under the address and undefined-behaviour
 * sanitizers, so that an input that makes any of them read or write
 * where it should not is found by a machine rather than by a person.
 * The first byte chooses the tool; the rest is the text or the object.
 *
 *   sh tools/fuzz/run.sh [seconds]
 */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <eb/cc.h>
#include <eb/asm.h>
#include <eb/ld.h>
#include <eb/lang.h>

void *lang_big_alloc(u64 size) { return calloc(1, (size_t)size); }

static bool no_find(void *ctx, const char *name, const u8 **text, u64 *len)
{
    (void)ctx; (void)name; (void)text; (void)len;
    return false;
}

static char *text;
static u8 *obj, *out;
static char err[200];

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2) return 0;
    if (!text) { text = malloc(4u << 20); obj = malloc(8u << 20); out = malloc(LANG_OUT_MAX); }
    uint8_t which = data[0] % 4;
    const u8 *body = data + 1;
    u64 n = size - 1;

    if (which == 0) {
        i64 got = cc_compile(body, n, "fuzz.c", no_find, NULL, text, 4u << 20, err, sizeof(err));
        if (got > 0) {
            i64 on = asm_assemble((const u8 *)text, (u64)got, obj, 8u << 20, err, sizeof(err));
            if (on > 0) {
                ld_unit u = { obj, (u64)on, "fuzz" };
                ld_link(&u, 1, LD_PROGRAM, out, LANG_OUT_MAX, err, sizeof(err));
                ld_link(&u, 1, LD_KERNEL, out, LANG_OUT_MAX, err, sizeof(err));
            }
        }
    } else if (which == 1) {
        i64 on = asm_assemble(body, n, obj, 8u << 20, err, sizeof(err));
        if (on > 0) { ld_unit u = { obj, (u64)on, "fuzz" }; ld_link(&u, 1, LD_PROGRAM, out, LANG_OUT_MAX, err, sizeof(err)); }
    } else if (which == 2) {
        i64 on = asm_assemble_gnu(body, n, obj, 8u << 20, err, sizeof(err));
        if (on > 0) { ld_unit u = { obj, (u64)on, "fuzz" }; ld_link(&u, 1, LD_KERNEL, out, LANG_OUT_MAX, err, sizeof(err)); }
    } else {
        /* the bytes as an object: the linker must refuse what is not one */
        ld_unit u = { body, n, "fuzz" };
        char wants[160];
        ld_object_ok(body, n);
        ld_object_wants(body, n, wants, sizeof(wants));
        ld_object_defines(body, n, "kmain");
        ld_link(&u, 1, LD_PROGRAM, out, LANG_OUT_MAX, err, sizeof(err));
        ld_link(&u, 1, LD_KERNEL, out, LANG_OUT_MAX, err, sizeof(err));
    }
    return 0;
}
