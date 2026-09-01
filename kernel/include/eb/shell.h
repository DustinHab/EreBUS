#ifndef EB_SHELL_H
#define EB_SHELL_H

#include <eb/types.h>
#include <eb/cap.h>
#include <eb/object.h>

/* The shell: how a person moves through the object graph.
 *
 * There is no desktop here, no task bar and no window frame with three
 * buttons in the corner. None of those would mean anything. A task bar
 * exists so buried windows can be found again, but nothing is buried
 * and there are no applications to find. Closing, minimising and
 * maximising manage the lifetime of a document that has to be saved,
 * and there is no such thing here either: a view is somewhere you are
 * looking, and looking away costs nothing and loses nothing.
 *
 * What the system actually needs is different. Move through the graph.
 * Choose how to see what you are looking at. See what you are allowed
 * to do with it. Find your way back -- which, with no namespace, can
 * only mean retracing references.
 *
 * The state below is what that needs, and it is deliberately separate
 * from how it is drawn. Three shells render the same state three ways,
 * and switching between them changes nothing but the drawing.
 */

typedef enum {
    SHELL_FOCUS,     /* one object large, the path beside it */
    SHELL_GRAPH,     /* the reachable graph as nodes and edges */
    SHELL_TILES,     /* the path as columns, side by side */
    SHELL_INDEX,     /* everything reachable, one line each */
    SHELL_SPLIT,     /* two independent walks, side by side */
    SHELL_TERM,      /* the system spoken to in lines */
    SHELL_MODE_COUNT
} shell_mode;

typedef enum {
    LENS_TEXT,       /* the payload as characters */
    LENS_BYTES,      /* the payload as bytes */
    LENS_STRUCTURE,  /* what it is and what it points at */
    LENS_PAINT,      /* the payload as cells of ink, drawable */
    LENS_HTML,       /* a fetched page as prose, links and all */
    LENS_COUNT
} lens_kind;

/* Starts at an object, holding the given rights on it.
 *
 * session may be an object saved by an earlier run, in which case the
 * shell comes up exactly where it was left -- same focus, same lenses,
 * same path. Pass NULL for a fresh start. */
void shell_init(domain *d, object *root, u32 rights, object *session);

/* The shell's own state, kept as an object in the graph like anything
 * else. That is not a trick: it means persistence writes it out with
 * everything else and nothing special had to be arranged for it. */
object *shell_session(void);

/* Whether the session handed to shell_init was actually usable. */
bool shell_resumed(void);

void shell_run(void *arg);

/* How often an object was altered through the shell. Persistence
 * watches this so that nothing has to be saved by hand. */
u64 shell_changes(void);

/* Where the shell currently is, so the state can be restored. */
object    *shell_focus(void);
shell_mode shell_current_mode(void);

#endif /* EB_SHELL_H */
