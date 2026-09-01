#ifndef EB_THREAD_H
#define EB_THREAD_H

#include <eb/types.h>
#include <eb/cap.h>

/* Threads and the scheduler.
 *
 * Round robin, preemptive, one processor. A thread runs until it
 * blocks, yields, or its slice runs out; then the next one in line
 * runs. No priorities yet: a scheduler that cannot be explained in a
 * sentence is a scheduler whose behaviour under load nobody can
 * predict, and there is nothing here yet to tune against.
 *
 * Every thread belongs to a domain, and that is where its authority
 * comes from. Two threads in the same domain see the same capabilities;
 * two in different domains cannot reach each other's objects at all,
 * whatever they do with handle values.
 */

typedef struct thread thread;
typedef void (*thread_entry)(void *arg);

/* Adopts the currently running context as the first thread, so there is
 * never a moment with no thread at all. */
void sched_init(domain *boot_domain);

thread *thread_create(const char *name, thread_entry entry, void *arg,
                      domain *d);

/* Ends the calling thread. Does not return. */
void thread_exit(void);

/* Hands the processor on voluntarily. */
void sched_yield(void);

/* Takes the calling thread off the run queue until somebody wakes it.
 * Interrupts must already be off; the caller has put the thread on
 * whatever wait list will wake it, and losing the race between those
 * two steps would lose the wakeup. */
void sched_block(void);

/* Gives a thread its own address space. Zero means the kernel's. */
void thread_set_pml4(thread *t, phys_addr pml4);

/* Runs fn(arg) after the thread has finished and left its stack, from
 * whoever reaps it. Where a process hangs its teardown. */
void thread_on_reap(thread *t, void (*fn)(void *), void *arg);

/* Time actually held, booked at every handover of the processor. The
 * boot thread's time is the machine's idle time: it does nothing but
 * halt, so whatever it was holding, nobody else wanted. */
u64  thread_ran_ns(const thread *t);
u64  sched_idle_ns(void);

/* How long a thread may hold the processor before the timer takes it,
 * in ticks. The settings speak in milliseconds; this speaks in what
 * the hardware counts. */
void sched_set_slice_ticks(u32 t);
void sched_wake(thread *t);

/* Marks a thread to end at its next step into the kernel; a blocked
 * one is woken so that step comes. The ending runs in the thread's
 * own context, through the ordinary exit and reaping. */
void thread_condemn(thread *t);
bool thread_condemned(const thread *t);

/* Called from the timer interrupt. Marks the slice as spent; the switch
 * itself happens on the way out of the handler. */
void sched_tick(void);

/* Performs a pending switch, if one is due. Called at the end of
 * interrupt handling, where it is safe to change stacks. */
void sched_preempt_if_due(void);

thread     *sched_current(void);
domain     *thread_domain(const thread *t);
const char *thread_name(const thread *t);
u64         thread_id(const thread *t);

u64 sched_switches(void);
u64 sched_threads(void);
u64 sched_runnable(void);

bool sched_selftest(void);

#endif /* EB_THREAD_H */
