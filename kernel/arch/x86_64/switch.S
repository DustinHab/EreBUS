/*
 * switch.S -- the context switch.
 *
 * Only the callee-saved registers are touched. Everything else the ABI
 * already treats as destroyed across a call, so whoever called us has
 * either saved it or does not care -- and a switch is, from the running
 * code's point of view, a function call that happens to take a very
 * long time.
 *
 * The flags go too, because they carry the interrupt enable bit. A
 * thread suspended inside an interrupt handler was running with
 * interrupts off and has to come back that way; one that yielded
 * voluntarily comes back with them on. Restoring the wrong one would
 * either drop interrupts for good or re-enable them halfway through a
 * handler.
 *
 * switch_stack(u64 *save_rsp, u64 load_rsp)
 *   rdi: where to write the outgoing thread's stack pointer
 *   rsi: the incoming thread's stack pointer
 */

    .section .text
    .globl switch_stack
    .type switch_stack, @function
switch_stack:
    pushq   %rbp
    pushq   %rbx
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15
    pushfq

    movq    %rsp, (%rdi)        /* remember where we left off */
    movq    %rsi, %rsp          /* and stand on the other stack */

    popfq
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbx
    popq    %rbp
    ret                         /* returns into the other thread */
    .size switch_stack, . - switch_stack

    .section .note.GNU-stack, "", @progbits
