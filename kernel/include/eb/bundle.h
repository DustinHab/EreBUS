#ifndef EB_BUNDLE_H
#define EB_BUNDLE_H

#include <eb/types.h>
#include <eb/object.h>

/* Bundles: a list folded into one plain thing, and back.
 *
 * The pipe carries single flat objects; a bundle is how a whole list
 * travels. Packing walks the list -- texts, bytes, pictures, and
 * lists inside lists -- and lays their substance into one BYTES
 * object, petnames along. Unpacking builds the list again from it,
 * every entry a fresh object. Substance only, like the pipe itself:
 * no rights, no programs, no reach back; what cannot be read is not
 * packed, and a loop in the graph is walked once, not forever.
 */

/* Folds the list into a fresh BYTES object, or NULL when it will not
 * fit in one bundle (64 KiB, the pipe's own limit). The caller owns
 * the returned reference. */
object *bundle_pack(object *list);

/* Whether these bytes begin like a bundle. */
bool bundle_smells(object *bytes);

/* Builds a fresh list from a bundle, or NULL when the bytes do not
 * parse. The caller owns the returned reference. */
object *bundle_unpack(object *bytes);

#endif /* EB_BUNDLE_H */
