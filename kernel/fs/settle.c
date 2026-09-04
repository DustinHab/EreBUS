/*
 * settle.c -- 'disks', 'settle on disk N', 'settle in partition P of disk N',
 * 'settle in the free space of disk N', 'yes'.
 * - each plan says what is lost; nothing is written before 'yes' (offer valid 180 s)
 * - whole disk: GPT with EFI boot volume (loader + kernel) and store partition
 * - the running machine adopts the new store at once
 */
#include <eb/settle.h>
#include <eb/blk.h>
#include <eb/gpt.h>
#include <eb/fat.h>
#include <eb/standard.h>
#include <eb/blob.h>
#include <eb/journal.h>
#include <eb/time.h>
#include <eb/fmt.h>
#include <eb/string.h>

#define STORE_MIN_SECTORS 40960u              /* 20 MiB */
#define ESP_SECTORS       131072u             /* 64 MiB */
#define ALIGN_SECTORS     2048u               /* a megabyte, as everyone aligns */
#define OFFER_NS          (180ULL * 1000000000ULL)

#define KIND_WHOLE 1
#define KIND_PART  2
#define KIND_FREE  3

static struct {
    bool active;
    u8   kind;
    u32  disk;
    u32  entry;               /* the table slot, for a partition */
    u64  first, count;        /* where the store goes */
    u64  made_ns;
} plan;

static gpt_table table;

/* ------------------------------------------------------------------ */
/* Words                                                               */
/* ------------------------------------------------------------------ */

static u32 put_str(char *out, u32 at, u32 max, const char *s)
{
    while (*s && at + 1 < max) out[at++] = *s++;
    out[at] = 0;
    return at;
}

static u32 put_num(char *out, u32 at, u32 max, u64 v)
{
    char d[24];
    u32 n = 0;
    if (v == 0) d[n++] = '0';
    while (v) { d[n++] = (char)('0' + v % 10); v /= 10; }
    while (n && at + 1 < max) out[at++] = d[--n];
    out[at] = 0;
    return at;
}

/* "512.0 MiB", "1.5 GiB" */
static u32 put_size(char *out, u32 at, u32 max, u64 bytes)
{
    u64 unit = 1024 * 1024;
    const char *name = "MiB";
    if (bytes >= 1024ULL * 1024 * 1024) { unit = 1024ULL * 1024 * 1024; name = "GiB"; }
    u64 tenths = (bytes * 10 + unit / 2) / unit;
    at = put_num(out, at, max, tenths / 10);
    at = put_str(out, at, max, ".");
    at = put_num(out, at, max, tenths % 10);
    at = put_str(out, at, max, " ");
    return put_str(out, at, max, name);
}

static bool number_after(const char *s, const char *word, u32 *out)
{
    u32 wl = 0;
    while (word[wl]) wl++;
    for (u32 i = 0; s[i]; i++) {
        u32 k = 0;
        while (k < wl && s[i + k] == word[k]) k++;
        if (k != wl) continue;
        const char *p = s + i + wl;
        while (*p == ' ') p++;
        if (*p < '0' || *p > '9') return false;
        u32 v = 0;
        while (*p >= '0' && *p <= '9') v = v * 10 + (u32)(*p++ - '0');
        *out = v;
        return true;
    }
    return false;
}

