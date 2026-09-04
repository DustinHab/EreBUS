/*
 * cc.h -- compiler interface: c text -> assembly text (kept beside the source) -> image.
 * - no library: main gets the two starting handles, syscall(nr, ...) is the door to the kernel
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
/* The line-level tidying of the output can be switched off from
 * outside, to measure what it is worth. */
extern bool cc_peep_off;
extern const char *cc_peep_only;      /* or only in these functions, comma-separated */

i64 cc_compile(const u8 *src, u64 len, const char *src_name,
               cc_find_fn find, void *ctx,
               char *out, u64 max, char *err, u32 errmax);

#endif /* EB_CC_H */
