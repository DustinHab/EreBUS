/*
 * settings.c -- the system, set by a table.
 *
 * The whole configuration machinery is: one text object, and this file
 * reading it. Each row is a matter and its value, split at the bar.
 * The matter is matched exactly against the left column -- a row sets
 * what it names and nothing else, so no value, note or slip of the
 * keyboard can reach a different setting by containing the wrong word.
 * The value stays free wording, because "1 second" and "where i left"
 * are worth more than an enum. Lines without a bar are prose and have
 * no effect. Should a matter appear twice, the later row is believed.
 *
 * Nothing here is privileged about the object itself. It is reachable
 * like anything else, snapshotted like anything else, and shown
 * through the same lenses; hand somebody the reference read-only and
 * they can see how the system is set without being able to set it.
 */
#include <eb/settings.h>
#include <eb/journal.h>
#include <eb/string.h>
#include <eb/thread.h>
#include <eb/time.h>
#include <eb/io.h>

static object *settings;

/* The defaults, which are also what an empty text means. */
#define DEFAULT_QUIET_NS 1000000000ULL

typedef struct {
    u64  save_quiet_ns;
    i64  clock_offset_min;
    i32  pointer_num, pointer_den;
    u32  slice_ms;
    bool hints;
    bool light;
    bool start_home;
} values;

static values current = { DEFAULT_QUIET_NS, 0, 1, 1, 50, true, false, false };

object *settings_object(void) { return settings; }

u64  settings_save_quiet_ns(void)   { return current.save_quiet_ns; }
i64  settings_clock_offset_min(void){ return current.clock_offset_min; }
bool settings_hints(void)           { return current.hints; }
bool settings_light(void)           { return current.light; }
bool settings_start_home(void)      { return current.start_home; }

void settings_pointer_scale(i32 *num, i32 *den)
{
    if (num) *num = current.pointer_num;
    if (den) *den = current.pointer_den;
}

/* What a fresh system starts with: a table, one matter per row, the
 * value in the right column. Editing the value is changing the system,
 * as the letters land. No preamble in the object itself -- a table
 * explains itself, and prose in a settings file is noise. Should a
 * matter appear twice, the later row is believed. */
static const char seed[] =
    "theme    | dark\n"
    "save     | 1 second\n"
    "clock    | on time\n"
    "pointer  | normal\n"
    "hints    | shown\n"
    "slice    | 50 ms\n"
    "start    | where i left\n";

bool settings_create(void)
{
    if (settings) return true;
    settings = obj_create(TYPE_TEXT, 2048, 0);
    if (!settings) return false;

    obj_set_name(settings, "settings");
    u8 *d = (u8 *)obj_data(settings);
    for (u32 i = 0; i < sizeof(seed); i++) d[i] = (u8)seed[i];
    return true;
}

void settings_adopt(object *o)
{
    if (!o || obj_type(o) != TYPE_TEXT) return;
    if (settings) obj_release(settings);
    obj_retain(o);
    settings = o;
}

/* ------------------------------------------------------------------ */
/* Reading sentences                                                   */
/* ------------------------------------------------------------------ */

static bool line_has(const char *line, u64 len, const char *word)
{
    u64 wl = strlen(word);
    if (wl > len) return false;
    for (u64 i = 0; i + wl <= len; i++) {
        u64 j = 0;
        while (j < wl && line[i + j] == word[j]) j++;
        if (j == wl) return true;
    }
    return false;
}

static bool line_number(const char *line, u64 len, u64 *out)
{
    for (u64 i = 0; i < len; i++) {
        if (line[i] < '0' || line[i] > '9') continue;
        u64 v = 0;
        while (i < len && line[i] >= '0' && line[i] <= '9') {
            v = v * 10 + (u64)(line[i] - '0');
            i++;
        }
        *out = v;
        return true;
    }
    return false;
}

/* Whether the left column says exactly this matter, spaces aside. */
static bool matter_is(const char *line, u64 a, u64 b, const char *word)
{
    u64 wl = strlen(word);
    if (b - a != wl) return false;
    for (u64 i = 0; i < wl; i++)
        if (line[a + i] != word[i]) return false;
    return true;
}

