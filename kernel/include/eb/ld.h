/*
 * ld.h -- linker interface: objects -> one thing that runs.
 * - sections laid in order, one address per name; a name laid down twice or never stops the link
 * - PROGRAM: code at USER_LOAD_CODE, data at USER_LOAD_DATA, entry = _start if laid down, else the first code byte
 * - KERNEL: ELF linked at -2 GiB, loaded at 2 MiB; text, rodata (ring-3 programs page-aligned), data, bss; __kernel_start and kin provided
 * - a text that lays down kmain is a kernel
 */
#ifndef EB_LD_H
#define EB_LD_H

#include <eb/types.h>

typedef struct {
    const u8   *data;                 /* an object, as the assembler made it */
    u64         len;
    const char *name;                 /* what to call it when something is wrong */
} ld_unit;

enum { LD_PROGRAM = 0, LD_KERNEL = 1 };

/* Links the units into out. Answers the length, or -1 with a one-line
 * reason in err ("b.c wants f_x, and no text lays it down"). */
i64 ld_link(const ld_unit *units, u32 n, u32 layout, u8 *out, u64 max,
            char *err, u32 errmax);

/* Whether these bytes are an object. */
bool ld_object_ok(const u8 *d, u64 n);

/* The public names of the last link and their addresses, as lines of
 * "hex name"; answers how many. */
u32 ld_map(char *out, u64 max);

/* Whether the object lays down a name of its own (kmain, say). */
bool ld_object_defines(const u8 *d, u64 n, const char *name);

/* The names the object uses and does not lay down, as "f_x, v_y" in
 * out; answers how many there were. */
u32 ld_object_wants(const u8 *d, u64 n, char *out, u32 max);

/* What a text becomes: an object that stands alone is linked into an
 * image at once; one that uses other texts' names stays an object. */
enum { LANG_IMAGE = 1, LANG_OBJECT = 2 };

i64 lang_build_text(const u8 *src, u64 len, bool gnu, u8 *out, u64 max,
                    u32 *kind, char *err, u32 errmax);

/* Two buffers, asked for once: one for whatever the tools make,
 * large enough for the kernel itself, and one for the assembly a
 * compile produces on the way. */
#define LANG_OUT_MAX  (8u * 1024 * 1024)
#define LANG_TEXT_MAX (4u * 1024 * 1024)
u8   *lang_out_buffer(void);
char *lang_text_buffer(void);

#endif /* EB_LD_H */
