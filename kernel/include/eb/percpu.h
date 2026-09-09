#ifndef EB_PERCPU_H
#define EB_PERCPU_H

#include <eb/types.h>

/* Per-processor block, reached through this processor's gs base.
 *
 * The first two fields are fixed: syscall.S reads gs:0 (kernel_rsp) and
 * gs:8 (user_rsp) by hand, and self at gs:16 is how C code finds the
 * block's own address without a system-register read. Everything after
 * that is the scheduler's, one running thread and one idle thread per
 * processor, so that a switch on one core never touches another's. */
typedef struct percpu {
    u64            kernel_rsp;    /* 0  -- where a system call lands */
    u64            user_rsp;      /* 8  -- where it came from */
    struct percpu *self;          /* 16 -- this block's own address */
    u32            index;         /* 24 -- 0 is the boot processor */
    u32            apic_id;       /* 28 */
    struct thread *current;       /* 32 -- the thread this core runs */
    struct thread *idle;          /* 40 -- runs when nothing else is ready */
    u32            slice_left;     /* 48 -- ticks until preemption */
    u32            resched;       /* 52 -- this core owes itself a reschedule */
    u64            switch_stamp;  /* 56 -- when the running thread took the core */
} percpu;

/* This processor's block. Valid only after percpu_init has run here. */
static inline percpu *this_cpu(void)
{
    percpu *p;
    __asm__ volatile ("movq %%gs:16, %0" : "=r"(p));
    return p;
}

/* This processor's index: 0 is the boot processor. */
static inline u32 this_cpu_id(void) { return this_cpu()->index; }

/* Points this processor's gs base at its own block and records who it is.
 * Called once per processor, before it schedules or takes a system call. */
void percpu_init(u32 index, u32 apic_id);

/* Where an interrupt from ring 3 should land on this processor: the
 * kernel stack of the thread it is about to run. */
void percpu_set_kernel_stack(u64 stack_top);

#endif /* EB_PERCPU_H */
