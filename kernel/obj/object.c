/*
 * object.c -- the object store.
 *
 * Every object is a header followed by its payload and its outgoing
 * reference slots, in one allocation. One allocation rather than three
 * because an object is a unit: it is created whole, destroyed whole,
 * and will one day be written to disk whole.
 *
 * Lifetime is by reference count. An object exists as long as somebody
 * holds a reference to it -- a capability in some domain, or a slot in
 * another object. When the last one goes, so does the object.
 *
 * Reference counting cannot collect a cycle: two objects pointing at
 * each other keep each other alive after everything else has let go.
 * That is a known and accepted gap here. The answer is not a cleverer
 * count but a collector that walks the graph from its roots, and this
 * system will need one anyway -- persistence works by walking exactly
 * that graph. A cycle leaks memory until then; it cannot leak
 * authority, which is the property that actually matters.
 */
#include <eb/object.h>
#include <eb/cap.h>
#include <eb/kheap.h>
#include <eb/fmt.h>
#include <eb/panic.h>

#define OBJ_MAGIC 0x4F424A454354ULL   /* "OBJECT" */

/* An outgoing reference carries its own rights.
 *
 * Without them a reference would be a bare pointer and every step
 * through the graph would hand over everything the target can do. With
 * them, following a reference narrows what you hold in exactly the way
 * passing a capability to another domain does -- an object can point at
 * something and let readers of itself only read it. The rule is the
 * same everywhere: authority never grows by being followed. */
typedef struct {
    object *target;
    u32     rights;
    u32     _pad;
    char    name[OBJ_NAME_MAX];   /* what the holder calls it */
} obj_slot;

struct object {
    u64     magic;
    u64     id;
    type_id type;
    u32     _pad;
    u64     refs;
    u64     size;      /* payload bytes */
    u64     nslots;    /* outgoing references */
    u32     mark;      /* used by graph walks; not part of the object */
    u32     _mark_pad;
    char    name[OBJ_NAME_MAX];   /* what the object calls itself */
    obj_slot *slots;              /* its own allocation, so it can grow */
    /* the payload follows this header */
};

static inline u8 *payload_of(object *o)
{
    return (u8 *)o + sizeof(object);
}

/* The slots live in their own allocation rather than behind the
 * payload.
 *
 * One allocation for the whole object is tidier and was the original
 * arrangement, but it makes an object's shape permanent: adding a
 * reference means moving the object, and everything pointing at it --
 * capabilities, other objects' slots -- would be left pointing at the
 * old address. With the header staying put, growing is a matter of
 * replacing one array, and every reference to the object survives it. */
static inline obj_slot *slots_of(object *o)
{
    return o->slots;
}

static u64 next_id = 1;
static u64 live_objects;
static u64 created_objects;

/* ------------------------------------------------------------------ */
/* Type registry                                                       */
/* ------------------------------------------------------------------ */

#define MAX_TYPES 64

static const char *type_names[MAX_TYPES];
static u32 types_registered;

void obj_store_init(void)
{
    types_registered = 0;
    type_names[TYPE_NULL]   = "null";
    type_names[TYPE_TYPE]   = "type";
    type_names[TYPE_BYTES]  = "bytes";
    type_names[TYPE_TEXT]   = "text";
    type_names[TYPE_LIST]   = "list";
    type_names[TYPE_DOMAIN] = "domain";
    type_names[TYPE_SESSION] = "session";
    types_registered = TYPE_BUILTIN_COUNT;

    next_id = 1;
    live_objects = 0;
    created_objects = 0;
}

type_id type_register(const char *name)
{
    if (types_registered >= MAX_TYPES) return TYPE_NULL;
    type_names[types_registered] = name;
    return types_registered++;
}

const char *type_name(type_id t)
{
    if (t >= types_registered || !type_names[t]) return "unknown";
    return type_names[t];
}

u32 type_count(void) { return types_registered; }

/* ------------------------------------------------------------------ */
/* Objects                                                             */
/* ------------------------------------------------------------------ */

