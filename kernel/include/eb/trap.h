#ifndef EB_TRAP_H
#define EB_TRAP_H

#include <eb/types.h>

/* The register set as the entry stubs leave it on the stack.
 *
 * The field order is not a style choice: it mirrors the push order in
 * isr.S exactly, lowest address first. Change one without the other and
 * every register in a crash report is wrong. */
typedef struct {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 vector, error;          /* pushed by the stub */
    u64 rip, cs, rflags, rsp, ss;  /* pushed by the processor */
} trap_frame;

typedef void (*irq_handler)(trap_frame *f);

/* Builds and loads the interrupt descriptor table. */
void trap_init(void);

/* Attaches a handler to one of the 16 legacy interrupt lines. A line
 * may carry several devices (pci pins are shared); each handler looks
 * at its own device and does nothing when it was not the one. */
void irq_install(u8 irq, irq_handler fn);

/* A vector of the processor's own, for a message-signalled interrupt:
 * the handler is attached and the vector number returned, or -1 when
 * there is no local controller or no vector left. */
i32  irq_alloc_vector(irq_handler fn);

/* Total number of hardware interrupts serviced, for reporting; and of
 * those, the message-signalled ones. */
u64 trap_irq_count(void);
u64 trap_msi_count(void);

#endif /* EB_TRAP_H */
