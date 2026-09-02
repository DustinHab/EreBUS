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
 * The counts stay -- they free almost everything, immediately and
 * predictably -- and obj_collect() below walks the graph now and then
 * to catch the rest. Between runs a cycle costs memory; it never costs
 * authority, which is the property that actually matters here.
 */
#include <eb/object.h>
#include <eb/cap.h>
#include <eb/msg.h>
#include <eb/io.h>
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
    u32     fleeting;  /* changes by design; its edits are not history */
    u32     transient; /* lives until the next boot; the snapshot leaves it out */
    char    name[OBJ_NAME_MAX];   /* what the object calls itself */
    obj_slot *slots;              /* its own allocation, so it can grow */

    /* Every object that exists, on one list.
     *
     * Nothing may use this to find anything: it is not reachable from
     * outside this file and there is no call that turns it into a
     * lookup. It exists for one purpose, which is that a collector has
     * to be able to sweep what the roots did not reach, and there is no
     * way to sweep a set one cannot enumerate. The security argument is
     * unchanged -- an enumeration only the allocator can see is not a
     * namespace. */
    object *all_prev, *all_next;
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

static object *all_objects;

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
    type_names[TYPE_PROGRAM] = "program";
    type_names[TYPE_PICTURE] = "picture";
    types_registered = TYPE_BUILTIN_COUNT;

    all_objects = NULL;
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

    /* The list link, the counts, and every reference count change below
     * happen with interrupts off. Threads mutate the graph and the
     * timer switches between them mid-operation; a release preempted
     * between the decrement and the unlink would leave a half-dead
     * object on the list for anything else -- another release, the
     * collector -- to trip over. */
    u64 flags = irq_save();
    o->all_next = all_objects;
    if (all_objects) all_objects->all_prev = o;
    all_objects = o;

    live_objects++;
    created_objects++;
    irq_restore(flags);
    return o;
}

void obj_retain(object *o)
{
    check(o, "retain");
    u64 flags = irq_save();
    o->refs++;
    irq_restore(flags);
}

