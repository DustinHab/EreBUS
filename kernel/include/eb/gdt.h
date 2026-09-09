#ifndef EB_GDT_H
#define EB_GDT_H

#include <eb/types.h>

/* Segment selectors. The order is not free: SYSRET later expects user
 * data immediately before user code, otherwise there is no clean way
 * back out of a system call. */
#define SEL_KERNEL_CODE 0x08
#define SEL_KERNEL_DATA 0x10
#define SEL_USER_DATA   0x18
#define SEL_USER_CODE   0x20
#define SEL_TSS         0x28

/* Emergency stacks in the TSS, numbered from 1 by the hardware.
 *
 * Why this matters: if the kernel stack overflows, touching it raises a
 * page fault. Handling that fault needs stack -- which is exactly what
 * is missing. That gives a double fault, whose handler fails the same
 * way, and therefore a triple fault: the machine reboots without a
 * word. With a dedicated stack per critical exception the handler runs
 * on solid ground and can report what happened. */
#define IST_DOUBLE_FAULT 1
#define IST_NMI          2
#define IST_DEBUG        3

void gdt_init(void);

/* An application processor loading the shared table and kernel
 * selectors. It does not load a task register here: it does that in
 * gdt_setup_ap once it has a task segment of its own. */
void gdt_load_ap(void);

/* An application processor claiming its own task segment: fills the
 * descriptor for this processor in the shared table and loads the task
 * register with it. Each processor needs its own, because the task
 * segment holds the ring-0 stack a ring-3 interrupt lands on, and two
 * processors sharing one would land on the same stack at once. */
void gdt_setup_ap(u32 cpu);

/* Stack the hardware switches to when entering ring 0 from ring 3, for
 * the processor this runs on. Only relevant once there are processes. */
void tss_set_kernel_stack(u64 rsp0);

#endif /* EB_GDT_H */
