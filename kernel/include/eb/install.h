#ifndef EB_INSTALL_H
#define EB_INSTALL_H

#include <eb/types.h>

/* The offer made at start-up, on a machine that has nowhere to keep
 * anything.
 *
 * A machine booted from a stick forgets everything when it stops. The
 * words "disks", "settle" and "yes" have always been able to fix that
 * from the terminal, but they are of no use to somebody who has just
 * started the system for the first time and does not yet know they
 * exist. So the machine asks, once, while it is starting: here are the
 * disks, name one to take it for the system, or press escape and carry
 * on without a memory.
 *
 * Nothing is written until a disk is named and then the word "yes" is
 * typed out in full. The offer is skipped entirely where there is
 * already a store, where there is no disk that could be taken, and
 * where there is no keyboard to answer with -- a machine nobody is
 * sitting at boots without waiting for anyone. */
void install_offer(void);

#endif /* EB_INSTALL_H */
