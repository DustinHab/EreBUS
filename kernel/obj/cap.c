/*
 * cap.c -- capability tables and protection domains.
 */
#include <eb/cap.h>
#include <eb/kheap.h>
#include <eb/fmt.h>
#include <eb/panic.h>

#define DOMAIN_MAGIC 0x444F4D41494EULL   /* "DOMAIN" */

typedef struct {
    object *target;      /* NULL when the slot is empty */
    u32     rights;
    u32     generation;  /* bumped every time the slot is reused */
} cap_slot;

struct domain {
    u64         magic;
    const char *label;
    u64         capacity;
    u64         used;
    cap_slot   *slots;
};

/* A handle carries the slot index in the low half and the generation in
 * the high half. Index zero is never handed out, so a handle of zero is
 * always invalid and needs no special case. */
static inline cap_handle make_handle(u64 index, u32 generation)
{
    return ((cap_handle)generation << 32) | (index & 0xFFFFFFFFULL);
}

static inline u64 handle_index(cap_handle h) { return h & 0xFFFFFFFFULL; }
static inline u32 handle_gen(cap_handle h)   { return (u32)(h >> 32); }

static void check(const domain *d, const char *where)
{
    if (!d || d->magic != DOMAIN_MAGIC)
        panic("not a domain at %p, found during %s", (const void *)d, where);
}

/* ------------------------------------------------------------------ */

domain *domain_create(const char *label, u64 slot_count)
{
    if (slot_count < 2) slot_count = 2;

    domain *d = (domain *)kzalloc(sizeof(domain));
    if (!d) return NULL;

    d->slots = (cap_slot *)kzalloc(slot_count * sizeof(cap_slot));
    if (!d->slots) { kfree(d); return NULL; }

    d->magic    = DOMAIN_MAGIC;
    d->label    = label;
    d->capacity = slot_count;
    d->used     = 0;
    return d;
}

void domain_destroy(domain *d)
{
    check(d, "destroy");

    /* Letting go of a domain lets go of everything it could reach. That
     * is the whole teardown: there is no other list to walk, because
     * there was no other way in. */
    for (u64 i = 1; i < d->capacity; i++)
        if (d->slots[i].target) obj_release(d->slots[i].target);

    d->magic = 0;
    kfree(d->slots);
    kfree(d);
}

const char *domain_label(const domain *d) { check(d, "label"); return d->label; }
u64 domain_used(const domain *d)     { check(d, "used"); return d->used; }
u64 domain_capacity(const domain *d) { check(d, "capacity"); return d->capacity - 1; }

/* ------------------------------------------------------------------ */

cap_handle cap_insert(domain *d, object *o, u32 rights)
{
    check(d, "insert");
    if (!o) return CAP_INVALID;

    rights &= CAP_ALL;
    if (rights == 0) return CAP_INVALID;   /* a capability to do nothing */

    for (u64 i = 1; i < d->capacity; i++) {
        if (d->slots[i].target) continue;

        /* Bump before use, so a handle from the slot's previous life
         * can never match its next one. */
        d->slots[i].generation++;
        d->slots[i].target = o;
        d->slots[i].rights = rights;
        d->used++;

        obj_retain(o);
        return make_handle(i, d->slots[i].generation);
    }
    return CAP_INVALID;
}

/* The single point where a handle turns back into an object. Every path
 * into an object goes through here, which is what makes the rules
 * enforceable at all. */
static cap_slot *resolve(domain *d, cap_handle h)
{
    u64 index = handle_index(h);
    if (index == 0 || index >= d->capacity) return NULL;

    cap_slot *s = &d->slots[index];
    if (!s->target) return NULL;
    if (s->generation != handle_gen(h)) return NULL;
    return s;
}

object *cap_lookup(domain *d, cap_handle h, u32 needed)
{
    check(d, "lookup");
    cap_slot *s = resolve(d, h);
    if (!s) return NULL;
    if ((s->rights & needed) != needed) return NULL;
    return s->target;
}

u32 cap_rights(domain *d, cap_handle h)
{
    check(d, "rights");
    cap_slot *s = resolve(d, h);
    return s ? s->rights : 0;
}

cap_handle cap_delegate(domain *from, cap_handle h, domain *to, u32 mask)
{
    check(from, "delegate");
    check(to, "delegate");

    cap_slot *s = resolve(from, h);
    if (!s) return CAP_INVALID;

    /* Passing on is itself a right. A capability can be usable without
     * being shareable. */
    if (!(s->rights & CAP_GRANT)) return CAP_INVALID;

    /* The intersection, and nothing else. This one line is why
     * authority in this system can only ever shrink as it travels. */
    u32 rights = s->rights & mask;
    if (rights == 0) return CAP_INVALID;

    return cap_insert(to, s->target, rights);
}

bool cap_revoke(domain *d, cap_handle h)
{
    check(d, "revoke");

    cap_slot *s = resolve(d, h);
    if (!s) return false;

    object *target = s->target;
    s->target = NULL;
    s->rights = 0;
    /* The generation is not touched here: it is bumped on the next
     * insert. Either way this handle can never match again. */
    d->used--;

    obj_release(target);
    return true;
}

