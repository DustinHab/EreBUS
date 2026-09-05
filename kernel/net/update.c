/*
 * update.c -- self-update from a signed release over the network.
 * - checks a 'version' file under the release base; if newer, fetches kernel.elf and kernel.elf.sig
 * - verifies the detached ed25519 signature against the built-in release key; installs only if it checks
 * - runs in the network thread; the loader's kernel.old rollback is the last net
 */
#include <eb/update.h>
#include <eb/net.h>
#include <eb/settings.h>
#include <eb/journal.h>
#include <eb/crypto.h>
#include <eb/fat.h>
#include <eb/standard.h>
#include <eb/object.h>
#include <eb/string.h>
#include <eb/fmt.h>
#include <eb/time.h>

extern const char erebus_version[];

/* The release signing key's public half. Its private half signs each
 * release on the build machine and never leaves it. A kernel whose
 * signature does not verify against this key is never installed, so the
 * network only decides WHEN to update, never WHAT to. */
static const u8 release_pub[32] = {
    0x37,0x30,0x1e,0x25,0x9f,0xb5,0xdd,0x6a,
    0x72,0x08,0xf1,0x98,0x1a,0x6a,0xf7,0x5b,
    0x3d,0x67,0x4a,0x88,0xa0,0x37,0xd0,0x21,
    0xe1,0xbe,0x14,0xe7,0xd6,0x3f,0x5c,0x98
};

#define GH_BASE "https://github.com/DustinHab/EreBUS/releases/latest/download"
#define KERNEL_MAX (4u * 1024 * 1024)   /* room for the elf, a few times over */
#define SECOND     1000000000ULL

static u64  next_check_ns;
static u64  restart_at_ns;
static bool want_now;

void update_request(void) { want_now = true; }

/* Compares two dotted-decimal versions ("0.8.4"), reading only the
 * leading numeric parts; a trailing "-dirty" or " self-built" is
 * ignored. Positive when a is newer than b. */
static int ver_cmp(const char *a, const char *b)
{
    for (;;) {
        u32 va = 0, vb = 0;
        while (*a >= '0' && *a <= '9') va = va * 10 + (u32)(*a++ - '0');
        while (*b >= '0' && *b <= '9') vb = vb * 10 + (u32)(*b++ - '0');
        if (va != vb) return va > vb ? 1 : -1;
        if (*a == '.' && *b == '.') { a++; b++; continue; }
        if (*a == '.') return 1;     /* a has more parts: 0.8.4 > 0.8 */
        if (*b == '.') return -1;
        return 0;
    }
}

/* The release base: the setting if one is given, else GitHub. */
static u32 base_url(char *out, u32 max)
{
    if (settings_update_from(out, max) && out[0]) {
        u32 n = 0; while (out[n]) n++;
        while (n > 0 && (out[n-1] == '/' || out[n-1] == ' ')) n--;  /* no trailing slash */
        out[n] = 0;
        return n;
    }
    u32 n = 0;
    const char *s = GH_BASE;
    while (s[n] && n < max - 1) { out[n] = s[n]; n++; }
    out[n] = 0;
    return n;
}

static u32 join(char *out, u32 max, const char *base, const char *tail)
{
    u32 n = 0;
    while (base[n] && n < max - 1) { out[n] = base[n]; n++; }
    for (u32 i = 0; tail[i] && n < max - 1; i++) out[n++] = tail[i];
    out[n] = 0;
    return n;
}

/* One signed package holds everything an update needs, so the whole
 * check is a single fetch:
 *   magic "EBUPDATE"  (8 bytes)
 *   signature         (64 bytes, ed25519 over the version and kernel)
 *   version           (24 bytes, space-padded)
 *   kernel.elf        (the rest)
 * The signature covers the version together with the kernel, so neither
 * a forged kernel nor a lied-about version passes. */
#define PKG_MAGIC   "EBUPDATE"
#define PKG_MAGIC_N 8
#define PKG_SIG_AT  8
#define PKG_SIGNED  72          /* version+kernel begin here; the signature covers from here */
#define PKG_VER_AT  72
#define PKG_KERN_AT 96
#define PKG_MIN     (PKG_KERN_AT + 4096)