static void check(const object *o, const char *where)
{
    if (!o || o->magic != OBJ_MAGIC)
        panic("not an object at %p, found during %s", (const void *)o, where);
}

object *obj_create(type_id type, u64 payload_size, u64 slot_count)
{
    if (type >= types_registered) return NULL;

    object *o = (object *)kzalloc(sizeof(object) + payload_size);
    if (!o) return NULL;

    if (slot_count) {
        o->slots = (obj_slot *)kzalloc(slot_count * sizeof(obj_slot));
        if (!o->slots) { kfree(o); return NULL; }
    }

    o->magic  = OBJ_MAGIC;
    o->id     = next_id++;
    o->type   = type;
    o->refs   = 1;
    o->size   = payload_size;
    o->nslots = slot_count;

    live_objects++;
    created_objects++;
    return o;
}

void obj_retain(object *o)
{
    check(o, "retain");
    o->refs++;
}

void obj_release(object *o)
{
    check(o, "release");

    if (o->refs == 0)
        panic("object %llu released more often than it was held", o->id);

    if (--o->refs > 0) return;

    /* Let go of everything this object was holding, which may in turn
     * be the last reference to those. */
    obj_slot *slots = slots_of(o);
    for (u64 i = 0; i < o->nslots; i++)
        if (slots[i].target) obj_release(slots[i].target);

    /* Wipe the header before the memory goes back. The heap clears the
     * payload on release; the header is ours to clear, and leaving a
     * valid magic behind in freed memory would make a stale pointer
     * look like a live object. */
    o->magic = 0;
    o->id = 0;
    o->type = TYPE_NULL;
    o->nslots = 0;
    o->size = 0;

    live_objects--;
    if (o->slots) kfree(o->slots);
    kfree(o);
}

type_id obj_type(const object *o)  { check(o, "type"); return o->type; }
u64     obj_id(const object *o)    { check(o, "id"); return o->id; }
u64     obj_refs(const object *o)  { check(o, "refs"); return o->refs; }
u64     obj_size(const object *o)  { check(o, "size"); return o->size; }
u64     obj_slots(const object *o) { check(o, "slots"); return o->nslots; }

/* Names.
 *
 * Two kinds, and the difference matters.
 *
 * The name on a reference is the holder's: you wrote it down, about
 * something you already hold. Nothing else can set it. That is what
 * stops an object handed to you from calling itself whatever would
 * best persuade you to use it -- it does not get a say in what you
 * call it.
 *
 * The name on the object is the object's own claim, shown only when
 * nobody has given it one of their own, and never confused with the
 * first. It is a convenience, not evidence.
 *
 * Neither is a namespace. A name here is readable only by whoever
 * already holds the reference, and there is nothing anywhere that
 * turns a name back into an object. */
