#ifndef EB_OBJECT_H
#define EB_OBJECT_H

#include <eb/types.h>

/* The object store.
 *
 * This is the part that makes Erebus not a Unix. There are no files and
 * no paths. There is a graph of typed objects, and the only way to
 * reach one is to already hold a reference to it.
 *
 * Note what is deliberately absent from this header: there is no
 * obj_find(), no obj_open_by_name(), no way to enumerate everything in
 * existence. That absence is the whole security argument. A program
 * cannot ask for an object it was not handed, because there is no
 * namespace in which to name one. Path traversal, symlink races, a
 * library quietly reading somewhere it should not -- all of it needs a
 * namespace to work in, and there is none.
 *
 * An object is three things: a type, a payload, and a set of outgoing
 * references to other objects. The references are what make this a
 * graph rather than a heap of blobs, and they are what the persistence
 * layer will walk when it snapshots the system.
 */

typedef u32 type_id;
typedef struct object object;

/* Built-in types. More can be registered at run time, and a type
 * descriptor is itself an object -- the system describes itself in its
 * own terms rather than in a table the kernel keeps privately. */
#define TYPE_NULL    0u
#define TYPE_TYPE    1u   /* describes a type */
#define TYPE_BYTES   2u   /* uninterpreted bytes */
#define TYPE_TEXT    3u   /* UTF-8 text */
#define TYPE_LIST    4u   /* an ordered set of references */
#define TYPE_DOMAIN  5u   /* a protection domain */
#define TYPE_BUILTIN_COUNT 6u

void obj_store_init(void);

/* Creates an object with room for payload_size bytes and slot_count
 * outgoing references. Comes back with one reference already held by
 * the caller. */
object *obj_create(type_id type, u64 payload_size, u64 slot_count);

void obj_retain(object *o);

/* Drops one reference. At zero the payload is cleared and the object is
 * gone. Clearing matters: the memory returns to the heap, and whatever
 * the object held must not travel with it. */
void obj_release(object *o);

type_id obj_type(const object *o);
u64     obj_id(const object *o);
u64     obj_refs(const object *o);
void   *obj_data(object *o);
u64     obj_size(const object *o);
u64     obj_slots(const object *o);

/* Outgoing references. Setting a slot takes a reference on the target
 * and drops whatever was there before. */
bool    obj_set_slot(object *o, u64 index, object *target);
object *obj_get_slot(object *o, u64 index);

/* Type registry. */
type_id     type_register(const char *name);
const char *type_name(type_id t);
u32         type_count(void);

/* A mark bit for graph walks: snapshotting now, cycle collection later. */
bool obj_marked(const object *o);
void obj_set_mark(object *o, bool marked);

u64 obj_live_count(void);
u64 obj_total_created(void);

bool obj_selftest(void);

#endif /* EB_OBJECT_H */
