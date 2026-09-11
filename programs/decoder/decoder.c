/*
 * decoder.c -- the picture decoder, a program in ring 3: given a picture's bytes, it answers with the pixels.
 * - the kernel lays the bytes at DECODER_IN before the program starts and posts "PICT" (length, most pixels)
 * - the pixels go into this program's own memory; "DONE" names their address, width, height and kind, and the
 *   kernel takes them from there; "FAIL" says the bytes are not a picture this reads
 * - after the answer the program waits in its letter box; the kernel ends it once the pixels are taken
 * - built by clang and lld on the host into the image format of MANUAL 18.4 (program.ld, tools/mkimage.py) and by
 *   the machine's own compiler in the self-build; the kernel carries the image as bytes and runs nothing of it in ring 0
 */
#include <eb/decoder.h>
#include "image.h"
#include "erebus.h"

/* Where the pixels and the working room lie: this program's own zeroed
 * memory, mapped with its data. The kernel is told the pixels' address
 * in the answer and checks the range is this program's before reading. */
static u32 pixels[DECODER_PIXELS_MAX];
static u8  scratch[DECODER_SCRATCH];

long main(long console, long inbox)
{
    unsigned char m[EB_MSG_BYTES];
    long len = -1, most = 0;

    /* The job: the one message the kernel posts. Anything else in the
     * box is not for us. */
    while (len < 0) {
        if (eb_receive(inbox, m, 0) != EB_OK) continue;
        if (eb_msg_tag(m) != (long)DECODER_TAG_PICT) continue;
        len = eb_msg_word(m, 0);
        most = eb_msg_word(m, 1);
    }
    if (most > (long)DECODER_PIXELS_MAX) most = (long)DECODER_PIXELS_MAX;

    const u8 *in = (const u8 *)DECODER_IN;
    u32 w = 0, h = 0, kind = IMAGE_NONE;
    bool ok = false;
    if (len > 0 && len <= (long)DECODER_IN_MAX) {
        kind = image_kind(in, (u32)len);
#if defined(EREBUS_TEST_FAULT) && EREBUS_TEST_FAULT == 4
        /* The test build (tools/decoder-fault.sh): a jpeg ends this
         * program on a page fault, a webp never answers. Neither can
         * reach the kernel; the page goes on without the picture. */
        if (kind == IMAGE_JPEG) *(volatile u32 *)0 = 1;
        if (kind == IMAGE_WEBP) for (;;) {}
#endif
        if (kind == IMAGE_PNG)
            ok = png_decode(in, (u32)len, pixels, (u32)most, &w, &h, scratch, sizeof(scratch));
        else if (kind == IMAGE_JPEG)
            ok = jpeg_decode(in, (u32)len, pixels, (u32)most, &w, &h, scratch, sizeof(scratch));
        else if (kind == IMAGE_WEBP)
            ok = webp_decode(in, (u32)len, pixels, (u32)most, &w, &h, scratch, sizeof(scratch));
    }

    if (ok)
        eb_send(console, (long)DECODER_TAG_DONE, (long)w | ((long)h << 32), (long)kind,
                (long)(unsigned long)pixels);
    else
        eb_send(console, (long)DECODER_TAG_FAIL, (long)kind, 0, 0);

    /* The kernel reads the pixels while this waits, then ends it; a
     * receive on an ended program's box answers refused, and the next
     * call is the door it leaves through. */
    for (;;) eb_receive(inbox, m, 0);
}
