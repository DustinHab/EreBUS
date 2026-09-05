/*
 * nodes.c -- the nodes table: "name | key | address | version | may", one row per machine met.
 * - identity is the key column (base64 of the 32-byte ed25519 public key); a row whose key does not decode is ignored
 * - the kernel rewrites the whole text when a node appears, moves or changes version; names and the may column survive
 * - may: "work", "update", "all"; anything else grants nothing
 */
#include <eb/nodes.h>
#include <eb/base64.h>
#include <eb/journal.h>
#include <eb/ssh.h>
#include <eb/string.h>
#include <eb/fmt.h>
#include <eb/io.h>

#define NODES_BYTES 4096

typedef struct {
    char name[24];
    u8   key[32];
    bool has_addr;
    u8   ip[4];
    u16  port;
    char version[24];
    u32  may;
} noderec;

static object  *nodes;
static noderec  rows[NODES_MAX];
static u32      count;

object *nodes_object(void) { return nodes; }
u32     nodes_count(void)  { return count; }

static const char header[] =
    "name         | key                                         | address           | version       | may\n";

bool nodes_create(void)
{
    if (nodes) return true;
    nodes = obj_create(TYPE_TEXT, NODES_BYTES, 0);
    if (!nodes) return false;
    obj_set_name(nodes, "nodes");
    u8 *d = (u8 *)obj_data(nodes);
    for (u32 i = 0; i < sizeof(header); i++) d[i] = (u8)header[i];
    return true;
}

void nodes_adopt(object *o)
{
    if (!o || obj_type(o) != TYPE_TEXT) return;
    if (nodes) obj_release(nodes);
    obj_retain(o);
    nodes = o;
}

/* ------------------------------------------------------------------ */
/* Small tools                                                         */
/* ------------------------------------------------------------------ */

static char low(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

static bool same_name(const char *a, const char *b)
{
    u32 i = 0;
    while (a[i] && b[i] && low(a[i]) == low(b[i])) i++;
    return !a[i] && !b[i];
}

static void trim(const char **p, u64 *n)
{
    while (*n && **p == ' ') { (*p)++; (*n)--; }
    while (*n && ((*p)[*n - 1] == ' ' || (*p)[*n - 1] == '\r')) (*n)--;
}

static bool has_word(const char *s, u64 len, const char *w)
{
    u64 wl = strlen(w);
    for (u64 i = 0; i + wl <= len; i++) {
        u64 j = 0;
        while (j < wl && s[i + j] == w[j]) j++;
        if (j == wl) return true;
    }
    return false;
}

static void copy_printable(char *out, u32 max, const char *s, u64 len)
{
    u32 n = 0;
    for (u64 i = 0; i < len && n + 1 < max; i++) {
        char c = s[i];
        if (c >= 0x20 && c < 0x7F) out[n++] = c;
    }
    while (n && out[n - 1] == ' ') n--;
    out[n] = 0;
}

static u32 put(char *d, u32 at, const char *s)
{
    while (*s) d[at++] = *s++;
    return at;
}

static u32 put_pad(char *d, u32 at, const char *s, u32 width)
{
    u32 n = 0;
    while (s[n]) d[at++] = s[n++];
    while (n < width) { d[at++] = ' '; n++; }
    return at;
}

static u32 put_dec(char *d, u32 at, u64 v)
{
    char tmp[24];
    u32 n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) d[at++] = tmp[--n];
    return at;
}

/* ------------------------------------------------------------------ */
/* Reading                                                             */
/* ------------------------------------------------------------------ */

