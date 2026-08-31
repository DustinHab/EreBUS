#ifndef EB_CAP_H
#define EB_CAP_H

#include <eb/types.h>
#include <eb/object.h>

/* Capabilities and protection domains.
 *
 * A domain is whatever holds authority: for now the kernel itself and
 * whatever it sets up, later every process. A domain owns a table of
 * capability slots, and that table is the complete and exclusive list
 * of everything the domain can reach. Not a cache of permissions, not
 * a starting point for lookups -- the whole extent of its world.
 *
 * A handle is an index into the holder's own table, so the same number
 * means different things in different domains, and usually nothing at
 * all. There is no address in it, nothing derived from the object, and
 * nothing that carries meaning outside the table it came from. A
 * program that invents handle values gets its own slots or an error,
 * never somebody else's object.
 *
 * The generation counter is what makes a handle stop working for good.
 * Slots are reused, and without it a stale handle would quietly start
 * referring to whatever moved in afterwards -- the same bug as a
 * dangling pointer, with authority attached.
 *
 * Rights only ever narrow. Passing a capability on takes a mask, and
 * the result is the intersection with what the sender already had.
 * There is no call anywhere that adds a right to an existing
 * capability, which is why authority in this system can be traced: it
 * has to have come from somewhere that already had it.
 */

typedef u64 cap_handle;
#define CAP_INVALID ((cap_handle)0)

#define CAP_READ    (1u << 0)   /* may read the payload            */
#define CAP_WRITE   (1u << 1)   /* may change the payload          */
#define CAP_GRANT   (1u << 2)   /* may pass this on to a third party */
#define CAP_CALL    (1u << 3)   /* may send messages to it         */
#define CAP_DESTROY (1u << 4)   /* may ask for it to be destroyed  */
#define CAP_ALL     (CAP_READ | CAP_WRITE | CAP_GRANT | CAP_CALL | CAP_DESTROY)

typedef struct domain domain;

domain *domain_create(const char *label, u64 slot_count);
void    domain_destroy(domain *d);
const char *domain_label(const domain *d);
u64     domain_used(const domain *d);
u64     domain_capacity(const domain *d);

/* Puts an object into a domain's table. Takes a reference. */
cap_handle cap_insert(domain *d, object *o, u32 rights);

/* Resolves a handle within one domain. Returns NULL if the handle does
 * not belong to this domain, has been revoked, is stale, or does not
 * carry every right in `needed`. */
object *cap_lookup(domain *d, cap_handle h, u32 needed);

/* What rights a handle carries, or zero if it does not resolve. */
u32 cap_rights(domain *d, cap_handle h);

/* Passes a capability from one domain to another. The result carries
 * `rights & mask & what the sender held` -- never more. Requires the
 * sender to hold CAP_GRANT on it. */
cap_handle cap_delegate(domain *from, cap_handle h, domain *to, u32 mask);

/* Withdraws one capability. The handle stops resolving immediately and
 * never resolves again, even after the slot is reused. */
bool cap_revoke(domain *d, cap_handle h);

bool cap_selftest(void);

#endif /* EB_CAP_H */
