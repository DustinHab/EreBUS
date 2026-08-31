#ifndef EB_SYSCALL_H
#define EB_SYSCALL_H

#include <eb/types.h>

/* The whole system call interface.
 *
 * Eight calls. There is nothing here to open a file with, no way to ask
 * for anything by name, and no call that grants authority out of thin
 * air. A program begins holding whatever capabilities it was handed and
 * can only ever use those, pass them on weakened, or let them go --
 * pass is the one call that moves authority, and it can only ever move
 * less than the caller holds. clock is the one call that answers
 * without any capability at all, because the time of day is the one
 * fact that belongs to nobody.
 *
 * The size of this list is the point. Every system call is a place
 * where untrusted input crosses into the kernel, and the surest way to
 * get that right is to have very few of them. */

#define SYS_EXIT    0   /* exit(code) */
#define SYS_YIELD   1   /* yield() */
#define SYS_SEND    2   /* send(handle, tag, word0, word1, word2) */
#define SYS_RECEIVE 3   /* receive(handle, buffer, no_wait) */
#define SYS_READ    4   /* read(handle, offset) -> eight bytes */
#define SYS_WRITE   5   /* write(handle, offset, value) */
#define SYS_PASS    6   /* pass(port, tag, capability, mask, word) */
#define SYS_CLOCK   7   /* clock() -> seconds since midnight */
#define SYS_MAX     8

/* Results. Deliberately few and deliberately vague: telling a caller
 * exactly why a capability did not work would let it map out what it
 * does not hold. */
#define SYS_OK        0
#define SYS_DENIED   ((u64)-1)   /* no such capability, or not enough rights */
#define SYS_WOULDFAIL ((u64)-2)  /* the port is full, or nothing to receive */
#define SYS_BADCALL  ((u64)-3)   /* no such call */

/* Sets up the MSRs the syscall instruction reads, and points it here. */
void syscall_init(void);

/* Per-processor data reachable through GS while in the kernel. The
 * offsets are fixed: syscall.S reads them directly. */
void percpu_init(void);
void percpu_set_kernel_stack(u64 stack_top);

#endif /* EB_SYSCALL_H */