static void read_row(const char *line, u64 len)
{
    const char *c[5];
    u64 cl[5];
    u32 nc = 0;
    u64 start = 0;
    for (u64 i = 0; i <= len && nc < 5; i++) {
        if (i < len && line[i] != '|') continue;
        c[nc] = line + start;
        cl[nc] = i - start;
        trim(&c[nc], &cl[nc]);
        nc++;
        start = i + 1;
    }
    if (nc < 2) return;

    u8 key[48];
    i32 kl = base64_decode(c[1], (u32)cl[1], key, sizeof(key));
    if (kl != 32) return;

    u32 slot = count;
    for (u32 s = 0; s < count; s++)
        if (memcmp(rows[s].key, key, 32) == 0) slot = s;
    if (slot >= NODES_MAX) return;

    noderec *r = &rows[slot];
    memset(r, 0, sizeof(*r));
    memcpy(r->key, key, 32);

    copy_printable(r->name, sizeof(r->name), c[0], cl[0]);
    if (!r->name[0]) { const char *u = "unnamed"; u32 i = 0; while (u[i]) { r->name[i] = u[i]; i++; } r->name[i] = 0; }

    if (nc > 2) {
        u64 nums[5];
        u32 got = 0;
        for (u64 i = 0; i < cl[2] && got < 5; i++) {
            if (c[2][i] < '0' || c[2][i] > '9') continue;
            u64 g = 0;
            while (i < cl[2] && c[2][i] >= '0' && c[2][i] <= '9')
                g = g * 10 + (u64)(c[2][i++] - '0');
            nums[got++] = g;
        }
        if (got >= 4 && nums[0] < 256 && nums[1] < 256 && nums[2] < 256 && nums[3] < 256) {
            for (u32 i = 0; i < 4; i++) r->ip[i] = (u8)nums[i];
            r->port = (got == 5 && nums[4] > 0 && nums[4] < 65536) ? (u16)nums[4] : 7800;
            r->has_addr = true;
        }
    }
    if (nc > 3) copy_printable(r->version, sizeof(r->version), c[3], cl[3]);
    if (nc > 4) {
        if (has_word(c[4], cl[4], "work"))   r->may |= NODE_MAY_WORK;
        if (has_word(c[4], cl[4], "update")) r->may |= NODE_MAY_UPDATE;
        if (has_word(c[4], cl[4], "all"))    r->may |= NODE_MAY_WORK | NODE_MAY_UPDATE;
    }
    if (slot == count) count++;
}

void nodes_apply(void)
{
    count = 0;
    if (!nodes) return;
    const u8 *d = (const u8 *)obj_data(nodes);
    u64 size = obj_size(nodes);
    if (!d) return;

    u64 start = 0;
    for (u64 i = 0; i <= size; i++) {
        bool end = (i == size) || d[i] == 0 || d[i] == '\n';
        if (!end) continue;
        if (i > start) read_row((const char *)d + start, i - start);
        if (i == size || d[i] == 0) break;
        start = i + 1;
    }
}

/* ------------------------------------------------------------------ */
/* Writing                                                             */
/* ------------------------------------------------------------------ */

void nodes_may_words(u32 may, char out[24])
{
    u32 at = 0;
    if ((may & (NODE_MAY_WORK | NODE_MAY_UPDATE)) == (NODE_MAY_WORK | NODE_MAY_UPDATE))
        at = put(out, at, "work update");
    else if (may & NODE_MAY_WORK)   at = put(out, at, "work");
    else if (may & NODE_MAY_UPDATE) at = put(out, at, "update");
    else                            at = put(out, at, "-");
    out[at] = 0;
}

static void nodes_write(void)
{
    if (!nodes) return;
    u8 *d = (u8 *)obj_data(nodes);
    u64 size = obj_size(nodes);
    if (!d || size < sizeof(header) + 8) return;

    static char buf[NODES_BYTES];
    u32 at = put(buf, 0, header);
    for (u32 i = 0; i < count; i++) {
        noderec *r = &rows[i];
        if (at + 160 > sizeof(buf)) break;
        at = put_pad(buf, at, r->name, 12);
        at = put(buf, at, " | ");
        at += base64_encode(r->key, 32, buf + at, false);
        at = put(buf, at, " | ");
        u32 col = at;
        if (r->has_addr) {
            for (u32 k = 0; k < 4; k++) {
                if (k) buf[at++] = '.';
                at = put_dec(buf, at, r->ip[k]);
            }
            buf[at++] = ' ';
            at = put_dec(buf, at, r->port);
        } else {
            buf[at++] = '-';
        }
        while (at < col + 17) buf[at++] = ' ';
        at = put(buf, at, " | ");
        at = put_pad(buf, at, r->version[0] ? r->version : "-", 13);
        at = put(buf, at, " | ");
        char may[24];
        nodes_may_words(r->may, may);
        at = put(buf, at, may);
        buf[at++] = '\n';
    }
    if ((u64)at + 1 > size) at = (u32)size - 1;
    memcpy(d, buf, at);
    memset(d + at, 0, size - at);
    obj_touch(nodes);
}