static void read_line(values *v, const char *line, u64 len)
{
    /* The matter is the left column and nothing else. A row sets only
     * what its own first column names -- exactly, not by resemblance --
     * and a line without the bar is prose and stays prose. Inside the
     * value the wording remains free ("1 second", "where i left"): the
     * column decides WHAT is being set, the words decide what it is
     * set TO, and neither can reach across into the other's job. */
    u64 bar = 0;
    while (bar < len && line[bar] != '|') bar++;
    if (bar == len) return;

    u64 a = 0, b = bar;
    while (a < b && line[a] == ' ') a++;
    while (b > a && line[b - 1] == ' ') b--;

    const char *val = line + bar + 1;
    u64 vlen = len - bar - 1;
    u64 n;

    if (matter_is(line, a, b, "save")) {
        if (line_number(val, vlen, &n)) {
            if (n > 60) n = 60;
            v->save_quiet_ns = n * 1000000000ULL;
        }
    } else if (matter_is(line, a, b, "clock")) {
        if (line_has(val, vlen, "on time")) {
            v->clock_offset_min = 0;
        } else if (line_number(val, vlen, &n)) {
            if (n > 23) n = 23;
            if (line_has(val, vlen, "ahead"))  v->clock_offset_min = (i64)n * 60;
            if (line_has(val, vlen, "behind")) v->clock_offset_min = -(i64)n * 60;
        }
    } else if (matter_is(line, a, b, "pointer")) {
        if (line_has(val, vlen, "slow"))        { v->pointer_num = 1; v->pointer_den = 2; }
        else if (line_has(val, vlen, "quick") ||
                 line_has(val, vlen, "fast"))   { v->pointer_num = 2; v->pointer_den = 1; }
        else if (line_has(val, vlen, "normal")) { v->pointer_num = 1; v->pointer_den = 1; }
    } else if (matter_is(line, a, b, "hints")) {
        if (line_has(val, vlen, "hidden")) v->hints = false;
        if (line_has(val, vlen, "shown"))  v->hints = true;
    } else if (matter_is(line, a, b, "theme") ||
               matter_is(line, a, b, "colors")) {
        if (line_has(val, vlen, "light")) v->light = true;
        if (line_has(val, vlen, "dark"))  v->light = false;
    } else if (matter_is(line, a, b, "slice")) {
        if (line_number(val, vlen, &n)) {
            if (n < 10)  n = 10;
            if (n > 500) n = 500;
            v->slice_ms = (u32)n;
        }
    } else if (matter_is(line, a, b, "start")) {
        if (line_has(val, vlen, "home")) v->start_home = true;
        if (line_has(val, vlen, "left")) v->start_home = false;
    }
}

/* Says what changed, in the settings' own words. */
static void note_changes(const values *was, const values *now)
{
    char line[48];
    u64 at;

    if (was->light != now->light)
        journal_says("settings", now->light ? "the theme is light now"
                                            : "the theme is dark now");
    if (was->hints != now->hints)
        journal_says("settings", now->hints ? "hints are shown again"
                                            : "hints are hidden now");
    if (was->pointer_num != now->pointer_num ||
        was->pointer_den != now->pointer_den)
        journal_says("settings",
                     now->pointer_num > now->pointer_den ? "the pointer moves quickly now"
                     : now->pointer_num < now->pointer_den ? "the pointer moves slowly now"
                     : "the pointer moves normally again");

    if (was->save_quiet_ns != now->save_quiet_ns) {
        at = 0;
        const char *p = "saving after ";
        while (*p) line[at++] = *p++;
        u64 secs = now->save_quiet_ns / 1000000000ULL;
        if (secs >= 10) line[at++] = (char)('0' + secs / 10);
        line[at++] = (char)('0' + secs % 10);
        p = secs == 1 ? " second of quiet" : " seconds of quiet";
        while (*p) line[at++] = *p++;
        line[at] = 0;
        journal_says("settings", line);
    }

    if (was->slice_ms != now->slice_ms) {
        at = 0;
        const char *p = "the slice is ";
        while (*p) line[at++] = *p++;
        u64 ms = now->slice_ms;
        if (ms >= 100) line[at++] = (char)('0' + ms / 100);
        if (ms >= 10)  line[at++] = (char)('0' + (ms / 10) % 10);
        line[at++] = (char)('0' + ms % 10);
        p = " ms now";
        while (*p) line[at++] = *p++;
        line[at] = 0;
        journal_says("settings", line);
    }

    if (was->start_home != now->start_home)
        journal_says("settings", now->start_home
                     ? "the next start is at home"
                     : "the next start is where you left");

    if (was->clock_offset_min != now->clock_offset_min) {
        i64 m = now->clock_offset_min;
        if (m == 0) {
            journal_says("settings", "the clock runs on time again");
        } else {
            at = 0;
            const char *p = "the clock now runs ";
            while (*p) line[at++] = *p++;
            u64 h = (u64)(m < 0 ? -m : m) / 60;
            if (h >= 10) line[at++] = (char)('0' + h / 10);
            line[at++] = (char)('0' + h % 10);
            p = m > 0 ? " hours ahead" : " hours behind";
            while (*p) line[at++] = *p++;
            line[at] = 0;
            journal_says("settings", line);
        }
    }
}

void settings_apply(void)
{
    if (!settings) return;

    const u8 *d = (const u8 *)obj_data(settings);
    u64 size = obj_size(settings);
    if (!d) return;

    values next = { DEFAULT_QUIET_NS, 0, 1, 1, 50, true, false, false };

    u64 start = 0;
    for (u64 i = 0; i <= size; i++) {
        bool end = (i == size) || d[i] == 0 || d[i] == '\n';
        if (!end) continue;

        if (i > start) read_line(&next, (const char *)d + start, i - start);
        if (i == size || d[i] == 0) break;
        start = i + 1;
    }

    if (memcmp(&current, &next, sizeof(values)) != 0) {
        values was = current;
        current = next;
        note_changes(&was, &current);

        /* The one value someone else keeps: the scheduler holds the
         * slice, so the new length is handed over in ticks. */
        if (was.slice_ms != current.slice_ms) {
            u32 hz = pit_hz();
            if (hz == 0) hz = 100;
            u32 t = current.slice_ms * hz / 1000;
            sched_set_slice_ticks(t ? t : 1);
        }
    }
}
