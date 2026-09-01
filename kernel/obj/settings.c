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
    bool peer_set;
    u8   peer_ip[4];
    u16  peer_port;
    bool addr_set;
    u8   addr_ip[4];
    char name[24];
} values;

static values current = { DEFAULT_QUIET_NS, 0, 1, 1, 50, true, false, false,
                          false, { 0, 0, 0, 0 }, 0,
                          false, { 0, 0, 0, 0 }, "erebus" };

object *settings_object(void) { return settings; }

u64  settings_save_quiet_ns(void)   { return current.save_quiet_ns; }
i64  settings_clock_offset_min(void){ return current.clock_offset_min; }
bool settings_hints(void)           { return current.hints; }
bool settings_light(void)           { return current.light; }
bool settings_start_home(void)      { return current.start_home; }

bool settings_peer(u8 ip[4], u16 *port)
{
    if (!current.peer_set) return false;
    if (ip) for (u32 i = 0; i < 4; i++) ip[i] = current.peer_ip[i];
    if (port) *port = current.peer_port;
    return true;
}

bool settings_address(u8 ip[4])
{
    if (!current.addr_set) return false;
    if (ip) for (u32 i = 0; i < 4; i++) ip[i] = current.addr_ip[i];
    return true;
}

void settings_name(char *out, u32 max)
{
    u32 i = 0;
    while (current.name[i] && i < max - 1) { out[i] = current.name[i]; i++; }
    out[i] = 0;
}

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
    "start    | where i left\n"
    "name     | erebus\n"
    "address  | by lease\n"
    "peer     | nobody\n";

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
    } else if (matter_is(line, a, b, "peer")) {
        /* Five numbers make a peer -- four of address, one of port --
         * and the punctuation between them is anyone's: "10.0.2.2 7802"
         * and "10.0.2.2:7802" both say the same thing. Anything short
         * of five numbers, "nobody" included, names no one. */
        u64 nums[5];
        u32 got = 0;
        for (u64 i = 0; i < vlen && got < 5; i++) {
            if (val[i] < '0' || val[i] > '9') continue;
            u64 g = 0;
            while (i < vlen && val[i] >= '0' && val[i] <= '9')
                g = g * 10 + (u64)(val[i++] - '0');
            nums[got++] = g;
        }
        if (got == 5 && nums[0] < 256 && nums[1] < 256 &&
            nums[2] < 256 && nums[3] < 256 && nums[4] < 65536 &&
            nums[4] > 0) {
            for (u32 i = 0; i < 4; i++) v->peer_ip[i] = (u8)nums[i];
            v->peer_port = (u16)nums[4];
            v->peer_set = true;
        } else {
            v->peer_set = false;
        }
    } else if (matter_is(line, a, b, "address")) {
        /* Four numbers claim an address of our own instead of asking
         * the network for one -- what a wire with no landlord needs.
         * Anything else, "by lease" included, means asking. */
        u64 nums[4];
        u32 got = 0;
        for (u64 i = 0; i < vlen && got < 4; i++) {
            if (val[i] < '0' || val[i] > '9') continue;
            u64 g = 0;
            while (i < vlen && val[i] >= '0' && val[i] <= '9')
                g = g * 10 + (u64)(val[i++] - '0');
            nums[got++] = g;
        }
        if (got == 4 && nums[0] > 0 && nums[0] < 256 && nums[1] < 256 &&
            nums[2] < 256 && nums[3] > 0 && nums[3] < 255) {
            for (u32 i = 0; i < 4; i++) v->addr_ip[i] = (u8)nums[i];
            v->addr_set = true;
        } else {
            v->addr_set = false;
        }
    } else if (matter_is(line, a, b, "name")) {
        /* What this machine calls itself when another asks: shown to
         * the other side as a claim, like every self-given name. */
        u64 from = 0, to = vlen;
        while (from < to && val[from] == ' ') from++;
        while (to > from && (val[to-1] == ' ' || val[to-1] == '\r')) to--;
        u32 n = 0;
        for (u64 i = from; i < to && n < sizeof(v->name) - 1; i++)
            if ((u8)val[i] >= 0x20 && (u8)val[i] < 0x7F)
                v->name[n++] = val[i];
        v->name[n] = 0;
        if (n == 0) {
            const char *fb = "erebus";
            for (n = 0; fb[n]; n++) v->name[n] = fb[n];
            v->name[n] = 0;
        }
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

    if (strcmp(was->name, now->name) != 0)
        journal_says("settings", "the machine goes by a new name");

    if (was->addr_set != now->addr_set ||
        (now->addr_set && memcmp(was->addr_ip, now->addr_ip, 4) != 0))
        journal_says("settings", now->addr_set
                     ? "the machine claims its own address now"
                     : "the machine asks for its address again");

    if (was->peer_set != now->peer_set ||
        (now->peer_set &&
         (memcmp(was->peer_ip, now->peer_ip, 4) != 0 ||
          was->peer_port != now->peer_port)))
        journal_says("settings", now->peer_set
                     ? "the pipe points at a peer now"
                     : "the pipe points at nobody");

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

    values next = { DEFAULT_QUIET_NS, 0, 1, 1, 50, true, false, false,
                    false, { 0, 0, 0, 0 }, 0,
                    false, { 0, 0, 0, 0 }, "erebus" };

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