/* ------------------------------------------------------------------ */
/* Looking things up                                                   */
/* ------------------------------------------------------------------ */

i32 nodes_by_key(const u8 key[32])
{
    for (u32 i = 0; i < count; i++)
        if (memcmp(rows[i].key, key, 32) == 0) return (i32)i;
    return -1;
}

i32 nodes_by_name(const char *name)
{
    if (!name || !name[0]) return -1;
    for (u32 i = 0; i < count; i++)
        if (same_name(rows[i].name, name)) return (i32)i;
    return -1;
}

i32 nodes_by_address(const u8 ip[4])
{
    for (u32 i = 0; i < count; i++)
        if (rows[i].has_addr && memcmp(rows[i].ip, ip, 4) == 0) return (i32)i;
    return -1;
}

bool nodes_name_at(u32 i, char out[24])
{
    if (i >= count) return false;
    memcpy(out, rows[i].name, 24);
    return true;
}

bool nodes_key_at(u32 i, u8 out[32])
{
    if (i >= count) return false;
    memcpy(out, rows[i].key, 32);
    return true;
}

bool nodes_address_at(u32 i, u8 ip[4], u16 *port)
{
    if (i >= count || !rows[i].has_addr) return false;
    if (ip) memcpy(ip, rows[i].ip, 4);
    if (port) *port = rows[i].port;
    return true;
}

bool nodes_version_at(u32 i, char out[24])
{
    if (i >= count) return false;
    memcpy(out, rows[i].version, 24);
    return true;
}

u32 nodes_may_at(u32 i)
{
    return i < count ? rows[i].may : 0;
}

/* ------------------------------------------------------------------ */
/* Changes                                                             */
/* ------------------------------------------------------------------ */

/* A name not used by another row: the claim, or the claim with a
 * number appended. */
static void unique_name(u32 self, const char *claim, char out[24])
{
    char base[24];
    copy_printable(base, sizeof(base), claim ? claim : "", claim ? strlen(claim) : 0);
    if (!base[0]) { const char *u = "unnamed"; u32 i = 0; while (u[i]) { base[i] = u[i]; i++; } base[i] = 0; }

    for (u32 n = 1; n < 100; n++) {
        char cand[24];
        u32 at = 0;
        u32 bl = (u32)strlen(base);
        if (n > 1 && bl > 20) bl = 20;
        for (u32 i = 0; i < bl; i++) cand[at++] = base[i];
        if (n > 1) { cand[at++] = ' '; at = put_dec(cand, at, n); }
        cand[at] = 0;
        bool taken = false;
        for (u32 i = 0; i < count; i++)
            if (i != self && same_name(rows[i].name, cand)) taken = true;
        if (!taken) { memcpy(out, cand, 24); return; }
    }
    memcpy(out, base, 24);
}

static void node_says(const char *name, const char *tail)
{
    char line[80];
    u32 at = put(line, 0, "node ");
    for (u32 i = 0; name[i] && at < 40; i++) line[at++] = name[i];
    for (u32 i = 0; tail[i] && at < sizeof(line) - 1; i++) line[at++] = tail[i];
    line[at] = 0;
    journal_says("pipe", line);
}

