#ifndef EB_PICTURE_H
#define EB_PICTURE_H

#include <eb/types.h>
#include <eb/object.h>
#include <eb/cap.h>

/* A picture decoded by a program in ring 3.
 *
 * The browser fetches bytes it does not trust and wants pixels. The
 * decoders that turn one into the other are the parsers most exposed
 * to the outside, so they do not run in the kernel: the bytes are laid
 * into a fresh process's memory, the decoder program (programs/decoder)
 * runs there holding nothing but a port to answer on, and the kernel
 * reads the pixels back out of that memory once the answer names where
 * they are. A decoder that faults ends as any program does; one that
 * never answers is ended after a while. Either way the page goes on
 * without that picture. The contract with the program is eb/decoder.h. */

typedef struct {
    object    *program;        /* the running decoder, held; NULL when none */
    object    *reply;          /* the port it answers on */
    cap_handle reply_cap;      /* the caller's read right on it */
    domain    *dom;
    u64        started_ns;
} picture_job;

/* Starts a decoder on the bytes: a process is made, the bytes laid
 * into it, the job posted. False when no process could be made (no
 * memory, too many running); nothing is held then. The caller's domain
 * is where the reply port's read right is kept. */
bool picture_start(picture_job *j, domain *d, const u8 *bytes, u32 len);

/* What became of it. PICTURE_WAIT while the decoder works. On
 * PICTURE_DONE the pixels are in fresh contiguous pages the caller now
 * owns (pmm_free_contig when done with them), w by h of 0x00RRGGBB,
 * and kind is one of DECODER_KIND_*. The other answers carry no pixels.
 * Anything but PICTURE_WAIT ends the job: the program is ended and the
 * port let go. */
#define PICTURE_WAIT    0
#define PICTURE_DONE    1
#define PICTURE_REFUSED 2      /* the decoder said the bytes are not a picture it reads */
#define PICTURE_FAULTED 3      /* the decoder ended, or answered wrongly, without pixels */
#define PICTURE_LATE    4      /* the decoder gave no answer within the limit and was ended */
int picture_poll(picture_job *j, u32 **px, u32 *pages, u32 *w, u32 *h, u32 *kind);

/* Ends the job early: the page changed, the picture is not wanted. */
void picture_end(picture_job *j);

bool picture_busy(const picture_job *j);

/* How long a decoder may take before it is ended. */
#define PICTURE_LIMIT_NS (30ULL * 1000000000ULL)

#endif /* EB_PICTURE_H */
