/*
 * render_check.c -- the kernel's check of a block the renderer program answered with (eb/render.h).
 * - the program is not trusted: every count is held to its table, every string ended, every op whole and its
 *   indices inside the tables, every size bounded, before the kernel reads a byte of it as anything
 * - written against eb/render.h alone: the fuzzer (tools/fuzz/html_fuzz.c) runs it on hostile blocks too
 */
#include <eb/render.h>

static void end_string(char *s, u32 max) { s[max - 1] = 0; }

static bool spots_ok(html_spot *s, u32 n, u32 refs)
{
    for (u32 i = 0; i < n; i++) {
        if (s[i].w < 0 || s[i].h < 0 || s[i].w > (1 << 20) || s[i].h > (1 << 20)) return false;
        if (s[i].x < -(1 << 20) || s[i].x > (1 << 20) || s[i].y < -(1 << 20) || s[i].y > (1 << 24)) return false;
        if (s[i].ref >= refs) return false;
    }
    return true;
}

bool render_check(render_out *o, u64 bytes)
{
    if (!o || bytes < RENDER_OUT_HEAD || bytes > RENDER_OUT_MAX) return false;
    if (o->magic != RENDER_OUT_MAGIC || o->total != bytes) return false;
    if (o->ops_words != (bytes - RENDER_OUT_HEAD) / 4 || (bytes - RENDER_OUT_HEAD) % 4) return false;
    if (o->rows > (1u << 24)) return false;
    if (o->nurls > HTML_LINKS_MAX || o->nforms > HTML_FORMS_MAX || o->nfields > HTML_FIELDS_MAX ||
        o->nimages > HTML_IMAGES_MAX || o->nsheets > RENDER_SHEETS_MAX ||
        o->nlinks > HTML_SPOTS_MAX || o->nfspots > HTML_SPOTS_MAX || o->nfolds > HTML_SPOTS_MAX)
        return false;

    end_string(o->title, HTML_TITLE_MAX);
    for (u32 i = 0; i < HTML_LINKS_MAX; i++) end_string(o->urls[i], HTML_URL_MAX);
    for (u32 i = 0; i < HTML_FORMS_MAX; i++) {
        end_string(o->forms[i].action, HTML_URL_MAX);
        if (i < o->nforms && o->forms[i].method > METHOD_POST) return false;
    }
    for (u32 i = 0; i < HTML_FIELDS_MAX; i++) {
        end_string(o->fields[i].name, HTML_NAME_MAX);
        end_string(o->init[i], HTML_VALUE_MAX);
        if (i < o->nfields && (o->fields[i].kind > FIELD_SUBMIT || o->fields[i].form >= HTML_FORMS_MAX)) return false;
    }
    for (u32 i = 0; i < HTML_IMAGES_MAX; i++) end_string(o->images[i], HTML_URL_MAX);
    for (u32 i = 0; i < RENDER_SHEETS_MAX; i++) end_string(o->sheets[i], HTML_URL_MAX);

    if (!spots_ok(o->links, o->nlinks, o->nurls)) return false;
    if (!spots_ok(o->fspots, o->nfspots, o->nfields)) return false;
    if (!spots_ok(o->folds, o->nfolds, 1u << 20)) return false;   /* a fold's number; only the first 64 open */

    /* The ops: each whole, of a known kind, its numbers within reason. */
    u32 at = 0, n = o->ops_words;
    while (at < n) {
        u32 head = o->ops[at];
        u32 kind = OP_KIND(head), words = OP_WORDS(head);
        if (words > n - at - 1) return false;
        const u32 *w = &o->ops[at + 1];
        switch (kind) {
        case OP_RECT:
            if (words != 5) return false;
            if ((i32)w[2] < 0 || (i32)w[3] < 0 || w[2] > (1u << 20) || w[3] > (1u << 20)) return false;
            break;
        case OP_TEXT:
            if (words < 3 || words - 3 > OP_TEXT_MAX) return false;
            if ((w[2] >> 24) < 1 || (w[2] >> 24) > 3) return false;
            for (u32 i = 3; i < words; i++) if (w[i] > 0x10FFFF) return false;
            break;
        case OP_IMAGE:
            if (words != 5) return false;
            if ((i32)w[2] <= 0 || (i32)w[3] <= 0 || w[2] > (1u << 20) || w[3] > (1u << 20)) return false;
            if (w[4] >= o->nimages) return false;
            break;
        case OP_FIELD:
            if (words != 6) return false;
            if ((i32)w[2] <= 0 || (i32)w[3] <= 0 || w[2] > (1u << 20) || w[3] > (1u << 20)) return false;
            if (w[4] >= o->nfields || w[5] > FIELD_SUBMIT) return false;
            break;
        default:
            return false;
        }
        if ((i32)w[0] < -(1 << 20) || (i32)w[0] > (1 << 20) || (i32)w[1] < -(1 << 20) || (i32)w[1] > (1 << 24)) return false;
        at += 1 + words;
    }
    return true;
}
