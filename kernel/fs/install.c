/*
 * install.c -- the offer to take a disk, made while the machine starts.
 *
 * This is the same work "disks", "settle" and "yes" do from the
 * terminal, and it calls exactly those; nothing here knows what a
 * partition table looks like. What it adds is the asking. A person who
 * has just written the image to a stick and started a machine with it
 * should not have to already know the words.
 *
 * The rules it keeps are the ones that make an offer safe to accept:
 *
 *   - it is only made when there is nothing to lose by making it, which
 *     means no store yet and at least one disk that is not the one the
 *     machine is running from;
 *   - it is only made when somebody can answer, so a machine with no
 *     keyboard boots straight through instead of waiting at a question
 *     nobody will ever see;
 *   - it names the disk, its size, and what is on it before anything is
 *     written, and it takes no answer but the word "yes", typed out;
 *   - it gives up on its own after a while, so a machine that is merely
 *     restarting unattended does not stop here forever.
 */
#include <eb/install.h>
#include <eb/settle.h>
#include <eb/blk.h>
#include <eb/ps2.h>
#include <eb/xhci.h>
#include <eb/thread.h>
#include <eb/time.h>
#include <eb/fmt.h>

#define ASK_NS   (120ULL * 1000000000ULL)   /* two minutes with nobody typing */

static void say(void *ctx, const char *line)
{
    (void)ctx;
    kprintf("      %s\n", line);
}

/* Waits for one key with a character, or a bare escape or return.
 * Answers 0 when the time runs out. */
static u32 wait_key(u64 limit_ns)
{
    u64 since = time_ns();
    for (;;) {
        key_event k;
        while (ps2_poll_key(&k)) {
            if (!k.down) continue;
            if (k.codepoint) return k.codepoint;
        }
        if (time_ns() - since > limit_ns) return 0;
        sched_yield();
    }
}

/* Reads a line, echoing it. Answers false when the time runs out or
 * escape is pressed. */
static bool wait_line(char *out, u32 max, u64 limit_ns)
{
    u32 n = 0;
    out[0] = 0;
    for (;;) {
        u32 c = wait_key(limit_ns);
        if (c == 0 || c == KEY_ESCAPE) return false;
        if (c == KEY_ENTER) { kprintf("\n"); out[n] = 0; return true; }
        if (c == 8 || c == 127) {
            if (n) { n--; out[n] = 0; kprintf("\b \b"); }
            continue;
        }
        if (c < 0x20 || c > 0x7E || n + 1 >= max) continue;
        out[n++] = (char)c;
        out[n] = 0;
        kprintf("%c", (char)c);
    }
}

/* Is there anything worth offering? A disk, and nowhere to keep
 * anything yet. */
static bool anything_to_take(void)
{
    return blk_store_disk() < 0 && blk_disk_count() > 0;
}

void install_offer(void)
{
    if (!anything_to_take()) return;

    if (!ps2_keyboard_present() && !xhci_keyboards()) {
        kprintf("disk: no store, and no keyboard to be asked with; "
                "starting without a memory\n");
        return;
    }

    kprintf("\n");
    kprintf("disk: this machine keeps nothing between starts yet.  "
            "a disk can be given to it now.\n");
    settle_disks(say, NULL);
    kprintf("\n");
    kprintf("disk: type the number of a disk to give it to the system, "
            "or press escape to start without a memory.\n");
    kprintf("      the disk is emptied: a boot volume with this system, "
            "and the rest for what you make.\n");
    kprintf("disk: > ");

    u32 c = wait_key(ASK_NS);
    if (c == 0) {
        kprintf("\ndisk: nobody answered; starting without a memory.  "
                "the words 'disks' and 'settle' do this later.\n");
        return;
    }
    if (c < '1' || c > '9') {
        kprintf("\ndisk: left alone.  the words 'disks' and 'settle' "
                "do this later.\n");
        return;
    }
    u32 which = c - '0';
    kprintf("%c\n", (char)c);

    if (which > blk_disk_count()) {
        kprintf("disk: there is no disk %u.  starting without a memory.\n", which);
        return;
    }

    /* From here the words themselves: the same offer, the same warning
     * and the same refusals as from the terminal. */
    char what[32];
    what[0] = 'o'; what[1] = 'n'; what[2] = ' ';
    what[3] = 'd'; what[4] = 'i'; what[5] = 's'; what[6] = 'k'; what[7] = ' ';
    what[8] = (char)c; what[9] = 0;
    kprintf("\n");
    settle_plan(what, say, NULL);

    kprintf("\n");
    kprintf("disk: everything on disk %u will be lost.  "
            "write yes to go on, or press escape to leave it alone.\n", which);
    kprintf("disk: > ");

    char answer[16];
    if (!wait_line(answer, sizeof(answer), ASK_NS)) {
        kprintf("\ndisk: left alone; nothing was written.\n");
        return;
    }
    if (!(answer[0] == 'y' && answer[1] == 'e' && answer[2] == 's' && answer[3] == 0)) {
        kprintf("disk: that was not yes; nothing was written.\n");
        return;
    }

    kprintf("\n");
    settle_yes(say, NULL);
    kprintf("\n");
}
