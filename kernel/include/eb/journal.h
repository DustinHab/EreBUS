#ifndef EB_JOURNAL_H
#define EB_JOURNAL_H

#include <eb/types.h>
#include <eb/object.h>

/* What has happened, as an object.
 *
 * Not a file, and not a kernel buffer that some tool would have to be
 * written to read. The record of what the system did is a text object
 * in the graph: looked at through the same lenses as everything else,
 * written to disk with everything else, and held through a read-only
 * reference -- so history can be read by anyone who holds it and
 * rewritten by nobody, including its own author's slip of the hand.
 */

/* Creates the journal if none exists yet. */
bool journal_create(void);

/* Continues an existing journal, typically one that came back from
 * disk. Appending resumes into it; no second history starts beside it. */
void journal_adopt(object *o);

object *journal_object(void);

/* Appends one line: the uptime, who, and what. Safe from any thread. */
void journal_says(const char *who, const char *what);

/* Copies the newest line into out (truncated to max, always
 * terminated). Returns false while the journal is empty. */
bool journal_latest(char *out, u64 max);

/* Grows with every line written; whoever displays the journal can poll
 * this instead of the text. */
u64 journal_sequence(void);

/* Attention: the subset of the log worth noticing, kept in its own text
 * object so the person can find open matters without reading everything.
 * attention_note writes both the full log and the attention text, and
 * raises the unseen count; attention_seen clears it (the shell calls it
 * when the attention object is the focus). */
bool    attention_create(void);
void    attention_adopt(object *o);
object *attention_object(void);
void    attention_note(const char *who, const char *what);
u32     attention_unseen(void);
void    attention_seen(void);

#endif /* EB_JOURNAL_H */
