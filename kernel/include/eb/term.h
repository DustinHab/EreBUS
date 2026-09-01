/*
 * term.h -- the terminal: the system spoken to in lines.
 *
 * The core knows nothing of screens. It holds a standpoint in the
 * graph -- the same kind of walk the shell makes with clicks -- takes
 * one line at a time, and appends what it has to say to a transcript.
 * The screen's terminal view is one feeder; a remote line arriving
 * over the network later is another, and it will use exactly this
 * interface. That is the whole design: the terminal is a place words
 * go, not a place pixels come from.
 */
#ifndef EB_TERM_H
#define EB_TERM_H

#include <eb/types.h>
#include <eb/object.h>

/* Where every walk starts, and with what in hand. */
void term_init(object *root, u32 rights);

/* One command in. The reply lands on the transcript. */
void term_line(const char *line);

/* The transcript, oldest first. Grows until full, then the older
 * half makes room, like the journal. */
const char *term_out(u64 *len);

/* Bumps whenever the transcript changes, so a view knows to look. */
u64 term_sequence(void);

/* The line being gathered, for a view that shows typing as it
 * happens. A remote feeder sends whole lines and never uses these. */
const char *term_gather(u32 *len);
void term_key(char c);
void term_rub(void);
void term_clear_line(void);
void term_recall(void);              /* bring back the last line */
void term_enter(void);

#endif /* EB_TERM_H */