i32 nodes_meet(const char *claim, const u8 key[32], const u8 ip[4], u16 port,
               const char *version, bool quiet)
{
    i32 i = nodes_by_key(key);
    bool fresh = false, changed = false;

    if (i < 0) {
        if (count >= NODES_MAX) return -1;
        i = (i32)count;
        noderec *r = &rows[i];
        memset(r, 0, sizeof(*r));
        memcpy(r->key, key, 32);
        unique_name((u32)i, claim, r->name);
        count++;
        fresh = true;
    }
    noderec *r = &rows[i];

    if (ip) {
        i32 j = nodes_by_address(ip);
        if (j >= 0 && j != i) {
            /* the address was in another row: that row's address is cleared */
            rows[j].has_addr = false;
            changed = true;
        }
        if (!r->has_addr || memcmp(r->ip, ip, 4) != 0 || r->port != port) {
            memcpy(r->ip, ip, 4);
            r->port = port;
            r->has_addr = true;
            changed = true;
            if (!fresh && !quiet) node_says(r->name, " has a new address");
        }
    }
    if (version && version[0] && strcmp(r->version, version) != 0) {
        copy_printable(r->version, sizeof(r->version), version, strlen(version));
        changed = true;
        if (!fresh && !quiet) {
            char tail[40];
            u32 at = put(tail, 0, " runs ");
            for (u32 k = 0; r->version[k] && at < sizeof(tail) - 6; k++) tail[at++] = r->version[k];
            at = put(tail, at, " now");
            tail[at] = 0;
            node_says(r->name, tail);
        }
    }
    if (!fresh && claim && claim[0] && same_name(r->name, "unnamed")) {
        unique_name((u32)i, claim, r->name);
        changed = true;
    }

    if (fresh && !quiet) {
        char fp[64];
        ssh_fingerprint_of(key, fp);
        if (ip)
            kprintf("pipe: first handshake with %u.%u.%u.%u; key %s remembered in nodes as '%s'\n",
                    ip[0], ip[1], ip[2], ip[3], fp, r->name);
        node_says(r->name, ": first handshake; key written to nodes");
    }
    if (fresh || changed) nodes_write();
    return i;
}

bool nodes_allow(u32 i, u32 may)
{
    if (i >= count) return false;
    rows[i].may = may & (NODE_MAY_WORK | NODE_MAY_UPDATE);
    nodes_write();
    char words[24], tail[40];
    nodes_may_words(rows[i].may, words);
    u32 at = put(tail, 0, " may now: ");
    at = put(tail, at, rows[i].may ? words : "nothing");
    tail[at] = 0;
    node_says(rows[i].name, tail);
    return true;
}

i32 nodes_trust(const char *name, const u8 key[32])
{
    i32 i = nodes_by_key(key);
    if (i >= 0) {
        node_says(rows[i].name, " is already in nodes with that key");
        return i;
    }
    i = nodes_meet(name, key, NULL, 0, NULL, true);
    if (i < 0) return -1;
    char fp[64];
    ssh_fingerprint_of(key, fp);
    kprintf("pipe: node '%s' trusted before meeting; key %s\n", rows[i].name, fp);
    node_says(rows[i].name, " is trusted before meeting; its first handshake will be recognised");
    return i;
}

bool nodes_rekey(u32 i, const u8 newkey[32])
{
    if (i >= count) return false;
    if (nodes_by_key(newkey) >= 0 && nodes_by_key(newkey) != (i32)i) return false;
    memcpy(rows[i].key, newkey, 32);
    nodes_write();
    char fp[64];
    ssh_fingerprint_of(newkey, fp);
    kprintf("pipe: node '%s' renewed its key; now %s\n", rows[i].name, fp);
    node_says(rows[i].name, " renewed its key; the row carries the new one");
    return true;
}

/* Forgets a node: its row is dropped, so the next handshake from that
 * key -- or from its address with a new key -- is met fresh, trust on
 * first use again. This is how a changed key is deliberately accepted
 * (first forget the node, then let it knock) and how a wrong trust is
 * undone. */
bool nodes_forget(u32 i)
{
    if (i >= count) return false;
    char name[24];
    u32 n = 0;
    while (rows[i].name[n] && n < 23) { name[n] = rows[i].name[n]; n++; }
    name[n] = 0;

    for (u32 k = i; k + 1 < count; k++) rows[k] = rows[k + 1];
    count--;
    nodes_write();
    node_says(name, " is forgotten; the next handshake meets it fresh");
    return true;
}
