/*
 * journal.c -- the event log as an ordinary text object.
 * - shown through the lenses, written by the snapshot, seen as it stood in time travel
 * - the kernel holds the only writable path; the graph holds it read-only
 */
#include <eb/journal.h>
#include <eb/string.h>
#include <eb/time.h>
#include <eb/io.h>

#define JOURNAL_BYTES 8192

static object *journal;
static u64 sequence;

object *journal_object(void)  { return journal; }
u64     journal_sequence(void){ return sequence; }

bool journal_create(void)
{
    if (journal) return true;
    journal = obj_create(TYPE_TEXT, JOURNAL_BYTES, 0);
    /* The record changes by design: it is nobody's edit, and it stays
     * in the generation rather than costing the log an entry per save. */
    if (journal) obj_set_fleeting(journal, true);
    if (!journal) return false;
    obj_set_name(journal, "log");
    return true;
}

void journal_adopt(object *o)
{
    if (!o || obj_type(o) != TYPE_TEXT) return;
    if (journal) obj_release(journal);
    obj_retain(o);
    obj_set_fleeting(o, true);  /* a restored record is a record still */
    journal = o;
    sequence++;                 /* whoever displays it should look again */
}

static u64 line_len(const u8 *d, u64 size)
{
    u64 n = 0;
    while (n < size && d[n]) n++;
    return n;
}

void journal_says(const char *who, const char *what)
{
    if (!journal || !who || !what) return;

    /* The line is composed first, appended second, so the time under
     * interrupts-off covers only the copy. */
    char line[112];
    u64 at = 0;

    u64 secs = time_ns() / 1000000000ULL;
    char digits[24];
    u64 nd = 0;
    if (secs == 0) digits[nd++] = '0';
    while (secs) { digits[nd++] = (char)('0' + secs % 10); secs /= 10; }
    for (u64 pad = nd; pad < 4; pad++) line[at++] = ' ';
    while (nd) line[at++] = digits[--nd];
    line[at++] = 's';
    line[at++] = ' ';
    line[at++] = ' ';

    for (u64 i = 0; who[i] && at < sizeof(line) - 4; i++) line[at++] = who[i];
    line[at++] = ':';
    line[at++] = ' ';
    for (u64 i = 0; what[i] && at < sizeof(line) - 2; i++) {
        char c = what[i];
        line[at++] = (c >= 32 && c < 127) ? c : ' ';
    }
    /* Trailing spaces carry nothing; programs pad their words to eight. */
    while (at > 0 && line[at - 1] == ' ') at--;
    line[at++] = '\n';

    u64 flags = irq_save();

    u8 *d = (u8 *)obj_data(journal);
    u64 size = obj_size(journal);
    if (!d || size < sizeof(line) + 2) { irq_restore(flags); return; }

    u64 len = line_len(d, size);

    /* When the page is full, the oldest half makes room -- cut at a
     * line boundary, so nothing survives torn in the middle. */
    if (len + at + 1 > size) {
        u64 from = len / 2;
        while (from < len && d[from] != '\n') from++;
        if (from < len) from++;
        memmove(d, d + from, len - from);
        len -= from;
        memset(d + len, 0, size - len);
    }

    memcpy(d + len, line, at);
    d[len + at] = 0;
    sequence++;

    irq_restore(flags);
}

bool journal_latest(char *out, u64 max)
{
    if (!journal || !out || max == 0) return false;

    u64 flags = irq_save();
    const u8 *d = (const u8 *)obj_data(journal);
    u64 len = d ? line_len(d, obj_size(journal)) : 0;
    if (len == 0) { irq_restore(flags); return false; }

    u64 end = len;
    while (end > 0 && d[end - 1] == '\n') end--;
    u64 start = end;
    while (start > 0 && d[start - 1] != '\n') start--;

    u64 n = 0;
    while (start + n < end && n < max - 1) { out[n] = (char)d[start + n]; n++; }
    out[n] = 0;
    irq_restore(flags);
    return n > 0;
}
