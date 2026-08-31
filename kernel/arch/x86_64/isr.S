/*
 * isr.S -- interrupt entry stubs.
 *
 * The processor gives the handler almost nothing: a few registers on
 * the stack and, for some exceptions, an error code. It does not say
 * which vector fired. So every vector needs its own entry point that
 * records the number and then joins a common path.
 *
 * Rather than writing 256 labels, the stubs are generated as a table of
 * fixed 16-byte slots. Entry n therefore lives at isr_stubs + n * 16,
 * which the C side can compute without any symbol lookup.
 */

    .section .text

    .align 16
    .globl isr_stubs
isr_stubs:
    .set vec, 0
    .rept 256
    .align 16
    /* These ten vectors get an error code pushed by the processor. For
     * all others we push a zero so the stack layout is always the
     * same -- the crash report should not need a special case. */
    .if (vec == 8) || (vec == 10) || (vec == 11) || (vec == 12) || \
        (vec == 13) || (vec == 14) || (vec == 17) || (vec == 21) || \
        (vec == 29) || (vec == 30)
    .else
    pushq   $0
    .endif
    pushq   $vec
    jmp     isr_common
    .set vec, vec + 1
    .endr

    .align 16
    .type isr_common, @function
isr_common:
    /* Push order defines the trap_frame layout. Do not reorder without
     * changing eb/trap.h to match. */
    pushq   %rax
    pushq   %rbx
    pushq   %rcx
    pushq   %rdx
    pushq   %rsi
    pushq   %rdi
    pushq   %rbp
    pushq   %r8
    pushq   %r9
    pushq   %r10
    pushq   %r11
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15

    cld                         /* the ABI wants the direction flag
                                   clear; an interrupt can arrive while
                                   it is set */

    /* Once there is user mode, SWAPGS goes here, guarded by a check of
     * the saved CS: coming from ring 3 the GS base must be exchanged
     * for the kernel's before any per-CPU data is touched. */

    movq    %rsp, %rdi          /* first argument: the frame */
    call    trap_dispatch

    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %r11
    popq    %r10
    popq    %r9
    popq    %r8
    popq    %rbp
    popq    %rdi
    popq    %rsi
    popq    %rdx
    popq    %rcx
    popq    %rbx
    popq    %rax

    addq    $16, %rsp           /* drop vector and error code */
    iretq
    .size isr_common, . - isr_common

    .section .note.GNU-stack, "", @progbits
