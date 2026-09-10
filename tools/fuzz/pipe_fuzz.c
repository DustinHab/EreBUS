/*
 * pipe_fuzz.c -- libFuzzer entry for the object pipe: datagrams from one far machine.
 * - the first byte says how each datagram is delivered: bit 0 sealed (inside a session the harness opened with
 *   a HELLO first), else plain; bit 1 gives the far machine a nodes row with every right; bit 2 runs
 *   pipe_service between datagrams
 * - the seal is a stub that opens every envelope as it came, and every signature checks: the parsers behind
 *   authentication -- offers, chunks, asks, answers, the line, vouches, rotations -- see the input
 * - objects are a small fake store; what would start a program or install a kernel answers "no"
 * - built and run by tools/fuzz/run.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <eb/pipe.h>
#include <eb/nodes.h>
#include <eb/net.h>
#include <eb/crypto.h>
#include <eb/settings.h>
#include <eb/journal.h>
#include <eb/msg.h>
#include <eb/proc.h>
#include <eb/standard.h>
#include <eb/ssh.h>
#include <eb/term.h>
#include <eb/cc.h>
#include <eb/fat.h>
#include <eb/pmm.h>
#include <eb/time.h>
#include <eb/version.h>

const char erebus_version[] = "0.9.0-fuzz";

void kprintf(const char *fmt, ...) { (void)fmt; }
void journal_says(const char *who, const char *what) { (void)who; (void)what; }
void attention_note(const char *who, const char *what) { (void)who; (void)what; }

/* --- a fake object store --------------------------------------------------- */

struct object {
    type_id type;
    u64     size;
    u8     *data;
    u64     nslots;
    object **slot;
    char  **slot_name;
    u32    *slot_rights;
    char    name[OBJ_NAME_MAX];
    u32     refs;
    bool    transient;
};

object *obj_create(type_id type, u64 payload_size, u64 slot_count)
{
    if (payload_size > (64u << 20)) return NULL;
    object *o = calloc(1, sizeof *o);
    if (!o) return NULL;
    o->type = type;
    o->size = payload_size;
    o->data = calloc(1, payload_size ? payload_size : 1);
    o->nslots = slot_count;
    o->slot = calloc(slot_count ? slot_count : 1, sizeof(object *));
    o->slot_name = calloc(slot_count ? slot_count : 1, sizeof(char *));
    o->slot_rights = calloc(slot_count ? slot_count : 1, sizeof(u32));
    o->refs = 1;
    return o;
}
void obj_retain(object *o)  { if (o) o->refs++; }
void obj_release(object *o)
{
    if (!o || --o->refs) return;
    for (u64 i = 0; i < o->nslots; i++) { if (o->slot[i]) obj_release(o->slot[i]); free(o->slot_name[i]); }
    free(o->slot); free(o->slot_name); free(o->slot_rights); free(o->data); free(o);
}
type_id obj_type(const object *o)   { return o ? o->type : TYPE_NULL; }
void   *obj_data(object *o)         { return o ? o->data : NULL; }
u64     obj_size(const object *o)   { return o ? o->size : 0; }
u64     obj_slots(const object *o)  { return o ? o->nslots : 0; }
const char *obj_name(const object *o) { return o ? o->name : ""; }
void    obj_set_name(object *o, const char *name)
{ if (!o) return; strncpy(o->name, name ? name : "", OBJ_NAME_MAX - 1); o->name[OBJ_NAME_MAX - 1] = 0; }
void    obj_touch(object *o) { (void)o; }
void    obj_set_transient(object *o, bool t) { if (o) o->transient = t; }
bool    obj_set_slot(object *o, u64 i, object *target, u32 rights)
{
    if (!o || i >= o->nslots) return false;
    if (target) obj_retain(target);
    if (o->slot[i]) obj_release(o->slot[i]);
    o->slot[i] = target;
    o->slot_rights[i] = rights;
    return true;
}
object *obj_get_slot(object *o, u64 i) { return (o && i < o->nslots) ? o->slot[i] : NULL; }
bool    obj_grow_slots(object *o, u64 count)
{
    if (!o || count <= o->nslots || count > 4096) return false;
    o->slot = realloc(o->slot, count * sizeof(object *));
    o->slot_name = realloc(o->slot_name, count * sizeof(char *));
    o->slot_rights = realloc(o->slot_rights, count * sizeof(u32));
    for (u64 i = o->nslots; i < count; i++) { o->slot[i] = NULL; o->slot_name[i] = NULL; o->slot_rights[i] = 0; }
    o->nslots = count;
    return true;
}

