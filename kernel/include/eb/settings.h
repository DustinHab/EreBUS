#ifndef EB_SETTINGS_H
#define EB_SETTINGS_H

#include <eb/types.h>
#include <eb/object.h>

/* Settings are a text object.
 *
 * There is no dialogue and no menu: the system holds one text and
 * reads it, and whoever holds that text with the right to write sets
 * the system by writing sentences into it. The rule fits the editor
 * exactly -- typing appends, so on every matter the last line wins,
 * and changing your mind means adding your new decision at the end.
 * The older lines above it stop being settings and become history.
 *
 * They apply as they are typed: the moment a line comes to mean
 * something, it takes effect, and the journal says so.
 */

bool    settings_create(void);
void    settings_adopt(object *o);
object *settings_object(void);

/* Reads the text and applies it. Cheap; call whenever it may have
 * changed. Differences are noted in the journal. */
void settings_apply(void);

/* What currently holds. */
u64  settings_save_quiet_ns(void);
i64  settings_clock_offset_min(void);
void settings_pointer_scale(i32 *num, i32 *den);
bool settings_hints(void);
bool settings_light(void);
bool settings_start_home(void);

#endif /* EB_SETTINGS_H */
