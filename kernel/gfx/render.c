/*
 * render.c -- the page renderer as the kernel drives it: the input block put together, the program started on
 * it, its block read back and checked, then painted at any scroll (eb/renderer.h).
 * - the renderer image is programs/renderer, carried by the kernel as bytes (build/renderer/renderer_image.c)
 * - the block is laid at RENDER_IN before the program starts; the job is one message into its letter box
 * - the answer names the output block's address and length in the program's memory; every page of that range
 *   must be the program's own, and the block must pass render_check, or the page shows without a rendering
 */
#include <eb/renderer.h>
#include <eb/proc.h>
#include <eb/msg.h>
#include <eb/time.h>
#include <eb/fmt.h>
#include <eb/fb.h>
#include <eb/string.h>

extern const u8  renderer_image[];
extern const u32 renderer_image_len;

/* ------------------------------------------------------------------ */
/* The input block                                                     */
/* ------------------------------------------------------------------ */

static u32 take_bytes(render_input *b, const void *src, u32 len, bool nul)
{
    u32 need = len + (nul ? 1 : 0);
    if (b->full || need > b->cap - b->used) { b->full = true; return 0; }
    u32 at = b->used;
    u8 *d = (u8 *)b->in + at;
    if (len) memcpy(d, src, len);
    if (nul) d[len] = 0;
    b->used += need;
    return at;
}

void render_input_begin(render_input *b, void *buf, u32 cap, u32 width, u32 usew, u32 flags,
                        u64 unfold, const u32 col[5])
{
    b->in = (render_in *)buf;
    b->cap = cap;
    b->used = (u32)sizeof(render_in);
    b->full = cap < b->used;
    memset(b->in, 0, sizeof(render_in));
    b->in->magic = RENDER_IN_MAGIC;
    b->in->width = width;
    b->in->usew = usew;
    b->in->flags = flags;
    b->in->unfold = unfold;
    for (u32 i = 0; i < 5; i++) b->in->col[i] = col[i];
}

bool render_input_page(render_input *b, const u8 *page, u32 len)
{
    if (len > RENDER_PAGE_MAX) len = RENDER_PAGE_MAX;
    u32 at = take_bytes(b, page, len, true);
    if (!at) return false;
    b->in->page_at = at;
    b->in->page_len = len;
    return true;
}

static u32 slen(const char *s) { u32 n = 0; while (s[n]) n++; return n; }

bool render_input_sheet(render_input *b, const char *url, const u8 *text, u32 len)
{
    if (b->in->nsheets >= RENDER_SHEETS_MAX) return false;
    u32 ua = take_bytes(b, url, slen(url), true);
    if (!ua) return false;
    u32 ta = 0;
    if (text && len) { ta = take_bytes(b, text, len, false); if (!ta) return false; }
    render_sheet_in *s = &b->in->sheet[b->in->nsheets++];
    s->url_at = ua; s->text_at = ta; s->text_len = ta ? len : 0;
    return true;
}

bool render_input_image(render_input *b, const char *url, u32 w, u32 h)
{
    if (b->in->nimages >= HTML_IMAGES_MAX) return false;
    u32 ua = take_bytes(b, url, slen(url), true);
    if (!ua) return false;
    render_image_in *i = &b->in->image[b->in->nimages++];
    i->url_at = ua; i->w = w; i->h = h;
    return true;
}

bool render_input_find(render_input *b, const char *word, u32 from)
{
    if (!word || !word[0]) { b->in->find_at = 0; return true; }
    u32 at = take_bytes(b, word, slen(word), true);
    if (!at) return false;
    b->in->find_at = at;
    b->in->find_from = from;
    return true;
}

u32 render_input_end(render_input *b)
{
    if (b->full || !b->in->page_at) return 0;
    b->in->total = b->used;
    return b->used;
}

/* ------------------------------------------------------------------ */
/* The job                                                             */
/* ------------------------------------------------------------------ */

bool render_busy(const render_job *j) { return j && j->program; }

static void let_go(render_job *j)
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

bool render_start(render_job *j, domain *d, const void *block, u32 len)
{
    if (!j || !d || !block || len < sizeof(render_in) || len > RENDER_IN_MAX) return false;
    if (j->program) return false;

    j->dom = d;
    j->reply = port_create(4);
    if (!j->reply) return false;
    j->reply_cap = cap_insert(d, j->reply, CAP_READ);
    if (!j->reply_cap) { let_go(j); return false; }

    process *p = proc_create_code_laid("renderer", renderer_image, renderer_image_len,
                                       j->reply, RENDER_IN, (const u8 *)block, len);
    if (!p) {
        kprintf("web:  no process for the renderer: out of memory, or too many programs\n");
        let_go(j);
        return false;
    }
    j->program = proc_object(p);
    obj_retain(j->program);
    if (!proc_start(p)) { let_go(j); return false; }

    proc_post_range(j->program, RENDER_TAG_PAGE, len, 0);
    j->started_ns = time_ns();
    return true;
}

int render_poll(render_job *j, render_out *out, u32 cap)
{
    if (!j || !j->program) return RENDER_FAULTED;
    if (cap > RENDER_OUT_MAX) cap = RENDER_OUT_MAX;

    message m;
    while (port_try_receive(j->dom, j->reply_cap, &m)) {
        if (m.tag == RENDER_TAG_DONE && m.nwords >= 2) {
            u64 at = m.words[0], len = m.words[1];
            int r = RENDER_FAULTED;
            if (len >= RENDER_OUT_HEAD && len <= cap &&
                proc_read_memory(j->program, (virt_addr)at, out, len) &&
                render_check(out, len))
                r = RENDER_DONE;
            let_go(j);
            return r;
        }
        if (m.tag == RENDER_TAG_FAIL) { let_go(j); return RENDER_REFUSED; }
    }

    if (!proc_is_running(j->program)) { let_go(j); return RENDER_FAULTED; }
    if (time_ns() - j->started_ns > RENDER_LIMIT_NS) {
        kprintf("web:  the renderer gave no answer in %llu s; ending it\n",
                RENDER_LIMIT_NS / 1000000000ULL);
        let_go(j);
        return RENDER_LATE;
    }
    return RENDER_WAIT;
}