/* --- ports, capabilities, programs: nothing runs here ---------------------- */

object *port_create(u64 capacity) { (void)capacity; return obj_create(9, 0, 0); }
bool port_try_receive(domain *to, cap_handle h, message *out) { (void)to; (void)h; (void)out; return false; }
cap_handle cap_insert(domain *d, object *o, u32 rights) { (void)d; (void)o; (void)rights; return 1; }
bool cap_revoke(domain *d, cap_handle h) { (void)d; (void)h; return true; }
object *work_launch(object *s, object *r, u64 b, i64 lo, i64 hi, object *in)
{ (void)s; (void)r; (void)b; (void)lo; (void)hi; (void)in; return NULL; }
object *work_code_launch(object *i, object *r, object *in) { (void)i; (void)r; (void)in; return NULL; }
bool proc_end(object *p) { (void)p; return false; }
bool proc_is_running(object *p) { (void)p; return false; }
bool proc_post_range(object *p, u64 tag, u64 a, u64 b) { (void)p; (void)tag; (void)a; (void)b; return false; }
bool term_compile_claim(void) { return false; }
void term_compile_release(void) { }
i64  cc_compile(const u8 *src, u64 len, const char *n, cc_find_fn f, void *ctx, char *out, u64 max, char *err, u32 errmax)
{ (void)src; (void)len; (void)n; (void)f; (void)ctx; (void)out; (void)max; (void)err; (void)errmax; return -1; }
i64  lang_build_text(const u8 *src, u64 len, bool gnu, u8 *out, u64 max, u32 *kind, char *err, u32 errmax)
{ (void)src; (void)len; (void)gnu; (void)out; (void)max; (void)kind; (void)err; (void)errmax; return -1; }
char *lang_text_buffer(void) { static char b[16]; return b; }
u8   *lang_out_buffer(void)  { static u8 b[16]; return b; }
bool fat_install_kernel(const u8 *elf, u64 len, char *why, u32 max) { (void)elf; (void)len; (void)why; (void)max; return false; }
void system_restart(void) { }
bool system_boot_files(const u8 **l, u64 *ls, const u8 **k, u64 *ks) { (void)l; (void)ls; (void)k; (void)ks; return false; }
u64  pmm_free_frames(void) { return 100000; }

/* --- the wire ------------------------------------------------------------------ */

static u64 clock_ns;
u64  time_ns(void) { clock_ns += 1000000; return clock_ns; }
void rand_bytes(u8 *out, u32 len) { for (u32 i = 0; i < len; i++) out[i] = (u8)(i * 13 + 5); }

static u32 sent_count;
bool net_udp_send(const u8 dst[4], u16 sport, u16 dport, const u8 *data, u32 len)
{ (void)dst; (void)sport; (void)dport; (void)data; if (len > 1500) abort(); sent_count++; return true; }
bool net_crypto_ok(void) { return true; }
bool net_own_address(u8 ip[4]) { ip[0] = 10; ip[1] = 0; ip[2] = 2; ip[3] = 15; return true; }
bool net_on_link(const u8 ip[4]) { return ip[0] == 10; }

/* --- the seal, open for inspection ------------------------------------------- */

void aes128_gcm_seal(const u8 key[16], const u8 iv[12], const u8 *aad, u32 alen,
                     const u8 *pt, u32 len, u8 *ct, u8 tag[16])
{ (void)key; (void)iv; (void)aad; (void)alen; if (ct != pt) memmove(ct, pt, len); memset(tag, 0, 16); }
bool aes128_gcm_open(const u8 key[16], const u8 iv[12], const u8 *aad, u32 alen,
                     const u8 *ct, u32 len, const u8 tag[16], u8 *pt)
{ (void)key; (void)iv; (void)aad; (void)alen; (void)tag; if (pt != ct) memmove(pt, ct, len); return true; }
void x25519(u8 out[32], const u8 scalar[32], const u8 point[32]) { for (u32 i = 0; i < 32; i++) out[i] = scalar[i] ^ point[i]; }
void x25519_base(u8 out[32], const u8 scalar[32]) { for (u32 i = 0; i < 32; i++) out[i] = (u8)(scalar[i] + 1); }
void ed25519_sign(u8 sig[64], const u8 seed[32], const u8 pk[32], const void *msg, u32 len)
{ (void)seed; (void)pk; (void)msg; (void)len; memset(sig, 0x42, 64); }
bool ed25519_verify(const u8 pk[32], const void *msg, u32 len, const u8 sig[64])
{ (void)pk; (void)msg; (void)len; (void)sig; return true; }

/* --- the machine's own identity -------------------------------------------------- */

