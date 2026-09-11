/*
 * picture.c -- a picture decoded by a program in ring 3: the decoder image started on the bytes, its answer
 * taken from a port of its own, the pixels read out of its memory and checked, the program ended.
 * - the decoder image is programs/decoder, carried by the kernel as bytes (build/decoder/decoder_image.c)
 * - the bytes are laid at DECODER_IN before the program starts; the job is one message into its letter box
 * - the answer names width, height, kind and where in its own memory the pixels lie; every page of that range
 *   must be the program's own, or the picture is refused
 */
#include <eb/picture.h>
#include <eb/decoder.h>
#include <eb/proc.h>
#include <eb/msg.h>
#include <eb/pmm.h>
#include <eb/mm.h>
#include <eb/time.h>
#include <eb/fmt.h>

/* The program image, made by tools/mkimage.py from the linked decoder. */
extern const u8  decoder_image[];
extern const u32 decoder_image_len;

bool picture_busy(const picture_job *j) { return j && j->program; }

static void let_go(picture_job *j)
{
    if (j->program) {
        if (proc_is_running(j->program)) proc_end(j->program);
        obj_release(j->program);
        j->program = NULL;
    }
    if (j->reply) {
        if (j->reply_cap) cap_revoke(j->dom, j->reply_cap);
        obj_release(j->reply);
        j->reply = NULL;
        j->reply_cap = 0;
    }
}

bool picture_start(picture_job *j, domain *d, const u8 *bytes, u32 len)
{
    if (!j || !d || !bytes || len == 0 || len > DECODER_IN_MAX) return false;
    if (j->program) return false;

    j->dom = d;
    j->reply = port_create(4);
    if (!j->reply) return false;
    j->reply_cap = cap_insert(d, j->reply, CAP_READ);
    if (!j->reply_cap) { let_go(j); return false; }

    /* The process: the decoder's code and data from the image, the
     * picture's bytes laid read-only where the program expects them,
     * the reply port as its console. It holds that port send-only and
     * its own letter box, nothing else. */
    process *p = proc_create_code_laid("decoder", decoder_image, decoder_image_len,
                                       j->reply, DECODER_IN, bytes, len);
    if (!p) {
        kprintf("web:  no process for the decoder: out of memory, or too many programs\n");
        let_go(j);
        return false;
    }
    j->program = proc_object(p);
    obj_retain(j->program);
    if (!proc_start(p)) { let_go(j); return false; }

    proc_post_range(j->program, DECODER_TAG_PICT, len, DECODER_PIXELS_MAX);
    j->started_ns = time_ns();
    return true;
}

/* The pixels, out of the program's memory into pages of the caller's.
 * The address is the program's word and is checked page by page: a
 * range that is not the program's own memory refuses the copy. */
static bool take_pixels(picture_job *j, virt_addr at, u32 w, u32 h, u32 **px, u32 *pages)
{
    u64 bytes = (u64)w * h * 4;
    u32 n = (u32)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
    phys_addr pa = pmm_alloc_contig(n);
    if (pa == PMM_NO_FRAME) return false;
    u32 *out = (u32 *)phys_to_virt(pa);
    if (!proc_read_memory(j->program, at, out, bytes)) {
        pmm_free_contig(pa, n);
        return false;
    }
    *px = out;
    *pages = n;
    return true;
}

int picture_poll(picture_job *j, u32 **px, u32 *pages, u32 *w, u32 *h, u32 *kind)
{
    if (!j || !j->program) return PICTURE_FAULTED;

    message m;
    while (port_try_receive(j->dom, j->reply_cap, &m)) {
        if (m.tag == DECODER_TAG_DONE && m.nwords >= 3) {
            u32 pw = (u32)(m.words[0] & 0xFFFFFFFFu);
            u32 ph = (u32)(m.words[0] >> 32);
            u32 pk = (u32)m.words[1];
            int r = PICTURE_FAULTED;
            if (pw && ph && pw <= 16384 && ph <= 16384 &&
                (u64)pw * ph <= DECODER_PIXELS_MAX &&
                take_pixels(j, (virt_addr)m.words[2], pw, ph, px, pages)) {
                *w = pw; *h = ph; *kind = pk;
                r = PICTURE_DONE;
            }
            let_go(j);
            return r;
        }
        if (m.tag == DECODER_TAG_FAIL) {
            *kind = m.nwords ? (u32)m.words[0] : DECODER_KIND_NONE;
            let_go(j);
            return PICTURE_REFUSED;
        }
        /* anything else it says is not an answer */
    }

    if (!proc_is_running(j->program)) {
        let_go(j);
        return PICTURE_FAULTED;
    }
    if (time_ns() - j->started_ns > PICTURE_LIMIT_NS) {
        kprintf("web:  the decoder gave no answer in %llu s; ending it\n",
                PICTURE_LIMIT_NS / 1000000000ULL);
        let_go(j);
        return PICTURE_LATE;
    }
    return PICTURE_WAIT;
}

void picture_end(picture_job *j)
{
    if (j) let_go(j);
}
