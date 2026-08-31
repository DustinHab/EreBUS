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
#include <eb/kheap.h>
#include <eb/fmt.h>
#include <eb/panic.h>

#define OBJ_MAGIC 0x4F424A454354ULL   /* "OBJECT" */

struct object {
    u64     magic;
    u64     id;
    type_id type;
    u32     _pad;
    u64     refs;
    u64     size;      /* payload bytes */
    u64     nslots;    /* outgoing references */
    /* payload follows, then the slot array */
};

/* Where the two variable parts sit inside the allocation. */
static inline u8 *payload_of(object *o)
{
    return (u8 *)o + sizeof(object);
}

static inline object **slots_of(object *o)
{
    /* The slot array is pointer-aligned; the payload is rounded up so
     * it stays that way whatever length was asked for. */
    u64 aligned = (o->size + 7) & ~7ULL;
    return (object **)(payload_of(o) + aligned);
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

    u64 aligned = (payload_size + 7) & ~7ULL;
    u64 total = sizeof(object) + aligned + slot_count * sizeof(object *);

    object *o = (object *)kzalloc(total);
    if (!o) return NULL;

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
    object **slots = slots_of(o);
    for (u64 i = 0; i < o->nslots; i++)
        if (slots[i]) obj_release(slots[i]);

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
    kfree(o);
}

type_id obj_type(const object *o)  { check(o, "type"); return o->type; }
u64     obj_id(const object *o)    { check(o, "id"); return o->id; }
u64     obj_refs(const object *o)  { check(o, "refs"); return o->refs; }
u64     obj_size(const object *o)  { check(o, "size"); return o->size; }
u64     obj_slots(const object *o) { check(o, "slots"); return o->nslots; }

void *obj_data(object *o)
{
    check(o, "data");
    return o->size ? payload_of(o) : NULL;
}

bool obj_set_slot(object *o, u64 index, object *target)
{
    check(o, "set slot");
    if (index >= o->nslots) return false;
    if (target) check(target, "set slot target");

    object **slots = slots_of(o);
    object *old = slots[index];

    if (target) obj_retain(target);
    slots[index] = target;
    if (old) obj_release(old);
    return true;
}

object *obj_get_slot(object *o, u64 index)
{
    check(o, "get slot");
    if (index >= o->nslots) return NULL;
    return slots_of(o)[index];
}

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
    if (!obj_set_slot(list, 0, text)) return false;

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
    if (obj_set_slot(list, 99, list)) {
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

    obj_set_slot(holder, 0, a);
    obj_set_slot(holder, 0, b);
    if (obj_refs(a) != 1 || obj_refs(b) != 2) {
        kprintf("obj:  replacing a slot did not release the old target\n");
        return false;
    }

    obj_release(a);
    obj_release(b);
    obj_release(holder);

    return obj_live_count() == live_before;
}