static bool has_words(const char *s, const char *w)
{
    u32 wl = 0;
    while (w[wl]) wl++;
    for (u32 i = 0; s[i]; i++) {
        u32 k = 0;
        while (k < wl && s[i + k] == w[k]) k++;
        if (k == wl) return true;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* What is on the bus                                                  */
/* ------------------------------------------------------------------ */

/* The used entries of the table in order of where they lie. */
static u32 entries_in_order(const gpt_table *t, u32 *order, u32 max)
{
    u32 n = 0;
    for (u32 i = 0; i < GPT_ENTRIES && n < max; i++)
        if (gpt_entry(t, i, NULL, NULL, NULL)) order[n++] = i;
    for (u32 a = 0; a + 1 < n; a++)
        for (u32 b = a + 1; b < n; b++) {
            u64 fa, fb;
            gpt_entry(t, order[a], NULL, &fa, NULL);
            gpt_entry(t, order[b], NULL, &fb, NULL);
            if (fb < fa) { u32 x = order[a]; order[a] = order[b]; order[b] = x; }
        }
    return n;
}

/* The largest gap between partitions, aligned; zero when none is
 * worth a store. */
static u64 largest_gap(const gpt_table *t, u64 *first_out)
{
    u32 order[GPT_ENTRIES];
    u32 n = entries_in_order(t, order, GPT_ENTRIES);
    u64 cursor = t->first_usable, best = 0, best_first = 0;
    for (u32 i = 0; i <= n; i++) {
        u64 end = i < n ? 0 : t->last_usable + 1;
        if (i < n) gpt_entry(t, order[i], NULL, &end, NULL);
        u64 start = (cursor + ALIGN_SECTORS - 1) / ALIGN_SECTORS * ALIGN_SECTORS;
        if (end > start && end - start > best) { best = end - start; best_first = start; }
        if (i < n) {
            u64 last;
            gpt_entry(t, order[i], NULL, NULL, &last);
            if (last + 1 > cursor) cursor = last + 1;
        }
    }
    if (best < STORE_MIN_SECTORS) return 0;
    *first_out = best_first;
    return best;
}

static bool disk_is_blank(u32 d)
{
    static u8 probe[64 * 512];
    for (u64 lba = 0; lba < 1024; lba += 64) {
        if (!blk_disk_read(d, lba, 64, probe)) return false;
        for (u32 i = 0; i < sizeof(probe); i++) if (probe[i]) return false;
    }
    return true;
}

static bool disk_has_mbr(u32 d)
{
    static u8 sec[512];
    if (!blk_disk_read(d, 0, 1, sec)) return false;
    if (sec[510] != 0x55 || sec[511] != 0xAA) return false;
    for (u32 p = 0; p < 4; p++) if (sec[446 + p * 16 + 4]) return true;
    return false;
}

static void say_disk_line(settle_say_fn say, void *ctx, u32 d)
{
    char line[160];
    u32 at = 0;
    at = put_str(line, at, sizeof(line), "disk ");
    at = put_num(line, at, sizeof(line), d + 1);
    at = put_str(line, at, sizeof(line), "  ");
    at = put_str(line, at, sizeof(line), blk_disk_model(d));
    at = put_str(line, at, sizeof(line), "  ");
    at = put_size(line, at, sizeof(line), blk_disk_sectors(d) * 512);
    at = put_str(line, at, sizeof(line), "  port ");
    at = put_num(line, at, sizeof(line), blk_disk_port(d));
    /* Not "the machine runs from it": a machine booted from a stick has
     * somebody else's system at the first port, and saying otherwise
     * would be putting a guess where a fact belongs. */
    if (blk_boot_disk() == (i32)d) at = put_str(line, at, sizeof(line), "  (first on the bus)");
    if (blk_store_disk() == (i32)d) at = put_str(line, at, sizeof(line), "  (holds the store the machine runs with)");
    say(ctx, line);
}

void settle_disks(settle_say_fn say, void *ctx)
{
    u32 n = blk_disk_count();
    if (n == 0) { say(ctx, "no disk answers on the sata bus."); return; }

    char line[160];
    for (u32 d = 0; d < n; d++) {
        say_disk_line(say, ctx, d);
        if (gpt_read(d, &table)) {
            u32 order[GPT_ENTRIES];
            u32 k = entries_in_order(&table, order, GPT_ENTRIES);
            u64 cursor = table.first_usable;
            for (u32 i = 0; i < k; i++) {
                u8 type[16];
                u64 first, last;
                gpt_entry(&table, order[i], type, &first, &last);
                if (first > cursor + ALIGN_SECTORS && first - cursor >= STORE_MIN_SECTORS) {
                    u32 a = put_str(line, 0, sizeof(line), "  free           ");
                    a = put_size(line, a, sizeof(line), (first - cursor) * 512);
                    say(ctx, line);
                }
                char name[40];
                gpt_entry_name(&table, order[i], name, sizeof(name));
                u32 a = put_str(line, 0, sizeof(line), "  partition ");
                a = put_num(line, a, sizeof(line), order[i] + 1);
                a = put_str(line, a, sizeof(line), "  ");
                a = put_str(line, a, sizeof(line), name[0] ? name : "(unnamed)");
                a = put_str(line, a, sizeof(line), "  ");
                a = put_size(line, a, sizeof(line), (last - first + 1) * 512);
                a = put_str(line, a, sizeof(line), "  ");
                a = put_str(line, a, sizeof(line), gpt_type_name(type));
                if (blk_store_disk() == (i32)d && blk_store_base() == first)
                    a = put_str(line, a, sizeof(line), "  (the store the machine runs with)");
                say(ctx, line);
                if (last + 1 > cursor) cursor = last + 1;
            }
            if (table.last_usable + 1 > cursor + STORE_MIN_SECTORS) {
                u32 a = put_str(line, 0, sizeof(line), "  free           ");
                a = put_size(line, a, sizeof(line), (table.last_usable + 1 - cursor) * 512);
                say(ctx, line);
            }
            if (k == 0) say(ctx, "  a partition table with nothing in it");
        } else if (disk_has_mbr(d)) {
            say(ctx, "  an old-style partition table; only 'settle on disk N' can take it, whole");
        } else if (disk_is_blank(d)) {
            say(ctx, "  blank");
        } else {
            say(ctx, "  no partition table; something else is written on it");
        }
    }
    say(ctx, "'settle on disk N' takes a disk whole; 'settle in partition P of disk N' takes one partition;");
    say(ctx, "'settle in the free space of disk N' leaves every partition as it is.  each asks before it acts.");
}

/* ------------------------------------------------------------------ */
/* The offer                                                           */
/* ------------------------------------------------------------------ */

void settle_plan(const char *what, settle_say_fn say, void *ctx)
{
    plan.active = false;
    u32 dn = 0, pn = 0;
    if (!number_after(what, "disk", &dn) || dn == 0 || dn > blk_disk_count()) {
        say(ctx, "settle on disk N, in partition P of disk N, or in the free space of disk N.  'disks' names them.");
        return;
    }
    u32 d = dn - 1;
    char line[200];
    u32 at;

    if (has_words(what, "free space")) {
        if (!gpt_read(d, &table)) {
            say(ctx, disk_is_blank(d) ? "that disk carries no partition table; 'settle on disk N' would take it whole."
                                      : "that disk carries no table this system can add to; 'settle on disk N' would take it whole.");
            return;
        }
        u64 first = 0;
        u64 count = largest_gap(&table, &first);
        if (!count) { say(ctx, "that disk has no free stretch of twenty megabytes or more."); return; }
        u32 order[GPT_ENTRIES];
        u32 k = entries_in_order(&table, order, GPT_ENTRIES);
        plan.kind = KIND_FREE; plan.disk = d; plan.first = first; plan.count = count;
        at = put_str(line, 0, sizeof(line), "a store of ");
        at = put_size(line, at, sizeof(line), count * 512);
        at = put_str(line, at, sizeof(line), " would be made in the free space of disk ");
        at = put_num(line, at, sizeof(line), dn);
        at = put_str(line, at, sizeof(line), "; its ");
        at = put_num(line, at, sizeof(line), k);
        at = put_str(line, at, sizeof(line), k == 1 ? " partition stays as it is." : " partitions stay as they are.");
        say(ctx, line);
    } else if (number_after(what, "partition", &pn)) {
        if (!gpt_read(d, &table)) { say(ctx, "that disk carries no partition table this system reads."); return; }
        u8 type[16];
        u64 first, last;
        if (pn == 0 || pn > GPT_ENTRIES || !gpt_entry(&table, pn - 1, type, &first, &last)) {
            say(ctx, "there is no such partition on that disk.");
            return;
        }
        if (blk_store_disk() == (i32)d && blk_store_base() == first) {
            say(ctx, "that partition is the store the machine runs with already.");
            return;
        }
        if (last - first + 1 < STORE_MIN_SECTORS) { say(ctx, "that partition is too small for a store; twenty megabytes is the least."); return; }
        char name[40];
        gpt_entry_name(&table, pn - 1, name, sizeof(name));
        plan.kind = KIND_PART; plan.disk = d; plan.entry = pn - 1; plan.first = first; plan.count = last - first + 1;
        at = put_str(line, 0, sizeof(line), "partition ");
        at = put_num(line, at, sizeof(line), pn);
        at = put_str(line, at, sizeof(line), " of disk ");
        at = put_num(line, at, sizeof(line), dn);
        at = put_str(line, at, sizeof(line), " (");
        at = put_str(line, at, sizeof(line), name[0] ? name : "unnamed");
        at = put_str(line, at, sizeof(line), ", ");
        at = put_size(line, at, sizeof(line), plan.count * 512);
        at = put_str(line, at, sizeof(line), ", ");
        at = put_str(line, at, sizeof(line), gpt_type_name(type));
        at = put_str(line, at, sizeof(line), ") would become the store.  what it holds is lost.");
        say(ctx, line);
    } else {
        /* The one disk that cannot be taken is the one the graph is
         * being kept on right now -- taking that would empty the very
         * thing being written to.
         *
         * The disk at the first port is not that disk merely by being
         * first. A machine running from a stick has somebody else's
         * system at the first port, and refusing to install onto it
         * because of where it sits would be refusing on a fact that is
         * not true. What protects it is the same thing that protects
         * every other disk: being told what is on it, and being asked. */
        if (blk_store_disk() == (i32)d) { say(ctx, "that disk holds the store the machine runs with."); return; }
        const u8 *l, *k;
        u64 ls, ks;
        if (!system_boot_files(&l, &ls, &k, &ks)) {
            say(ctx, "the loader did not hand its files over, so no boot volume can be made; a partition or the free space can still be the store.");
            return;
        }
        u64 sectors = blk_disk_sectors(d);
        if (sectors < ALIGN_SECTORS + ESP_SECTORS + STORE_MIN_SECTORS + 34) { say(ctx, "that disk is too small: a boot volume and a store need ninety megabytes at least."); return; }
        plan.kind = KIND_WHOLE; plan.disk = d;
        plan.first = ALIGN_SECTORS + ESP_SECTORS;
        plan.count = sectors - 34 - plan.first + 1;
        u32 parts = 0;
        const char *state = "blank";
        if (gpt_read(d, &table)) {
            u32 order[GPT_ENTRIES];
            parts = entries_in_order(&table, order, GPT_ENTRIES);
            state = "a partition table";
        } else if (disk_has_mbr(d)) state = "an old-style partition table";
        else if (!disk_is_blank(d)) state = "something written on it";
        at = put_str(line, 0, sizeof(line), "disk ");
        at = put_num(line, at, sizeof(line), dn);
        at = put_str(line, at, sizeof(line), " would be taken whole: ");
        at = put_str(line, at, sizeof(line), blk_disk_model(d));
        at = put_str(line, at, sizeof(line), ", ");
        at = put_size(line, at, sizeof(line), sectors * 512);
        at = put_str(line, at, sizeof(line), ", ");
        at = put_str(line, at, sizeof(line), state);
        if (parts) {
            at = put_str(line, at, sizeof(line), " with ");
            at = put_num(line, at, sizeof(line), parts);
            at = put_str(line, at, sizeof(line), parts == 1 ? " partition" : " partitions");
        }
        at = put_str(line, at, sizeof(line), ".  everything on it is lost.");
        say(ctx, line);
        at = put_str(line, 0, sizeof(line), "it gets a boot volume with the loader and this kernel, and a store of ");
        at = put_size(line, at, sizeof(line), plan.count * 512);
        at = put_str(line, at, sizeof(line), ".");
        say(ctx, line);
    }
    plan.active = true;
    plan.made_ns = time_ns();
    say(ctx, "if that is what you want, write: yes");
}

/* ------------------------------------------------------------------ */
/* Doing it                                                            */
/* ------------------------------------------------------------------ */

static bool blank_head(u32 disk, u64 first)
{
    static u8 zeros[64 * 512];
    memset(zeros, 0, sizeof(zeros));
    for (u32 i = 0; i < 16; i++)
        if (!blk_disk_write(disk, first + (u64)i * 64, 64, zeros)) return false;
    return true;
}

static void adopt(settle_say_fn say, void *ctx, u32 disk, u64 first, u64 count, bool bootable)
{
    if (!blank_head(disk, first)) { say(ctx, "the store's first sectors could not be cleared; nothing was adopted."); return; }
    if (!blk_adopt(disk, first, count)) { say(ctx, "the store would not be claimed; the disk is as the table now says, but the graph stays where it was."); return; }
    blob_reset();
    persist_start();
    journal_says("system", bootable ? "settled on a disk: the graph is kept there, and the next start may come from it"
                                    : "settled: the graph is kept on a disk now");
    say(ctx, bootable ? "settled.  the graph is kept there from now on, and the next start may come from that disk."
                      : "settled.  the graph is kept there from now on.");
}

void settle_yes(settle_say_fn say, void *ctx)
{
    if (!plan.active) { say(ctx, "yes to what?  'settle ...' makes an offer first."); return; }
    if (time_ns() - plan.made_ns > OFFER_NS) { plan.active = false; say(ctx, "that offer has passed; say it again."); return; }
    plan.active = false;
    char why[96];

    if (plan.kind == KIND_WHOLE) {
        const u8 *l, *k;
        u64 ls, ks;
        if (!system_boot_files(&l, &ls, &k, &ks)) { say(ctx, "the loader's files are not at hand after all."); return; }
        u64 sectors = blk_disk_sectors(plan.disk);
        gpt_make(&table, sectors);
        gpt_add(&table, GPT_TYPE_EFI, ALIGN_SECTORS, ALIGN_SECTORS + ESP_SECTORS - 1, "EREBUS BOOT");
        gpt_add(&table, GPT_TYPE_STORE, plan.first, table.last_usable, "EREBUS STORE");
        say(ctx, "writing the partition table.");
        if (!gpt_write(plan.disk, &table)) { say(ctx, "the table could not be written; the disk is in an unknown state now."); return; }
        say(ctx, "laying down the boot volume: the loader and this kernel.");
        if (!fat_lay_boot_volume(plan.disk, ALIGN_SECTORS, ESP_SECTORS, l, ls, k, ks, why, sizeof(why))) {
            say(ctx, why);
            return;
        }
        say(ctx, "taking the store.");
        adopt(say, ctx, plan.disk, plan.first, table.last_usable - plan.first + 1, true);
        return;
    }

    if (!gpt_read(plan.disk, &table)) { say(ctx, "the partition table has changed since; nothing was done."); return; }
    if (plan.kind == KIND_PART) {
        u64 first, last;
        if (!gpt_entry(&table, plan.entry, NULL, &first, &last) || first != plan.first) {
            say(ctx, "that partition is not where it was; nothing was done.");
            return;
        }
        gpt_entry_set_type(&table, plan.entry, GPT_TYPE_STORE);
    } else {
        u64 first = 0;
        if (largest_gap(&table, &first) < plan.count || first != plan.first) {
            say(ctx, "the free space is not where it was; nothing was done.");
            return;
        }
        if (gpt_add(&table, GPT_TYPE_STORE, plan.first, plan.first + plan.count - 1, "EREBUS STORE") < 0) {
            say(ctx, "the table has no room for another partition.");
            return;
        }
    }
    say(ctx, "writing the partition table.");
    if (!gpt_write(plan.disk, &table)) { say(ctx, "the table could not be written; the disk is in an unknown state now."); return; }
    say(ctx, "taking the store.");
    adopt(say, ctx, plan.disk, plan.first, plan.count, false);
}
