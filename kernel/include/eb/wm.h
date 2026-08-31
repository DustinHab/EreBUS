#ifndef EB_WM_H
#define EB_WM_H

#include <eb/types.h>
#include <eb/cap.h>

/* Windows and the desktop.
 *
 * A window here is not an application with a document inside it. It is
 * a view onto an object, and it holds a capability rather than a name.
 * Three consequences follow, and all three are visible on screen:
 *
 * The same object can be open several times at once, shown differently
 * each time -- as text, as bytes, as its place in the graph. There is
 * no "the" representation, because the object is not a file whose
 * format someone has to guess.
 *
 * Changing it through one window changes it in all of them, with
 * nothing to save and no version to reconcile, because there is only
 * one object and the windows are looking at it rather than at copies.
 *
 * And what a window may do is what its capability allows. A window
 * holding a read-only capability displays the object perfectly well and
 * cannot alter a byte of it -- not because it politely refrains, but
 * because the write never resolves.
 */

typedef enum {
    VIEW_TEXT,      /* the payload as characters */
    VIEW_HEX,       /* the payload as bytes */
    VIEW_INSPECT    /* the object itself: type, identity, references */
} view_kind;

typedef struct window window;

void wm_init(void);

/* Opens a view. The capability, not the object, is what the window
 * holds -- so the window's authority is exactly the capability's. */
window *wm_open(const char *title, i32 x, i32 y, i32 w, i32 h,
                domain *d, cap_handle cap, view_kind view);

/* The desktop: draws, takes input, and never returns. */
void wm_run(void *arg);

u64 wm_frames(void);

/* How often an object has been altered through a window. What the
 * persistence layer watches, so that nothing has to be saved by hand. */
u64 wm_changes(void);

#endif /* EB_WM_H */
