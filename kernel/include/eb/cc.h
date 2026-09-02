/*
 * cc.h -- the compiler: c, the way this machine speaks it.
 *
 * A text of c becomes a text of assembly in the assembler's own
 * words, and that becomes an image the loader runs. The middle step
 * is kept, as an object beside the source: what the compiler made is
 * there to be read, which is the difference between a tool and a
 * trick. No library comes with it -- main is called with the two
 * handles a program starts holding, and syscall(nr, ...) is the door
 * to the kernel, the same eight calls as everywhere.
 */
#ifndef EB_CC_H
#define EB_CC_H

#include <eb/types.h>

/* How the compiler finds a text named in an #include: by petname,
 * among whatever the caller can see. Answers false when nothing of
 * that name is there. */
typedef bool (*cc_find_fn)(void *ctx, const char *name,
                           const u8 **text, u64 *len);

/* Compiles src into assembly text in out. Answers the text's length,
 * or -1 with a one-line reason in err ("line 12 in the compiler:
 * i do not know 'flaot'"). */
i64 cc_compile(const u8 *src, u64 len, const char *src_name,
               cc_find_fn find, void *ctx,
               char *out, u64 max, char *err, u32 errmax);

#endif /* EB_CC_H */
