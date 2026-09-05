/*
 * term.h -- terminal interface: a session holds a standpoint in the graph, takes lines, appends to its transcript.
 * - the screen's terminal view and the door are two feeders of the same interface; nothing here draws
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

/* Writes one line to the screen terminal from outside a command -- for a
 * later answer, like an update check's outcome. Call it from the shell's
 * own turn, which owns the screen session. */
void term_note(const char *str);

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

/* The compiler and assembler keep one shared, non-reentrant set of
 * tables, so only one may run at a time -- across the shell, the
 * terminal words and a job compiled for another machine. A caller
 * claims the tools before compiling and releases them after; claim
 * answers false when they are already in use (a build, another
 * compile), and the caller should try again later. term_building
 * reports the same state, so the two interlock. */
bool term_compile_claim(void);
void term_compile_release(void);
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
