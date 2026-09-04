/*
 * cipher.S -- ring-3 program: rotates every letter of the payload by thirteen, in place; its own inverse.
 * - without the write right it changes nothing and says so
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
    movabsq $0x7320726568706963, %rdx   /* "cipher s" */
    movabsq $0x2020646574726174, %r10   /* "tarted  " */
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
    movabsq $0x7061203331746F72, %rdx   /* "rot13 ap" */
    movabsq $0x2020206465696C70, %r10   /* "plied   " */
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

.Lno_read:
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6665722064616572, %rdx   /* "read ref" */
    movabsq $0x2020202064657375, %r10   /* "used    " */
    movabsq $0x2020202020202020, %r8    /* "        " */
    syscall
    jmp     .Lwait

    .section .note.GNU-stack, "", @progbits