/* ------------------------------------------------------------------ */
/* The properties, checked                                             */
/* ------------------------------------------------------------------ */

bool cap_selftest(void)
{
    u64 live_before = obj_live_count();

    domain *alice = domain_create("alice", 16);
    domain *bob   = domain_create("bob", 16);
    if (!alice || !bob) return false;

    object *secret = obj_create(TYPE_TEXT, 16, 0);
    if (!secret) return false;

    cap_handle a_secret = cap_insert(alice, secret, CAP_ALL);
    obj_release(secret);              /* alice's capability holds it now */

    if (a_secret == CAP_INVALID) return false;
    if (cap_lookup(alice, a_secret, CAP_READ) != secret) {
        kprintf("cap:  a domain could not use its own capability\n");
        return false;
    }

    /* 1. Isolation. Bob holds nothing, so no handle value works for
     *    him -- including the exact one that works for alice, and
     *    including every other value in the table's range. */
    if (cap_lookup(bob, a_secret, CAP_READ)) {
        kprintf("cap:  a handle resolved in a domain that was not given it\n");
        return false;
    }
    for (u64 guess = 0; guess < 64; guess++) {
        if (cap_lookup(bob, make_handle(guess, 1), CAP_READ) ||
            cap_lookup(bob, make_handle(guess, 0), CAP_READ)) {
            kprintf("cap:  a guessed handle resolved in an empty domain\n");
            return false;
        }
    }

    /* 2. Attenuation. Alice passes it on read-only. Bob can read and
     *    cannot write, however he asks. */
    cap_handle b_secret = cap_delegate(alice, a_secret, bob, CAP_READ);
    if (b_secret == CAP_INVALID) {
        kprintf("cap:  delegation refused where it should have worked\n");
        return false;
    }
    if (cap_lookup(bob, b_secret, CAP_READ) != secret) {
        kprintf("cap:  a delegated capability does not resolve\n");
        return false;
    }
    if (cap_lookup(bob, b_secret, CAP_WRITE)) {
        kprintf("cap:  a read-only capability granted write access\n");
        return false;
    }
    if (cap_rights(bob, b_secret) != CAP_READ) {
        kprintf("cap:  delegation produced rights 0x%x, expected 0x%x\n",
                cap_rights(bob, b_secret), CAP_READ);
        return false;
    }

    /* 3. Rights cannot grow. Bob asks for everything when passing it
     *    further on; he can only give away what he has, which is read.
     *    And he cannot pass it on at all, because CAP_GRANT was not in
     *    what he received. */
    domain *carol = domain_create("carol", 16);
    if (!carol) return false;

    if (cap_delegate(bob, b_secret, carol, CAP_ALL) != CAP_INVALID) {
        kprintf("cap:  a capability without grant rights was passed on\n");
        return false;
    }

    /* Alice may pass it on with grant, and then bob's copy is limited
     * to read plus grant no matter what mask he uses. */
    cap_handle b_shareable = cap_delegate(alice, a_secret, bob,
                                          CAP_READ | CAP_GRANT);
    cap_handle c_secret = cap_delegate(bob, b_shareable, carol, CAP_ALL);
    if (c_secret == CAP_INVALID) return false;
    if (cap_rights(carol, c_secret) != (CAP_READ | CAP_GRANT)) {
        kprintf("cap:  rights grew in transit, 0x%x came out\n",
                cap_rights(carol, c_secret));
        return false;
    }
    if (cap_lookup(carol, c_secret, CAP_WRITE)) {
        kprintf("cap:  write appeared two hops from a read-only grant\n");
        return false;
    }

    /* 4. Revocation is immediate and final. */
    if (!cap_revoke(bob, b_secret)) return false;
    if (cap_lookup(bob, b_secret, CAP_READ)) {
        kprintf("cap:  a revoked handle still resolves\n");
        return false;
    }
    /* Alice is unaffected: revoking a copy is not revoking the thing. */
    if (cap_lookup(alice, a_secret, CAP_READ) != secret) {
        kprintf("cap:  revoking one copy disturbed another domain\n");
        return false;
    }

    /* 5. Generations. The freed slot gets reused; the old handle must
     *    not follow the new occupant into it. */
    object *other = obj_create(TYPE_BYTES, 8, 0);
    if (!other) return false;
    cap_handle reused = cap_insert(bob, other, CAP_READ);
    obj_release(other);

    if (handle_index(reused) != handle_index(b_secret)) {
        /* Not the same slot this time, so the check below proves
         * nothing. Say so rather than claim a pass. */
        kprintf("cap:  slot was not reused, generation check inconclusive\n");
    } else if (cap_lookup(bob, b_secret, CAP_READ)) {
        kprintf("cap:  a stale handle followed the slot to its new owner\n");
        return false;
    }

    /* 6. Tearing a domain down releases everything it held, and only
     *    what it held. */
    domain_destroy(carol);
    domain_destroy(bob);
    if (cap_lookup(alice, a_secret, CAP_READ) != secret) {
        kprintf("cap:  destroying a domain took another one's object\n");
        return false;
    }
    domain_destroy(alice);

    if (obj_live_count() != live_before) {
        kprintf("cap:  %llu objects outlived their last capability\n",
                obj_live_count() - live_before);
        return false;
    }
    return true;
}