void render_end(render_job *j)
{
    if (j) let_go(j);
}

/* ------------------------------------------------------------------ */
/* Painting                                                            */
/* ------------------------------------------------------------------ */

/* A rectangle of the flow, clipped to the window's rows, in screen
 * coordinates. */
static void paint_rect(const render_window *w, i32 x, i32 y, i32 rw, i32 rh, color c)
{
    i32 sy = w->y + y - (i32)w->scroll * GLYPH_H;
    i32 y0 = sy < w->y ? w->y : sy;
    i32 y1 = sy + rh > w->y + w->h ? w->y + w->h : sy + rh;
    if (y1 <= y0) return;
    fb_rect(w->x + x, y0, rw, y1 - y0, c);
}

/* Whether a thing of the flow standing at y, h tall, falls whole
 * inside the window; and where on the screen. */
static bool whole(const render_window *w, i32 y, i32 h, i32 *sy)
{
    *sy = w->y + y - (i32)w->scroll * GLYPH_H;
    return *sy >= w->y && *sy + h <= w->y + w->h;
}

static void paint_field(const render_window *w, i32 x, i32 y, i32 fw, u32 idx, u32 kind,
                        const render_out *o)
{
    i32 sy;
    if (!whole(w, y - 2, GLYPH_H + 4, &sy)) return;
    sy += 2;
    x += w->x;
    color text = w->col[0], faint = w->col[2], accent = w->col[3], edge = w->col[4];
    u32 cols = (u32)(fw / GLYPH_W);
    if (kind == FIELD_SUBMIT) {
        const char *label = o->init[idx];
        fb_rect(x - 2, sy - 2, fw + 2, GLYPH_H + 4, edge);
        fb_glyph(x, sy, '[', accent, 0, false);
        u32 ll = 0;
        for (; label[ll] && ll + 2 < cols; ll++)
            fb_glyph(x + GLYPH_W + (i32)ll * GLYPH_W, sy, (u8)label[ll], accent, 0, false);
        fb_glyph(x + GLYPH_W + (i32)ll * GLYPH_W, sy, ']', accent, 0, false);
    } else {
        const char *shown = w->values ? w->values[idx] : o->init[idx];
        fb_rect(x - 2, sy - 2, fw + 2, GLYPH_H + 3, edge);
        fb_rect(x - 1, sy - 1, fw, GLYPH_H + 1, faint);
        for (u32 k = 0; shown[k] && k + 1 < cols; k++)
            fb_glyph(x + (i32)k * GLYPH_W, sy, kind == FIELD_PASS ? '*' : (u8)shown[k], text, 0, false);
    }
}

void render_paint(const render_out *o, const render_window *w)
{
    if (!o || !w) return;
    u32 at = 0, n = o->ops_words;
    while (at < n) {
        u32 head = o->ops[at];
        u32 kind = OP_KIND(head), words = OP_WORDS(head);
        const u32 *v = &o->ops[at + 1];
        at += 1 + words;
        i32 x = (i32)v[0], y = (i32)v[1];
        switch (kind) {
        case OP_RECT:
            paint_rect(w, x, y, (i32)v[2], (i32)v[3], v[4]);
            break;
        case OP_TEXT: {
            u32 scale = v[2] >> 24;
            color c = v[2] & 0xFFFFFFu;
            i32 sy;
            if (!whole(w, y, (i32)scale * GLYPH_H, &sy)) break;
            i32 sx = w->x + x;
            for (u32 i = 3; i < words; i++, sx += (i32)scale * GLYPH_W)
                fb_glyph_cp_scaled(sx, sy, v[i], c, (i32)scale);
            break;
        }
        case OP_IMAGE: {
            const render_picture *p = w->pictures ? &w->pictures[v[4]] : NULL;
            i32 sy = w->y + y - (i32)w->scroll * GLYPH_H;
            if (p && p->px && p->w && p->h)
                fb_image(w->x + x, sy, (i32)v[2], (i32)v[3], p->px, p->w, p->h, w->y, w->y + w->h);
            else
                paint_rect(w, x, y, (i32)v[2], (i32)v[3], w->col[2]);
            break;
        }
        case OP_FIELD:
            paint_field(w, x, y, (i32)v[2], v[4], v[5], o);
            break;
        default:
            break;
        }
    }
}

u32 render_spots(const render_out *o, const render_window *w, u32 kind, html_spot *dst)
{
    const html_spot *src;
    u32 n;
    if (kind == RENDER_SPOTS_LINKS) { src = o->links; n = o->nlinks; }
    else if (kind == RENDER_SPOTS_FIELDS) { src = o->fspots; n = o->nfspots; }
    else { src = o->folds; n = o->nfolds; }
    u32 k = 0;
    for (u32 i = 0; i < n && k < HTML_SPOTS_MAX; i++) {
        i32 sy;
        if (!whole(w, src[i].y, src[i].h, &sy)) continue;
        dst[k] = src[i];
        dst[k].x += w->x;
        dst[k].y = sy;
        k++;
    }
    return k;
}
