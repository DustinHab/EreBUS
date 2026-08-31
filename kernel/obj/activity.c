/*
 * activity.c -- what the machine is doing, kept where everything is.
 *
 * The same trick as the time and the journal: one ordinary text
 * object, one writer. The kernel rewrites it every second, the lenses
 * show it, the snapshot takes it along, and whoever is handed the
 * reference sees the machine's pulse without gaining a single right
 * beyond reading a text.
 *
 * The processor shares are not estimates. Every handover of the
 * processor books the interval to the thread that held it, so a row
 * saying 3% means three hundredths of the last second, clock-measured.
 * The boot thread only halts; its share is the idle in the summary.
 */
#include <eb/activity.h>
#include <eb/proc.h>
#include <eb/thread.h>
#include <eb/pmm.h>
#include <eb/kheap.h>
#include <eb/time.h>
#include <eb/string.h>
#include <eb/io.h>

#define ACTIVITY_BYTES 2048

static object *activity;

/* The previous reading, for turning totals into shares. Rows are
 * matched by process id; a fresh process starts from zero. */
static u64 last_wall;
static u64 last_idle;
static u64 last_pid[32];
static u64 last_ran[32];

object *activity_object(void) { return activity; }

bool activity_create(void)
{
    if (activity) return true;
    activity = obj_create(TYPE_TEXT, ACTIVITY_BYTES, 0);
    if (!activity) return false;
    obj_set_name(activity, "activity");
    return true;
}

void activity_adopt(object *o)
{
    if (!o || obj_type(o) != TYPE_TEXT) return;
    if (activity) obj_release(activity);
    obj_retain(o);
    activity = o;
}

/* ------------------------------------------------------------------ */

static u64 put(char *d, u64 at, const char *s)
{
    while (*s) d[at++] = *s++;
    return at;
}

static u64 put_dec(char *d, u64 at, u64 v)
{
    char tmp[24];
    u32 n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) d[at++] = tmp[--n];
    return at;
}

/* Right-aligned in `width`, space-padded, for the columns. */
static u64 put_num(char *d, u64 at, u64 v, u32 width)
{
    char tmp[24];
    u32 n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    for (u32 pad = n; pad < width; pad++) d[at++] = ' ';
    while (n) d[at++] = tmp[--n];
    return at;
}

void activity_update(void)
{
    if (!activity) return;
    char *d = (char *)obj_data(activity);
    if (!d) return;

    u64 now = time_ns();
    u64 wall_delta = now - last_wall;
    if (wall_delta == 0) wall_delta = 1;

    u64 idle = sched_idle_ns();
    u64 idle_delta = idle - last_idle;
    u64 busy_pct = 100 - (idle_delta * 100 / wall_delta > 100
                          ? 100 : idle_delta * 100 / wall_delta);

    u64 at = 0;

    u64 up = now / 1000000000ULL;
    at = put(d, at, "up        ");
    if (up >= 3600) { at = put_dec(d, at, up / 3600); at = put(d, at, "h "); }
    at = put_dec(d, at, (up / 60) % 60);
    at = put(d, at, "m ");
    at = put_dec(d, at, up % 60);
    at = put(d, at, "s\n");

    at = put(d, at, "cpu       ");
    at = put_dec(d, at, busy_pct);
    at = put(d, at, "% busy\n");

    u64 used_mib  = pmm_used_frames() * 4096 / (1024 * 1024);
    u64 total_mib = pmm_total_frames() * 4096 / (1024 * 1024);
    at = put(d, at, "memory    ");
    at = put_dec(d, at, used_mib);
    at = put(d, at, " of ");
    at = put_dec(d, at, total_mib);
    at = put(d, at, " MiB\n");

    at = put(d, at, "heap      ");
    at = put_dec(d, at, kheap_bytes_used() / 1024);
    at = put(d, at, " KiB\n");

    at = put(d, at, "objects   ");
    at = put_dec(d, at, obj_live_count());
    at = put(d, at, "\n");

    at = put(d, at, "threads   ");
    at = put_dec(d, at, sched_threads());
    at = put(d, at, ", ");
    at = put_dec(d, at, sched_switches());
    at = put(d, at, " switches\n\n");

    at = put(d, at, " id  program    cpu  holds\n");

    u32 count = proc_live_count();
    for (u32 i = 0; i < count && i < 32 && at + 64 < ACTIVITY_BYTES; i++) {
        const char *name = "?";
        u64 id = 0, holds = 0, ran = 0;
        if (!proc_live_at(i, &name, &id, &holds, &ran)) break;

        /* The share since the last look, matched to the same process
         * last time -- or a newborn, whose past hour is not held
         * against it. */
        u64 before = (last_pid[i] == id) ? last_ran[i] : ran;
        u64 pct = (ran - before) * 100 / wall_delta;
        if (pct > 99) pct = 99;
        last_pid[i] = id;
        last_ran[i] = ran;

        at = put_num(d, at, id, 3);
        at = put(d, at, "  ");
        u64 col = at;
        at = put(d, at, name);
        while (at < col + 11 && at < ACTIVITY_BYTES - 1) d[at++] = ' ';
        at = put_num(d, at, pct, 2);
        at = put(d, at, "%");
        at = put_num(d, at, holds, 7);
        at = put(d, at, "\n");
    }

    /* Whatever stood here before ends exactly where the new text does. */
    memset(d + at, 0, ACTIVITY_BYTES - at);

    last_wall = now;
    last_idle = idle;
}
