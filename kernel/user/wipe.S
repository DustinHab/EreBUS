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
    movabsq $0x6174732065706977, %rdx   /* "wipe sta" */
    movabsq $0x2020202064657472, %r10   /* "rted    " */
    movabsq $0x2020202020202020, %r8    /* "        " */
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
    movabsq $0x2064616F6C796170, %rdx   /* "payload " */
    movabsq $0x202064656F72657A, %r10   /* "zeroed  " */
    movabsq $0x2020202020202020, %r8    /* "        " */
    syscall
    jmp     .Lwait

.Lno_write:
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6572206574697277, %rdx   /* "write re" */
    movabsq $0x2020206465737566, %r10   /* "fused   " */
    movabsq $0x2020202020202020, %r8    /* "        " */
    syscall
    jmp     .Lwait

    .section .note.GNU-stack, "", @progbits
