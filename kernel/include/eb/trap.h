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

/* Attaches a handler to one of the 16 legacy interrupt lines. */
void irq_install(u8 irq, irq_handler fn);

/* Total number of hardware interrupts serviced, for reporting. */
u64 trap_irq_count(void);

#endif /* EB_TRAP_H */
