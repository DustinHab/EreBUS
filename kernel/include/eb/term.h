/*
 * term.h -- the terminal: the system spoken to in lines.
 *
 * The core knows nothing of screens. A session holds a standpoint in
 * the graph -- the same kind of walk the shell makes with clicks --
 * takes one line at a time, and appends what it has to say to its
 * transcript. The screen's terminal view is one feeder of one
 * session; a visitor who came in through the door over the network
 * gets a session of their own and speaks through exactly this
 * interface. That is the whole design: the terminal is a place words
 * go, not a place pixels come from.
 */
#ifndef EB_TERM_H
#define EB_TERM_H

#include <eb/types.h>
#include <eb/object.h>

typedef struct term_session term_session;

/* Where every walk starts, and with what in hand. Opens the screen's
 * own session. */
void term_init(object *root, u32 rights);

/* The screen's session; and sessions for visitors, opened at the
 * same beginning with the same rights, closed when they leave. */
term_session *term_screen(void);
term_session *term_open(void);
void          term_close(term_session *s);

/* One command in. The reply lands on the transcript. */
void term_line(term_session *s, const char *line);

/* The transcript, oldest first. Grows until full, then the older
 * half makes room, like the journal. term_total counts every byte
 * ever written, so a reader that keeps its own count can tell what
 * is new even after the ring has turned. */
const char *term_out(term_session *s, u64 *len);
u64         term_total(term_session *s);

/* Bumps whenever the transcript changes, so a view knows to look. */
u64 term_sequence(term_session *s);

/* The line being gathered, for a view that shows typing as it
 * happens. A feeder that has whole lines uses term_line directly. */
const char *term_gather(term_session *s, u32 *len);

/* Building, for the terminal and the shell's chip alike: every c and
 * assembly text in the list becomes an object, the objects are linked
 * -- a kernel when one lays down kmain, an image otherwise -- and the
 * result lies in the list. Each line of the report goes to say. */
typedef void (*term_say_fn)(void *ctx, const char *line);
bool term_build_list(object *list, const char *name, term_say_fn say, void *ctx);
bool term_link_list(object *list, const char *name, term_say_fn say, void *ctx);

/* The same build, in a thread of its own, reporting to the journal:
 * the shell keeps drawing while the compiler works. One build at a
 * time; false when one is running already, or no thread could be
 * had. The tools' tables are shared, so compile, assemble and link
 * wait while a build runs -- term_building says so. */
bool term_build_start(object *list, const char *name);
bool term_building(void);
bool term_secret(term_session *s);        /* the line being gathered is a passphrase: show dots */

/* Bytes coming in whole, after 'receive <n> bytes as <name>': while
 * term_taking says so, whatever arrives on the session is contents,
 * not words. term_take_bytes takes as many as are still owed and
 * answers how many it took; the rest are words again. */
bool term_taking(term_session *s);
u32  term_take_bytes(term_session *s, const u8 *d, u32 n);
void term_key(term_session *s, char c);
void term_rub(term_session *s);
void term_clear_line(term_session *s);
void term_recall(term_session *s);           /* bring back the last line */
void term_enter(term_session *s);

#endif /* EB_TERM_H */
