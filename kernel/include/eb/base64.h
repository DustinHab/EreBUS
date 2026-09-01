/*
 * base64.h -- bytes as letters, RFC 4648.
 *
 * Keys travel as text: in a settings line, in a fingerprint on the
 * screen, in a file another machine wrote. This turns the two ways.
 */
#ifndef EB_BASE64_H
#define EB_BASE64_H

#include <eb/types.h>

/* Writes the letters and a terminating zero; answers their count.
 * With pad false the trailing '=' signs are left off, the way a
 * fingerprint is written. out needs 4 * ((len + 2) / 3) + 1 bytes. */
u32 base64_encode(const u8 *in, u32 len, char *out, bool pad);

/* Reads letters until a '=' or anything that is not one of the
 * sixty-four; whitespace is skipped. Answers the byte count, or -1
 * when the letters do not fit or the room runs out. */
i32 base64_decode(const char *in, u32 len, u8 *out, u32 max);

#endif /* EB_BASE64_H */