void obj_release(object *o)
{
    check(o, "release");

    if (o->refs == 0)
        panic("object %llu released more often than it was held", o->id);

    u64 flags = irq_save();
    if (--o->refs > 0) { irq_restore(flags); return; }

    /* A port dying with messages still queued is holding their cargo,
     * and those holds live in the payload where the generic teardown
     * below cannot see them. The type comparison is guarded because
     * before the port type is registered, port_type() answers zero --
     * which is TYPE_NULL, and matching that would hand arbitrary
     * payloads to the queue walker. */
    if (o->type >= TYPE_BUILTIN_COUNT && o->type == port_type())
        port_drop_queued(o);

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

    if (o->all_prev) o->all_prev->all_next = o->all_next;
    else             all_objects = o->all_next;
    if (o->all_next) o->all_next->all_prev = o->all_prev;

    live_objects--;
    if (o->slots) kfree(o->slots);
    kfree(o);
    irq_restore(flags);
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

    u64 flags = irq_save();           /* swap and counts in one piece */
    obj_slot *slots = slots_of(o);
    object *old = slots[index].target;

    if (target) obj_retain(target);
    slots[index].target = target;
    slots[index].rights = target ? rights : 0;
    if (old) obj_release(old);
    irq_restore(flags);
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

    u64 flags = irq_save();           /* the array swap must be whole */
    for (u64 i = 0; i < o->nslots; i++) bigger[i] = o->slots[i];
    obj_slot *old = o->slots;
    o->slots = bigger;
    o->nslots = count;
    irq_restore(flags);

    if (old) kfree(old);
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
/* Collecting what counting cannot                                     */
/* ------------------------------------------------------------------ */

/* Reference counting frees an object the moment nobody holds it, which
 * is most of the work and all of the predictability. What it cannot do
 * is a cycle: two objects pointing at each other hold each other up
 * after everything else has let go, and no count will ever reach zero.
 *
 * So the counts stay, and this runs occasionally to catch what they
 * miss. It is the same walk persistence already does -- from the roots,
 * following every reference, marking what it reaches -- with the
 * addition that whatever the walk did not reach is not merely absent
 * from the snapshot but is genuinely gone, and is freed.
 *
 * The roots are not a list anybody keeps. They are computed, from the
 * one invariant this file has always enforced: every reference is
 * counted. Some of an object's count comes from inside the graph --
 * slots in other objects, messages waiting in ports -- and that part
 * can be recomputed by looking. Whatever is left over is a holder the
 * graph cannot see: a capability table, a kernel pointer, a message
 * being composed. Those holders can still reach the object, so it is a
 * root, and so is everything a root can reach. An object whose entire
 * count is accounted for from inside is held by nothing but the graph
 * itself -- and if no root reaches it either, the things holding it up
 * are exactly as dead as it is. */

#define COLLECT_REACHED 0x80000000u
#define COLLECT_COUNT   0x7FFFFFFFu

static object **grey;         /* what has been marked but not walked */
static u64      grey_count;

/* Guarded the same way as in obj_release, and for the same reason:
 * before the port type is registered, port_type() answers TYPE_NULL. */
static bool is_port(const object *o)
{
    return o->type >= TYPE_BUILTIN_COUNT && o->type == port_type();
}

static void count_inward(object *o)
{
    if (o) o->mark++;         /* counts stay far below the flag bit */
}

static void grey_push(object *o)
{
    if (!o || (o->mark & COLLECT_REACHED)) return;
    o->mark |= COLLECT_REACHED;
    grey[grey_count++] = o;      /* bounded by live_objects, pushed once */
}

static void trace_from_roots(void)
{
    /* Roots first: more count than the graph explains means an outside
     * holder. The flag bit is masked off because earlier roots have
     * already been stamped by the time later ones are tested. */
    for (object *o = all_objects; o; o = o->all_next)
        if (o->refs > (o->mark & COLLECT_COUNT)) grey_push(o);

    while (grey_count > 0) {
        object *o = grey[--grey_count];

        obj_slot *slots = slots_of(o);
        for (u64 i = 0; i < o->nslots; i++) grey_push(slots[i].target);

        /* A port's queue holds objects that are between two tables and
         * so belong to no table at all. */
        if (is_port(o)) port_visit_queued(o, grey_push);
    }
}

u64 obj_collect(void)
{
    /* An explicit worklist rather than recursion. A graph deep enough
     * to be worth collecting is a graph deep enough to run the kernel
     * stack into its guard page, and a collector that crashes on a big
     * heap is worse than no collector. Every object is pushed at most
     * once, because it is stamped before it is pushed, so live_objects
     * entries is not an estimate -- it is the exact bound. */
    /* Nothing may move underneath the walk. The graph is mutated from
     * kernel threads that the timer can switch between, and a count
     * taken before a mutation paired with a walk taken after it would
     * free live memory. The heap here is small enough that holding the
     * machine for one walk is cheaper than being clever about it.
     *
     * The worklist is sized inside the same stillness, for the same
     * reason: a count taken before the world stops is a count something
     * may have outgrown by the time it matters. */
    u64 flags = irq_save();

    u64 room = live_objects;
    if (room == 0) { irq_restore(flags); return 0; }

    grey = (object **)kzalloc(room * sizeof(object *));
    if (!grey) { irq_restore(flags); return 0; }
    grey_count = 0;

    /* How much of each count the graph itself explains. */
    for (object *o = all_objects; o; o = o->all_next) o->mark = 0;
    for (object *o = all_objects; o; o = o->all_next) {
        obj_slot *slots = slots_of(o);
        for (u64 i = 0; i < o->nslots; i++) count_inward(slots[i].target);
        if (is_port(o)) port_visit_queued(o, count_inward);
    }

    trace_from_roots();

    /* Hold on to the unreachable ones first.
     *
     * Everything about to be freed is pointing at other things about to
     * be freed, and letting go in the wrong order would free an object
     * while the next step still has to read its links. Holding all of
     * them for the length of the sweep makes the order stop mattering. */
    u64 doomed = 0;
    for (object *o = all_objects; o; o = o->all_next)
        if (!(o->mark & COLLECT_REACHED)) { o->refs++; doomed++; }

    if (doomed == 0) {
        for (object *o = all_objects; o; o = o->all_next) o->mark = 0;
        irq_restore(flags);
        kfree(grey);
        grey = NULL;
        return 0;
    }

    /* Break the cycles. After this every unreachable object is held by
     * exactly the reference taken above: not by each other, because
     * those are now cleared, and not by anything reachable, because
     * anything reachable would have marked it. A dead port's undelivered
     * messages count as references it was holding, so they are let go of
     * here with everything else. */
    for (object *o = all_objects; o; o = o->all_next) {
        if (o->mark & COLLECT_REACHED) continue;
        obj_slot *slots = slots_of(o);
        for (u64 i = 0; i < o->nslots; i++) {
            if (!slots[i].target) continue;
            obj_release(slots[i].target);
            slots[i].target = NULL;
            slots[i].rights = 0;
        }
        if (is_port(o)) port_drop_queued(o);
    }

    /* And let go. Each release is now the last one, and frees exactly
     * the object it was called on, which is what makes walking the list
     * while it shortens safe. */
    object *o = all_objects;
    while (o) {
        object *next = o->all_next;
        if (!(o->mark & COLLECT_REACHED)) obj_release(o);
        o = next;
    }

    /* The mark field goes back to meaning nothing, which is what the
     * snapshot walk expects to find. */
    for (object *s = all_objects; s; s = s->all_next) s->mark = 0;

    irq_restore(flags);
    kfree(grey);
    grey = NULL;
    return doomed;
}

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

bool obj_collect_selftest(void)
{
    u64 before = obj_live_count();

    /* Two objects pointing at each other and held by nothing else. The
     * counts alone cannot free them, and the test first proves that the
     * problem is real: after both releases they are still here. */
    object *a = obj_create(TYPE_LIST, 0, 1);
    object *b = obj_create(TYPE_LIST, 0, 1);
    if (!a || !b) return false;
    obj_set_slot(a, 0, b, CAP_READ);
    obj_set_slot(b, 0, a, CAP_READ);
    obj_release(a);
    obj_release(b);

    if (obj_live_count() != before + 2) {
        kprintf("obj:  the cycle did not leak, which this test relies on\n");
        return false;
    }

    /* A second cycle, this one still held: our reference to c is the
     * kind of hold the graph cannot see, and it must be enough. */
    object *c = obj_create(TYPE_LIST, 0, 1);
    object *d = obj_create(TYPE_LIST, 0, 1);
    if (!c || !d) return false;
    obj_set_slot(c, 0, d, CAP_READ);
    obj_set_slot(d, 0, c, CAP_READ);
    obj_release(d);                       /* now held only through c */

    /* Exactly two: the loose cycle and not one object more. Everything
     * else alive right now -- ports, sessions, the graph -- is held by
     * somebody, and a collector that cannot tell would show up here as
     * a count greater than two. */
    u64 swept = obj_collect();
    if (swept != 2) {
        kprintf("obj:  collector swept %llu, the loose cycle was 2\n", swept);
        return false;
    }
    if (obj_live_count() != before + 2) return false;   /* c and d remain */

    /* Let go of the held cycle and it is next. */
    obj_release(c);
    swept = obj_collect();
    if (swept != 2) {
        kprintf("obj:  released cycle not swept, got %llu\n", swept);
        return false;
    }

    return obj_live_count() == before;
}

/* ------------------------------------------------------------------ */

static u64 touches;

void obj_touch(object *o)
{
    if (o && o->fleeting) return;
    touches++;
}

u64 obj_touches(void) { return touches; }

void obj_set_fleeting(object *o, bool fleeting)
{
    check(o, "set fleeting");
    o->fleeting = fleeting ? 1 : 0;
}

bool obj_is_fleeting(const object *o)
{
    return o && o->magic == OBJ_MAGIC && o->fleeting;
}

void obj_set_transient(object *o, bool transient)
{
    check(o, "set transient");
    o->transient = transient ? 1 : 0;
    if (transient) o->fleeting = 1;   /* and its edits are nobody's history */
}

bool obj_is_transient(const object *o)
{
    return o && o->magic == OBJ_MAGIC && o->transient;
}
