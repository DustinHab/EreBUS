/*
 * wipe.S -- ring-3 program: zeroes the whole payload; needs the write right only, no read.
 * - earlier generations on the disk keep what stood before
 */

    .section .user, "ax"

    .globl user_wipe
    .align 16
user_wipe:
    movq    %rdi, %r12              /* the console */
    movq    %rsi, %r13              /* the letter box */

    subq    $256, %rsp
    andq    $-16, %rsp
    movq    %rsp, %rbp

    /* send(console, "TEXT", "i blank ", "what i a", "m given ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x206B6E616C622069, %rdx
    movabsq $0x6120692074616877, %r10
    movabsq $0x206E65766967206D, %r8
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
    movq    48(%rbp), %r14          /* the thing to blank */
    xorq    %r15, %r15              /* offset */

.Lchunk:
    movq    $5, %rax                /* write(thing, offset, 0) */
    movq    %r14, %rdi
    movq    %r15, %rsi
    xorq    %rdx, %rdx
    syscall
    cmpq    $0, %rax
    jne     .Ldone

    addq    $8, %r15
    jmp     .Lchunk

.Ldone:
    testq   %r15, %r15
    jz      .Lno_write

    /* send(console, "TEXT", "blanked.", " ask the", " past   ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x2E64656B6E616C62, %rdx
    movabsq $0x656874206B736120, %r10
    movabsq $0x2020207473617020, %r8
    syscall
    jmp     .Lwait

.Lno_write:
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6F6E2079616D2069, %rdx    /* "i may no" */
    movabsq $0x65676E6168632074, %r10    /* "t change" */
    movabsq $0x2020207461687420, %r8     /* " that   " */
    syscall
    jmp     .Lwait

    .section .note.GNU-stack, "", @progbits
