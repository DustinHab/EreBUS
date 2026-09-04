/*
 * isr.S -- interrupt entry stubs: 256 fixed 16-byte slots, entry n at isr_stubs + n * 16.
 * - each stub records its vector (and a dummy error code where the processor pushes none) and joins the common path
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

    /* If this interrupt arrived while a program was running, GS still
     * belongs to that program. Swap the kernel's in before anything
     * touches per-processor data. The saved code selector says where we
     * came from: its bottom two bits are the privilege level, and the
     * frame puts it 144 bytes up from here. */
    testb   $3, 144(%rsp)
    jz      1f
    swapgs
1:

    movq    %rsp, %rdi          /* first argument: the frame */
    call    trap_dispatch

    /* And back, if that is where we are returning to. */
    testb   $3, 144(%rsp)
    jz      2f
    swapgs
2:
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
