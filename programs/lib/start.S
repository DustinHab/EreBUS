/*
 * start.S -- the entry of the clang-built program: the stack guard drawn from the cycle counter, then
 * main(console, inbox) with the two capabilities the kernel hands over in rdi and rsi, then exit(its answer).
 * The machine's own compiler lays down the same _start itself (cc.c), so this file is for the host build only.
 */

    .section .text.start, "ax"
    .globl _start
_start:
    movq    %rdi, %r12
    movq    %rsi, %r13

    rdtsc
    shlq    $32, %rdx
    orq     %rdx, %rax
    movabsq $0x9E3779B97F4A7C15, %rdx
    xorq    %rdx, %rax
    movq    %rax, __stack_chk_guard(%rip)

    movq    %r12, %rdi
    movq    %r13, %rsi
    andq    $-16, %rsp
    call    main

    movq    %rax, %rdi          /* exit(what main answered) */
    movq    $0, %rax
    syscall
1:  jmp     1b

    .section .note.GNU-stack, "", @progbits
