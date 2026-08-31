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

/* Stack the hardware switches to when entering ring 0 from ring 3.
 * Only becomes relevant once there are processes. */
void tss_set_kernel_stack(u64 rsp0);

#endif /* EB_GDT_H */
