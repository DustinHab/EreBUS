/*
 * switch.S -- context switch: callee-saved registers and the flags (interrupt enable bit) only.
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
