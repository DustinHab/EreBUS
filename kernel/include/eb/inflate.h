#ifndef EB_INFLATE_H
#define EB_INFLATE_H

#include <eb/types.h>

/* Deflate decompression, RFC 1951, with the two wrappings the web
 * puts around it: zlib (RFC 1950, inside png) and gzip (RFC 1952,
 * Content-Encoding). No compression here: the machine reads what
 * others packed and packs nothing itself.
 *
 * Every call answers the number of bytes written, or -1 when the
 * input is not what it claims to be or the output would not fit. The
 * input is never trusted: a length field past the input, a code that
 * names nothing, a distance before the start of the output all end
 * the call with -1 rather than a read outside the buffers. */

i64 inflate_raw(const u8 *in, u32 ilen, u8 *out, u32 omax, u32 *consumed);
i64 inflate_zlib(const u8 *in, u32 ilen, u8 *out, u32 omax);
i64 inflate_gzip(const u8 *in, u32 ilen, u8 *out, u32 omax);

#endif /* EB_INFLATE_H */