static void do_check(bool manual)
{
    char base[192];
    base_url(base, sizeof base);
    kprintf("update: checking %s\n", base);

    object *k = obj_create(TYPE_BYTES, KERNEL_MAX, 0);
    if (!k) return;
    u8 *d = (u8 *)obj_data(k);

    char url[256];
    u32 ul = join(url, sizeof url, base, "/update.pkg");
    u32 off = 0, len = 0;
    if (!net_fetch(url, ul, d, KERNEL_MAX, &off, &len, NULL) || len < PKG_MIN) {
        kprintf("update: no update package from %s\n", url);
        if (manual) journal_says("update", "no update package was fetched");
        obj_release(k);
        return;
    }
    u8 *pkg = d + off;
    if (memcmp(pkg, PKG_MAGIC, PKG_MAGIC_N) != 0) {
        kprintf("update: the reply was not an update package\n");
        if (manual) journal_says("update", "the reply was not an update package");
        obj_release(k);
        return;
    }

    char latest[25];
    u32 li = 0;
    for (u32 i = 0; i < 24; i++) {
        char c = (char)pkg[PKG_VER_AT + i];
        if (c == 0 || c == ' ') break;
        if (c >= 0x20 && c < 0x7F) latest[li++] = c;
    }
    latest[li] = 0;
    kprintf("update: package version '%s', running %s\n", latest, erebus_version);

    if (li == 0 || ver_cmp(latest, erebus_version) <= 0) {
        if (manual) journal_says("update", "already current");
        obj_release(k);
        return;
    }

    /* The signature must be this project's, over the version and kernel
     * exactly. This is the check that makes fetching over an
     * unauthenticated channel safe: the bytes are trusted because of who
     * signed them, not because of where they came from. */
    if (!ed25519_verify(release_pub, pkg + PKG_SIGNED, len - PKG_SIGNED,
                        pkg + PKG_SIG_AT)) {
        attention_note("update", "a downloaded update's signature did not verify; refused");
        obj_release(k);
        return;
    }

    u8 *kern = pkg + PKG_KERN_AT;
    u32 kern_len = len - PKG_KERN_AT;
    if (kern[0] != 0x7F || kern[1] != 'E' || kern[2] != 'L' || kern[3] != 'F') {
        attention_note("update", "a downloaded update held no kernel; refused");
        obj_release(k);
        return;
    }

    /* Install and arm the restart. The loader falls back to kernel.old
     * if the new one does not come up twice. */
    char why[120];
    if (!fat_install_kernel(kern, kern_len, why, sizeof why)) {
        attention_note("update", why);
        obj_release(k);
        return;
    }
    obj_release(k);

    {
        char line[64];
        u32 at = 0; const char *s = "updated to ";
        while (*s) line[at++] = *s++;
        for (u32 i = 0; latest[i] && at < sizeof(line) - 16; i++) line[at++] = latest[i];
        const char *t = "; restarting";
        while (*t) line[at++] = *t++;
        line[at] = 0;
        attention_note("update", line);
    }
    kprintf("update: a signed kernel is installed; restarting in 3 s\n");
    restart_at_ns = time_ns() + 3 * SECOND;
}

void update_tick(void)
{
    u64 now = time_ns();

    if (restart_at_ns && now >= restart_at_ns) {
        restart_at_ns = 0;
        system_restart();
        return;
    }

    if (want_now) {
        want_now = false;
        do_check(true);
        return;
    }

    if (!settings_update_auto()) { next_check_ns = 0; return; }

    /* First look a little after boot, then every six hours. A check
     * fetches the whole package, so it is not run more often than that;
     * 'update check' looks on demand. */
    if (next_check_ns == 0) next_check_ns = now + 30 * SECOND;
    if (now >= next_check_ns) {
        next_check_ns = now + 6ULL * 3600ULL * SECOND;
        do_check(false);
    }
}
