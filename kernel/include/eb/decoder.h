#ifndef EB_DECODER_H
#define EB_DECODER_H

/* The contract between the kernel and the picture decoder, a program
 * in ring 3 (programs/decoder). Both sides include this and nothing
 * else of each other.
 *
 * The kernel starts the program with the picture's bytes laid at
 * DECODER_IN, read-only, before it runs -- the way a loaded program's
 * data is laid -- and posts one message into its letter box. The
 * program decodes into its own memory and answers on its console, which
 * the kernel made a port of its own for the purpose; the kernel then
 * reads the pixels out of the program's memory at the address the
 * answer names, checks every page of that range is the program's, and
 * ends the program. Nothing about the decoding runs in ring 0, and a
 * decoder that faults, lies or never answers costs the browser one
 * picture. */

/* Past anything the image lays: the data and its zeroed room begin at
 * 0x1100000 and may reach 64 MiB (asm.c ZERO_MAX). */
#define DECODER_IN          0x0000000006000000ULL   /* where the bytes lie */
#define DECODER_IN_MAX      (1u << 20)              /* the most bytes a picture may be */
#define DECODER_PIXELS_MAX  (1600u * 1200u)         /* the largest picture decoded */
#define DECODER_SCRATCH     (8u << 20)              /* the program's working room */

/* Into the program's letter box: the job. Two words, the length of the
 * bytes and the most pixels the kernel takes. */
#define DECODER_TAG_PICT 0x54434950ULL              /* "PICT" */

/* From the program, on its console: the pixels are there. Three words:
 * width in the low half of the first and height in the high, the kind
 * below, and the address in the program's own space where width times
 * height pixels of 0x00RRGGBB lie. */
#define DECODER_TAG_DONE 0x454E4F44ULL              /* "DONE" */

/* From the program: not a picture it reads. One word, the kind as far
 * as the first bytes said. */
#define DECODER_TAG_FAIL 0x4C494146ULL              /* "FAIL" */

/* The kinds, as the program names them in its answer. */
#define DECODER_KIND_NONE 0
#define DECODER_KIND_PNG  1
#define DECODER_KIND_JPEG 2
#define DECODER_KIND_WEBP 3

#endif /* EB_DECODER_H */
