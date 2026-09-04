/*
 * watch.S -- ring-3 program: says when the opening of a readable object changes, at most once a second.
 */

    .section .user, "ax"

    .globl user_watch
    .align 16
user_watch:
    movq    %rdi, %r12              /* the console */
    movq    %rsi, %r13              /* the letter box */
    xorq    %r14, %r14              /* what it watches: nothing yet */

    subq    $256, %rsp
    andq    $-16, %rsp
    movq    %rsp, %rbp

    /* send(console, "TEXT", "i watch ", "for chan", "ges     ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x2068637461772069, %rdx
    movabsq $0x6E61686320726F66, %r10
    movabsq $0x2020202020736567, %r8
    syscall

.Lround:
    movq    $1, %rax                /* yield; watching is patient work */
    syscall

    /* Look in the letter box without waiting. */
    xorq    %rax, %rax
    movq    %rax,  8(%rbp)
    movq    %rax, 48(%rbp)
    movq    $3, %rax
    movq    %r13, %rdi
    movq    %rbp, %rsi
    movq    $1, %rdx                /* do not wait */
    syscall
    cmpq    $0, %rax
    jne     .Llook

    movl    12(%rbp), %ecx
    testl   %ecx, %ecx
    jz      .Llook
    movq    48(%rbp), %r14          /* something new to watch */

    /* First sight becomes the baseline; nothing is reported for it. */
    movq    $4, %rax
    movq    %r14, %rdi
    xorq    %rsi, %rsi
    syscall
    cmpq    $-1, %rax
    je      .Lno_read
    movq    %rax, %r15

    movq    $4, %rax
    movq    %r14, %rdi
    movq    $8, %rsi
    syscall
    movq    %rax, %rbx
    movq    $-1, 96(%rbp)           /* free to speak at once */

.Llook:
    testq   %r14, %r14
    jz      .Lround

    movq    $4, %rax                /* read(watched, 0) */
    movq    %r14, %rdi
    xorq    %rsi, %rsi
    syscall
    cmpq    $-1, %rax
    je      .Llost
    movq    %rax, %rcx

    movq    %rax, 104(%rbp)
    movq    $4, %rax                /* read(watched, 8) */
    movq    %r14, %rdi
    movq    $8, %rsi
    syscall

    cmpq    104(%rbp), %r15
    jne     .Lchanged
    cmpq    %rax, %rbx
    jne     .Lchanged
    jmp     .Lround

.Lchanged:
    movq    104(%rbp), %r15
    movq    %rax, %rbx

    /* At most one remark per second. */
    movq    $7, %rax                /* clock() */
    syscall
    cmpq    96(%rbp), %rax
    je      .Lround
    movq    %rax, 96(%rbp)

    /* send(console, "TEXT", "what i w", "atch has", " changed") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x7720692074616877, %rdx
    movabsq $0x7361682068637461, %r10
    movabsq $0x6465676E61686320, %r8
    syscall
    jmp     .Lround

.Llost:
    xorq    %r14, %r14
    /* send(console, "TEXT", "i can no", " longer ", "see it  ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6F6E206E61632069, %rdx
    movabsq $0x207265676E6F6C20, %r10
    movabsq $0x2020746920656573, %r8
    syscall
    jmp     .Lround

.Lno_read:
    xorq    %r14, %r14
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6F6E2079616D2069, %rdx    /* "i may no" */
    movabsq $0x72206E6576652074, %r10    /* "t even r" */
    movabsq $0x2020746920646165, %r8     /* "ead it  " */
    syscall
    jmp     .Lround

    .section .note.GNU-stack, "", @progbits
