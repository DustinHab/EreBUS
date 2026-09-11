#ifndef DECODER_IMAGE_H
#define DECODER_IMAGE_H

#include <eb/types.h>

/* Pictures from outside, decoded into pixels of 0x00RRGGBB for the
 * screen: png (RFC 2083), baseline jpeg (ITU T.81) and webp (VP8 and
 * VP8L). The decoders take hostile input: every length, count and
 * index is checked, and a file that lies answers false with nothing
 * written past max_pixels.
 *
 * These run in ring 3, in the decoder program (programs/decoder), and
 * in the fuzzer (tools/fuzz/img_fuzz.c); nothing here is linked into
 * the kernel.
 *
 * scratch is working room the caller lends: for png the unfiltered
 * rows (up to (1 + 4 * width) * height bytes), for jpeg the planes of
 * the components, for webp the frame buffers and code tables. Too
 * little of it is answered with false. */

bool png_decode(const u8 *in, u32 len, u32 *out, u32 max_pixels, u32 *w, u32 *h,
                u8 *scratch, u32 scratch_len);
bool jpeg_decode(const u8 *in, u32 len, u32 *out, u32 max_pixels, u32 *w, u32 *h,
                 u8 *scratch, u32 scratch_len);
/* webp: the lossy (VP8) and lossless (VP8L) still frames. */
bool webp_decode(const u8 *in, u32 len, u32 *out, u32 max_pixels, u32 *w, u32 *h,
                 u8 *scratch, u32 scratch_len);
bool webp_size(const u8 *in, u32 len, u32 *w, u32 *h);

/* What a picture is, from its first bytes; 0 when none. The numbers
 * are the kinds the kernel is told in the decoder's answer
 * (eb/decoder.h). */
#define IMAGE_NONE 0
#define IMAGE_PNG  1
#define IMAGE_JPEG 2
#define IMAGE_WEBP 3
u32 image_kind(const u8 *in, u32 len);

/* Only the size, read from the header; for laying out before decoding. */
bool image_size(const u8 *in, u32 len, u32 *w, u32 *h);

#endif /* DECODER_IMAGE_H */