static void copy_name(char *dst, const char *src)
{
    u32 i = 0;
    if (src) while (src[i] && i < OBJ_NAME_MAX - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

void obj_set_name(object *o, const char *name)
{
    check(o, "set name");
    copy_name(o->name, name);
}

const char *obj_name(const object *o)
{
    check(o, "name");
    return o->name[0] ? o->name : NULL;
}

bool obj_set_slot_name(object *o, u64 index, const char *name)
{
    check(o, "set slot name");
    if (index >= o->nslots) return false;
    copy_name(slots_of(o)[index].name, name);
    return true;
}

const char *obj_slot_name(object *o, u64 index)
{
    check(o, "slot name");
    if (index >= o->nslots) return NULL;
    const char *n = slots_of(o)[index].name;
    return n[0] ? n : NULL;
}

void *obj_data(object *o)
{
    check(o, "data");
    return o->size ? payload_of(o) : NULL;
}

bool obj_set_slot(object *o, u64 index, object *target, u32 rights)
{
    check(o, "set slot");
    if (index >= o->nslots) return false;
    if (target) check(target, "set slot target");

    obj_slot *slots = slots_of(o);
    object *old = slots[index].target;

    if (target) obj_retain(target);
    slots[index].target = target;
    slots[index].rights = target ? rights : 0;
    if (old) obj_release(old);
    return true;
}

/* Makes room for more references.
 *
 * Creating something in this system means pointing at it: an object
 * nobody references is unreachable and gone the moment it is made. So
 * anywhere a person can add something, the holder needs a slot free --
 * and running out of them is not a reason to refuse. */
bool obj_grow_slots(object *o, u64 count)
{
    check(o, "grow");
    if (count <= o->nslots) return true;

    obj_slot *bigger = (obj_slot *)kzalloc(count * sizeof(obj_slot));
    if (!bigger) return false;

    for (u64 i = 0; i < o->nslots; i++) bigger[i] = o->slots[i];
    if (o->slots) kfree(o->slots);

    o->slots = bigger;
    o->nslots = count;
    return true;
}

u32 obj_slot_rights(object *o, u64 index)
{
    check(o, "slot rights");
    if (index >= o->nslots) return 0;
    return slots_of(o)[index].rights;
}

object *obj_get_slot(object *o, u64 index)
{
    check(o, "get slot");
    if (index >= o->nslots) return NULL;
    return slots_of(o)[index].target;
}

/* The mark lives on the object rather than in a table beside it. A walk
 * over a graph needs somewhere to record what it has already seen, and
 * putting that where the object is means the walk costs nothing extra
 * per object and terminates on a cycle. The snapshot writer uses it
 * now; a collector for the cycles reference counting cannot reach will
 * use the same field. */
bool obj_marked(const object *o)     { check(o, "mark"); return o->mark != 0; }
void obj_set_mark(object *o, bool m) { check(o, "mark"); o->mark = m ? 1 : 0; }

u64 obj_live_count(void)    { return live_objects; }
u64 obj_total_created(void) { return created_objects; }

/* ------------------------------------------------------------------ */

bool obj_selftest(void)
{
    u64 live_before = obj_live_count();

    /* A text object holds what was written into it. */
    object *text = obj_create(TYPE_TEXT, 32, 0);
    if (!text) return false;

    u8 *d = (u8 *)obj_data(text);
    const char *msg = "the reference is the permission";
    u64 n = 0;
    while (msg[n] && n < 31) { d[n] = (u8)msg[n]; n++; }

    if (obj_type(text) != TYPE_TEXT || obj_size(text) != 32) return false;
    if (((const u8 *)obj_data(text))[4] != 'r') return false;

    /* A list holding a reference keeps its target alive. */
    object *list = obj_create(TYPE_LIST, 0, 4);
    if (!list) return false;
    if (!obj_set_slot(list, 0, text, CAP_READ | CAP_WRITE)) return false;

    if (obj_refs(text) != 2) {
        kprintf("obj:  slot did not take a reference\n");
        return false;
    }

    /* Our own reference goes; the list still holds one. */
    obj_release(text);
    if (obj_get_slot(list, 0) != text || obj_refs(text) != 1) {
        kprintf("obj:  object died while still referenced\n");
        return false;
    }

    /* Out of range slots are refused rather than wrapping. */
    if (obj_set_slot(list, 99, list, CAP_READ)) {
        kprintf("obj:  accepted a slot index past the end\n");
        return false;
    }

    /* Releasing the list releases what it held. */
    obj_release(list);

    if (obj_live_count() != live_before) {
        kprintf("obj:  %llu objects outlived the test\n",
                obj_live_count() - live_before);
        return false;
    }

    /* Overwriting a slot must let go of the previous occupant. */
    object *a = obj_create(TYPE_BYTES, 8, 0);
    object *b = obj_create(TYPE_BYTES, 8, 0);
    object *holder = obj_create(TYPE_LIST, 0, 1);
    if (!a || !b || !holder) return false;

    obj_set_slot(holder, 0, a, CAP_READ);
    obj_set_slot(holder, 0, b, CAP_READ);
    if (obj_refs(a) != 1 || obj_refs(b) != 2) {
        kprintf("obj:  replacing a slot did not release the old target\n");
        return false;
    }

    obj_release(a);
    obj_release(b);
    obj_release(holder);

    return obj_live_count() == live_before;
}
