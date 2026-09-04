/*
 * sums.S -- ring-3 program: says one checksum of the payload; equal before and after a journey = unchanged.
 */

    .section .user, "ax"

    .globl user_sums
    .align 16
user_sums:
    movq    %rdi, %r12              /* the console */
    movq    %rsi, %r13              /* the letter box */

    subq    $256, %rsp
    andq    $-16, %rsp
    movq    %rsp, %rbp

    /* send(console, "TEXT", "i finger", "print th", "ings    ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x7265676E69662069, %rdx
    movabsq $0x687420746E697270, %r10
    movabsq $0x2020202073676E69, %r8
    syscall

.Lwait:
    movq    $1, %rax
    syscall

    xorq    %rax, %rax
    movq    %rax,  8(%rbp)
    movq    %rax, 48(%rbp)
    movq    $3, %rax                /* receive(inbox, buffer, wait) */
    movq    %r13, %rdi
    movq    %rbp, %rsi
    xorq    %rdx, %rdx
    syscall
    cmpq    $0, %rax
    jne     .Lwait

    movl    12(%rbp), %ecx
    testl   %ecx, %ecx
    jz      .Lwait
    movq    48(%rbp), %r14          /* the thing to fingerprint */

    xorq    %r15, %r15              /* offset */
    movabsq $0xCBF29CE484222325, %rbx   /* FNV-1a, from its beginning */

.Lchunk:
    movq    $4, %rax                /* read(thing, offset) */
    movq    %r14, %rdi
    movq    %r15, %rsi
    syscall
    cmpq    $-1, %rax
    je      .Ldone
    movq    %rax, 96(%rbp)

    /* Eight bytes into the running hash. */
    movq    $0, 104(%rbp)           /* which byte */
.Lbyte:
    movq    104(%rbp), %rcx
    movzbl  96(%rbp,%rcx), %eax
    xorq    %rax, %rbx
    movq    %rbx, %rax
    movabsq $0x100000001B3, %rdx
    mulq    %rdx
    movq    %rax, %rbx
    incq    104(%rbp)
    cmpq    $8, 104(%rbp)
    jb      .Lbyte

    addq    $8, %r15
    jmp     .Lchunk

.Ldone:
    testq   %r15, %r15
    jz      .Lno_read

    /* "sum " and sixteen hex digits, built on the stack and said in
     * three pieces. */
    movl    $0x206D7573, 152(%rbp)  /* "sum " */
    movq    %rbx, %rdx
    movq    $0, 104(%rbp)
.Lhex:
    rolq    $4, %rdx                /* highest nibble first */
    movq    %rdx, %rax
    andq    $15, %rax
    cmpq    $10, %rax
    jb      1f
    addq    $39, %rax               /* 'a' - '0' - 10 */
1:  addq    $48, %rax
    movq    104(%rbp), %rcx
    movb    %al, 156(%rbp,%rcx)
    incq    104(%rbp)
    cmpq    $16, 104(%rbp)
    jb      .Lhex
    movl    $0x20202020, 172(%rbp)

    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movq    152(%rbp), %rdx
    movq    160(%rbp), %r10
    movq    168(%rbp), %r8
    syscall
    jmp     .Lwait

.Lno_read:
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6F6E2079616D2069, %rdx    /* "i may no" */
    movabsq $0x72206E6576652074, %r10    /* "t even r" */
    movabsq $0x2020746920646165, %r8     /* "ead it  " */
    syscall
    jmp     .Lwait

    .section .note.GNU-stack, "", @progbits
