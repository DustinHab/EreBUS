/*
 * start.S -- first kernel code after the loader: RDI = eb_boot_info (SysV), switch to the kernel's own stack.
 */

    .section .text.boot, "ax"
    .global _start
    .type _start, @function
_start:
    cli                         /* interrupts off until the IDT is up */
    cld                         /* define the direction flag: the ABI
                                   wants it clear, and some firmware
                                   leaves it set */

    leaq    stack_top(%rip), %rsp
    andq    $-16, %rsp          /* 16-byte alignment for the ABI */
    xorq    %rbp, %rbp          /* mark the end of the call chain */

    pushq   $0                  /* fake return address so a stack walk
                                   terminates cleanly */
    pushq   $0

    /* RDI (boot_info) is untouched -- pass it straight on. */
    call    kmain

    /* kmain does not return. If it ever does, stop safely. */
.Lhang:
    cli
    hlt
    jmp     .Lhang
    .size _start, . - _start


    .section .bss
    .align 16
    .global stack_bottom
stack_bottom:
    .skip 64 * 1024             /* 64 KiB kernel stack */
    .global stack_top
stack_top:

/* Marks the stack as non-executable and silences a linker warning. */
    .section .note.GNU-stack, "", @progbits
