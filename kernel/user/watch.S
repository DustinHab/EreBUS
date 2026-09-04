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
    movabsq $0x7473206863746177, %rdx   /* "watch st" */
    movabsq $0x2020206465747261, %r10   /* "arted   " */
    movabsq $0x2020202020202020, %r8    /* "        " */
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
    movabsq $0x2064656863746177, %rdx   /* "watched " */
    movabsq $0x63207463656A626F, %r10   /* "object c" */
    movabsq $0x20206465676E6168, %r8    /* "hanged  " */
    syscall
    jmp     .Lround

.Llost:
    xorq    %r14, %r14
    /* send(console, "TEXT", "i can no", " longer ", "see it  ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x2064656863746177, %rdx   /* "watched " */
    movabsq $0x6C207463656A626F, %r10   /* "object l" */
    movabsq $0x202020202074736F, %r8    /* "ost     " */
    syscall
    jmp     .Lround

.Lno_read:
    xorq    %r14, %r14
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6665722064616572, %rdx   /* "read ref" */
    movabsq $0x2020202064657375, %r10   /* "used    " */
    movabsq $0x2020202020202020, %r8    /* "        " */
    syscall
    jmp     .Lround

    .section .note.GNU-stack, "", @progbits
