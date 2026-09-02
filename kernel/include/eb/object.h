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

/* How long a name may be. Short on purpose: a name is a label, and a
 * label that needs a paragraph is not doing its job. */
#define OBJ_NAME_MAX 32

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
#define TYPE_SESSION 6u   /* where somebody was looking, and how */
#define TYPE_PROGRAM 7u   /* something running outside the kernel */
#define TYPE_PICTURE 8u   /* a width, a height, and one ink per cell */
#define TYPE_BUILTIN_COUNT 9u

/* A picture's payload: two little-endian u32 for width and height,
 * then width*height bytes, each naming one of sixteen inks. Nothing
 * else -- a format small enough that every lens can honestly claim to
 * show all of it. */
#define PICTURE_HEADER 8u

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
 * and drops whatever was there before.
 *
 * Each reference carries its own rights, so an object can point at
 * something and still limit what following that reference gets you.
 * Authority narrows on every step and never widens -- the same rule as
 * for capabilities between domains, because it is the same idea. */
bool    obj_set_slot(object *o, u64 index, object *target, u32 rights);
object *obj_get_slot(object *o, u64 index);
u32     obj_slot_rights(object *o, u64 index);

/* Widens the reference table. The object keeps its address, so
 * everything already pointing at it stays valid. */
bool    obj_grow_slots(object *o, u64 count);

/* Names. The one on a reference belongs to whoever holds the
 * referencing object; the one on the object is the object's own claim
 * and is only shown when nobody has supplied their own. Neither can be
 * used to find anything -- there is no lookup, in either direction. */
void        obj_set_name(object *o, const char *name);
const char *obj_name(const object *o);
bool        obj_set_slot_name(object *o, u64 index, const char *name);
const char *obj_slot_name(object *o, u64 index);

/* Type registry. */
type_id     type_register(const char *name);
const char *type_name(type_id t);
u32         type_count(void);

/* Frees what is no longer reachable from any capability table, which is
 * the part reference counting cannot see: a cycle holds itself up. Runs
 * to completion and returns how many objects went.
 *
 * This is not a garbage collector standing between programs and their
 * memory. Nothing is deferred, nothing is paused, and an object with a
 * holder is never touched -- counting still does all of that. This only
 * sweeps up after it. */
u64 obj_collect(void);

/* A mark bit for graph walks: snapshotting and cycle collection. */
bool obj_marked(const object *o);
void obj_set_mark(object *o, bool marked);

u64 obj_live_count(void);
u64 obj_total_created(void);

/* A count of payload changes made past the shell's back -- a program
 * writing through a capability, the network writing a page home.
 * Persistence watches this beside the shell's own count, so what a
 * program wrote survives the next boot without anyone having typed.
 *
 * A fleeting object does not count: the clock's face changes every
 * second by design, and a change that recurs by design is a heartbeat,
 * not history. Without this the machine never falls quiet and either
 * never saves or saves its whole ring full of seconds. */
void obj_touch(object *o);
u64  obj_touches(void);
void obj_set_fleeting(object *o, bool fleeting);

/* A transient object lives until the next boot and is left out of the
 * snapshot: what came in from the exchange disk, and what the tools
 * build. Their persistence is the disk itself -- write out keeps them.
 * A reference to one becomes an empty slot on the disk. */
void obj_set_transient(object *o, bool transient);
bool obj_is_transient(const object *o);
bool obj_is_fleeting(const object *o);

bool obj_selftest(void);
bool obj_collect_selftest(void);

#endif /* EB_OBJECT_H */
