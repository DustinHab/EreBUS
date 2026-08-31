/*
 * cipher.S -- a program that turns writing over.
 *
 * Hand it something it may change and it walks the whole payload,
 * rotating every letter thirteen places. The rotation is its own
 * inverse: hand it the same thing again and the writing comes back.
 * Nothing is stored, no key is kept -- the transformation is the whole
 * of it, applied in place to the one object everyone is looking at.
 *
 * It makes the rights legible in the plainest way there is. Held with
 * writing, it changes what everyone sees, immediately. Held without,
 * it changes nothing and says so -- not because it checked a rule, but
 * because the kernel would not produce the object for a write.
 */

    .section .user, "ax"

    .globl user_cipher
    .align 16
user_cipher:
    movq    %rdi, %r12              /* the console */
    movq    %rsi, %r13              /* the letter box */

    subq    $256, %rsp
    andq    $-16, %rsp
    movq    %rsp, %rbp

    /* send(console, "TEXT", "i turn w", "riting o", "ver     ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x77206E7275742069, %rdx
    movabsq $0x6F20676E69746972, %r10
    movabsq $0x2020202020726576, %r8
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
    movq    48(%rbp), %r14          /* the thing to turn over */
    xorq    %r15, %r15              /* how far we have come */

.Lchunk:
    movq    $4, %rax                /* read(thing, offset) */
    movq    %r14, %rdi
    movq    %r15, %rsi
    syscall
    cmpq    $-1, %rax
    je      .Ldone

    /* Rotate the eight letters in place. Everything that is not a
     * letter passes through untouched, which is what keeps the shape
     * of the text -- and its length -- exactly as it was. */
    movq    %rax, %rbx
    xorq    %rdx, %rdx              /* rebuilt value */
    movq    $8, %rcx
.Lbyte:
    movzbl  %bl, %eax

    cmpb    $'A', %al
    jb      .Lkeep
    cmpb    $'Z', %al
    jbe     .Lupper
    cmpb    $'a', %al
    jb      .Lkeep
    cmpb    $'z', %al
    ja      .Lkeep

    addb    $13, %al                /* lower case */
    cmpb    $'z', %al
    jbe     .Lkeep
    subb    $26, %al
    jmp     .Lkeep
.Lupper:
    addb    $13, %al
    cmpb    $'Z', %al
    jbe     .Lkeep
    subb    $26, %al
.Lkeep:
    shrdq   $8, %rax, %rdx          /* slide the byte in from the top */
    shrq    $8, %rbx
    decq    %rcx
    jnz     .Lbyte

    /* write(thing, offset, turned) */
    movq    $5, %rax
    movq    %r14, %rdi
    movq    %r15, %rsi
    syscall
    cmpq    $0, %rax
    jne     .Lno_write

    addq    $8, %r15
    jmp     .Lchunk

.Ldone:
    testq   %r15, %r15
    jz      .Lno_read

    /* send(console, "TEXT", "turned; ", "again tu", "rns back") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x203B64656E727574, %rdx
    movabsq $0x7574206E69616761, %r10
    movabsq $0x6B63616220736E72, %r8
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
