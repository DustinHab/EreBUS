/*
 * bundle.c -- a list folded into one byte stream, and back.
 * - magic word, then entries: kind, name, byte count for plain kinds; a list opens a nested run, a closing mark ends it
 * - no addresses and no rights in the stream
 */
#include <eb/bundle.h>
#include <eb/cap.h>
#include <eb/journal.h>
#include <eb/string.h>
#include <eb/fmt.h>
#include <eb/io.h>

#define BMAGIC 0x31424245u           /* "EBB1", little-endian */
#define BUNDLE_MAX 65536
#define VISIT_MAX 128
#define DEPTH_MAX 4

#define E_TEXT  1
#define E_BYTES 2
#define E_PIC   3
#define E_LIST  4
#define E_END   5

static u8 pack_buf[BUNDLE_MAX];

static u64 text_span(const u8 *d, u64 size)
{
    u64 n = 0;
    while (n < size && d[n]) n++;
    return n;
}

static bool put_bytes(u32 *at, const void *src, u32 n)
{
    if (*at + n > BUNDLE_MAX) return false;
    memcpy(pack_buf + *at, src, n);
    *at += n;
    return true;
}

static bool put_head(u32 *at, u8 etype, const char *name)
{
    u8 nl = 0;
    if (name) while (name[nl] && nl < 63) nl++;
    if (!put_bytes(at, &etype, 1)) return false;
    if (!put_bytes(at, &nl, 1)) return false;
    return put_bytes(at, name ? name : "", nl);
}

static u8 etype_of(type_id t)
{
    if (t == TYPE_TEXT) return E_TEXT;
    if (t == TYPE_BYTES) return E_BYTES;
    if (t == TYPE_PICTURE) return E_PIC;
    return 0;
}

static u32 packed_count;

static bool pack_walk(object *l, u32 *at, object **vis, u32 *nvis,
                      u32 depth)
{
    if (depth > DEPTH_MAX) return true;      /* too deep: left behind */

    for (u64 i = 0; i < obj_slots(l); i++) {
        object *t = obj_get_slot(l, i);
        if (!t) continue;
        if (!(obj_slot_rights(l, i) & CAP_READ)) continue;

        /* A loop in the graph is walked once. */
        bool been = false;
        for (u32 k = 0; k < *nvis; k++) if (vis[k] == t) been = true;
        if (been) continue;
        if (*nvis >= VISIT_MAX) return false;
        vis[(*nvis)++] = t;

        const char *nm = obj_slot_name(l, i);
        if (!nm) nm = obj_name(t);

        if (obj_type(t) == TYPE_LIST) {
            if (!put_head(at, E_LIST, nm)) return false;
            packed_count++;
            if (!pack_walk(t, at, vis, nvis, depth + 1)) return false;
            u8 end = E_END;
            if (!put_bytes(at, &end, 1)) return false;
            continue;
        }

        u8 et = etype_of(obj_type(t));
        if (et == 0) continue;               /* nothing that runs */

        const u8 *d = (const u8 *)obj_data(t);
        u64 size = obj_size(t);
        u32 plen = (u32)(et == E_TEXT && d ? text_span(d, size) : size);
        if (!d) plen = 0;

        if (!put_head(at, et, nm)) return false;
        if (!put_bytes(at, &plen, 4)) return false;
        if (d && !put_bytes(at, d, plen)) return false;
        packed_count++;
    }
    return true;
}

object *bundle_pack(object *list)
{
    if (!list || obj_type(list) != TYPE_LIST) return NULL;

    u32 at = 0;
    u32 magic = BMAGIC;
    if (!put_bytes(&at, &magic, 4)) return NULL;

    object *vis[VISIT_MAX];
    u32 nvis = 0;
    vis[nvis++] = list;                      /* a list is not its own cargo */
    packed_count = 0;

    if (!pack_walk(list, &at, vis, &nvis, 1)) return NULL;

    object *o = obj_create(TYPE_BYTES, at, 0);
    if (!o) return NULL;
    memcpy(obj_data(o), pack_buf, at);
    obj_set_name(o, "packed");

    kprintf("bundle: packed %u things, %u bytes\n", packed_count, at);
    return o;
}

bool bundle_smells(object *bytes)
{
    if (!bytes || obj_type(bytes) != TYPE_BYTES) return false;
    const u8 *d = (const u8 *)obj_data(bytes);
    if (!d || obj_size(bytes) < 4) return false;
    u32 m;
    memcpy(&m, d, 4);
    return m == BMAGIC;
}

static bool list_place(object *l, object *o, const char *nm, u8 nl)
{
    u64 n = obj_slots(l), at = n;
    for (u64 i = 0; i < n; i++)
        if (!obj_get_slot(l, i)) { at = i; break; }
    if (at == n && !obj_grow_slots(l, n + 1)) return false;
    obj_set_slot(l, at, o, CAP_READ | CAP_WRITE);
    if (nl) {
        /* A bundle may come from another machine: its name length is
         * its own claim, and the room here is not. */
        char name[64];
        if (nl > sizeof(name) - 1) nl = sizeof(name) - 1;
        for (u8 i = 0; i < nl; i++) name[i] = (char)nm[i];
        name[nl] = 0;
        obj_set_slot_name(l, at, name);
    }
    return true;
}

static bool unpack_walk(object *into, const u8 *d, u32 *at, u32 len,
                        u32 depth, u32 *count)
{
    while (*at < len) {
        u8 et = d[(*at)++];
        if (et == E_END) return true;
        if (*at >= len) return false;
        u8 nl = d[(*at)++];
        if (*at + nl > len) return false;
        const char *nm = (const char *)d + *at;
        *at += nl;

        if (et == E_LIST) {
            if (depth > DEPTH_MAX) return false;
            object *sub = obj_create(TYPE_LIST, 0, 4);
            if (!sub) return false;
            if (!unpack_walk(sub, d, at, len, depth + 1, count)) {
                obj_release(sub);
                return false;
            }
            bool ok = list_place(into, sub, (const char *)nm, nl);
            obj_release(sub);
            if (!ok) return false;
            (*count)++;
            continue;
        }

        if (et != E_TEXT && et != E_BYTES && et != E_PIC) return false;
        if (*at + 4 > len) return false;
        u32 plen;
        memcpy(&plen, d + *at, 4);
        *at += 4;
        if (*at + plen > len) return false;

        type_id t = et == E_TEXT ? TYPE_TEXT
                  : et == E_PIC  ? TYPE_PICTURE : TYPE_BYTES;
        u64 room = plen + (t == TYPE_TEXT ? 512 : 0);
        if (room == 0) room = 8;
        object *o = obj_create(t, room, 0);
        if (!o) return false;
        memcpy(obj_data(o), d + *at, plen);
        *at += plen;

        bool ok = list_place(into, o, (const char *)nm, nl);
        obj_release(o);
        if (!ok) return false;
        (*count)++;
    }
    return depth == 1;                       /* only the top ends by length */
}

object *bundle_unpack(object *bytes)
{
    if (!bundle_smells(bytes)) return NULL;
    const u8 *d = (const u8 *)obj_data(bytes);
    u32 len = (u32)obj_size(bytes);

    object *top = obj_create(TYPE_LIST, 0, 4);
    if (!top) return NULL;
    obj_set_name(top, "unpacked");

    u32 at = 4, count = 0;
    if (!unpack_walk(top, d, &at, len, 1, &count)) {
        obj_release(top);
        return NULL;
    }

    kprintf("bundle: unpacked %u things\n", count);
    return top;
}