static const u8 MY_KEY[32] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                               17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };
static const u8 FAR_KEY[32] = { 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB,
                                0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
                                0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF };
bool ssh_identity(u8 pub[32]) { memcpy(pub, MY_KEY, 32); return true; }
bool ssh_sign(const void *msg, u32 len, u8 sig[64]) { (void)msg; (void)len; memset(sig, 0x24, 64); return true; }
bool ssh_key_bytes(u8 out[64]) { memset(out, 0, 32); memcpy(out + 32, MY_KEY, 32); return true; }
void ssh_make_key(u8 key[64]) { memset(key, 7, 64); }
void ssh_init(const u8 key[64]) { (void)key; }
void ssh_fingerprint(char out[64]) { strcpy(out, "SHA256:fuzz"); }
void ssh_fingerprint_of(const u8 pub[32], char out[64]) { (void)pub; strcpy(out, "SHA256:fuzz"); }

void settings_name(char *out, u32 max) { strncpy(out, "fuzzbox", max); out[max - 1] = 0; }
bool settings_peer(u8 ip[4], u16 *port) { ip[0] = 10; ip[1] = 0; ip[2] = 2; ip[3] = 16; *port = PIPE_PORT; return true; }
bool settings_peer_name(char *out, u32 max) { (void)out; (void)max; return false; }
bool settings_work(void) { return true; }
/* Open, so a SEEK from any source is answered and the fuzzer reaches
 * that path; the gate itself is a switch on a setting, not a parser. */
u32  settings_discovery(void) { return DISCOVERY_OPEN; }

/* --- the run ------------------------------------------------------------------------ */

static const u8 FAR[4] = { 10, 0, 2, 16 };
static bool prepared;

static void prepare(void)
{
    if (prepared) return;
    prepared = true;
    nodes_create();
    pipe_prepare(NULL);
    object *line = obj_create(TYPE_TEXT, 8192, 0);
    pipe_line_set(line);
    object *arrivals = obj_create(TYPE_LIST, 0, 64);
    pipe_arrivals_set(arrivals);
    object *ledger = obj_create(TYPE_TEXT, 8192, 0);
    pipe_ledger_set(ledger);
    object *pg = obj_create(TYPE_TEXT, 8192, 0);
    pipe_page_set(pg);
}

#define MAGIC 0x58504245u
static void wr32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static void wr64(u8 *p, u64 v) { wr32(p, (u32)v); wr32(p + 4, (u32)(v >> 32)); }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 3 || size > 65536) return 0;
    prepare();
    u8 mode = data[0];
    const u8 *p = data + 1;
    size_t left = size - 1;

    /* A row for the far machine, with every right, when asked. */
    nodes_apply();
    i32 row = nodes_by_key(FAR_KEY);
    if (mode & 2) {
        if (row < 0) row = nodes_trust("far", FAR_KEY);
        if (row >= 0) nodes_allow((u32)row, NODE_MAY_WORK | NODE_MAY_UPDATE | NODE_MAY_VOUCH);
    } else if (row >= 0) {
        nodes_forget((u32)row);
    }

    /* The session: a signed HELLO from the far machine. */
    u32 sid = 0x1234;
    u64 ctr = 1;
    if (mode & 1) {
        u8 hello[188];
        memset(hello, 0, sizeof hello);
        wr32(hello, MAGIC);
        hello[4] = 6;
        wr32(hello + 8, sid);
        for (u32 i = 0; i < 32; i++) hello[12 + i] = (u8)(0x50 + i);
        memcpy(hello + 44, FAR_KEY, 32);
        memset(hello + 76, 0x33, 64);
        memcpy(hello + 140, "far", 3);
        memcpy(hello + 164, "0.9.0", 5);
        pipe_input(FAR, PIPE_PORT, hello, sizeof hello);
    }

    while (left >= 2) {
        u32 n = (u32)p[0] | ((u32)p[1] << 8);
        p += 2; left -= 2;
        if (n > left) n = (u32)left;
        if (mode & 1) {
            static u8 env[36 + 4096];
            if (n > 4096) n = 4096;
            wr32(env, MAGIC);
            env[4] = 8; env[5] = env[6] = env[7] = 0;
            wr32(env + 8, sid);
            wr64(env + 12, ctr++);
            memcpy(env + 20, p, n);
            memset(env + 20 + n, 0, 16);
            pipe_input(FAR, PIPE_PORT, env, 36 + n);
        } else {
            pipe_input(FAR, PIPE_PORT, p, n);
        }
        p += n; left -= n;
        if (mode & 4) pipe_service();
    }
    pipe_service();
    return 0;
}
