#ifndef EB_SMP_H
#define EB_SMP_H

#include <eb/types.h>

/* Starts the application processors listed in the ACPI tables. Each one
 * comes up through a real-mode trampoline into long mode, enables its
 * local apic, and parks (idle) -- the scheduler still runs on the boot
 * processor alone until it is taught to spread. Pass the rsdp the loader
 * found. Returns how many processors are up, counting the bsp. */
u32 smp_start(u64 acpi_rsdp);

/* Claims the fixed physical page the trampoline runs from, so the frame
 * allocator never hands it to anything else. Call once, right after
 * pmm_init and before any other allocation. */
void smp_reserve_trampoline(void);

/* Processors up, including the boot processor. */
u32 smp_cpu_count(void);

/* Lets the application processors into the scheduler. The boot processor
 * calls this once the single-threaded part of start-up is done, after
 * which kernel threads and user processes run on every processor. */
void smp_release(void);

/* Proves the application processors run kernel threads in parallel: it
 * releases them into the scheduler, runs a batch of worker threads, and
 * checks that more than one processor took part, then parks the
 * application processors again. Returns true when parallel execution was
 * confirmed (or when there is only one processor, so nothing to prove).
 * The rest of start-up and every user process still run on the boot
 * processor alone until the object and capability layers are locked. */
bool smp_selftest(void);

#endif /* EB_SMP_H */
