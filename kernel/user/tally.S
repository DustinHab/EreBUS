/*
 * tally.S -- ring-3 program: counts words and lines of a readable text; needs the read right only.
 */

    .section .user, "ax"

    .globl user_tally
    .align 16
user_tally:
    movq    %rdi, %r12              /* the console */
    movq    %rsi, %r13              /* the letter box */

    subq    $256, %rsp
    andq    $-16, %rsp
    movq    %rsp, %rbp

    /* send(console, "TEXT", "i count ", "what i a", "m shown ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x747320796C6C6174, %rdx   /* "tally st" */
    movabsq $0x2020206465747261, %r10   /* "arted   " */
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
    movq    48(%rbp), %r14          /* what to count */

    xorq    %r15, %r15              /* offset */
    xorq    %rbx, %rbx              /* words */
    movq    $0, 128(%rbp)           /* lines */
    movq    $0, 136(%rbp)           /* inside a word right now */
    movq    $0, 144(%rbp)           /* read anything at all yet */

.Lchunk:
    movq    $4, %rax                /* read(thing, offset) */
    movq    %r14, %rdi
    movq    %r15, %rsi
    syscall
    cmpq    $-1, %rax
    je      .Lfinish
    movq    $1, 144(%rbp)

    movq    %rax, %r8
    movq    $8, %rcx
.Lbyte:
    movzbl  %r8b, %eax
    testb   %al, %al
    jz      .Lfinish                /* the text ends before the payload */

    cmpb    $10, %al                /* newline: a line, and a boundary */
    jne     .Lnot_nl
    incq    128(%rbp)
    movq    $0, 136(%rbp)
    jmp     .Lnext
.Lnot_nl:
    cmpb    $32, %al                /* space: a boundary */
    jne     .Lletter
    movq    $0, 136(%rbp)
    jmp     .Lnext
.Lletter:
    cmpq    $0, 136(%rbp)           /* stepping into a word counts it */
    jne     .Lnext
    incq    %rbx
    movq    $1, 136(%rbp)
.Lnext:
    shrq    $8, %r8
    decq    %rcx
    jnz     .Lbyte

    addq    $8, %r15
    jmp     .Lchunk

.Lfinish:
    cmpq    $0, 144(%rbp)
    je      .Lno_read               /* not one byte was readable */

.Lsay:
    /* "NNNN words, NNNN lines  ", the numbers right-aligned. Built on
     * the stack byte by byte, then said in three pieces. */
    leaq    160(%rbp), %r9

    movq    %rbx, %rax              /* the words, into bytes 0..3 */
    movq    $3, %rcx
1:  xorq    %rdx, %rdx
    movq    $10, %rsi
    divq    %rsi
    addb    $48, %dl
    movb    %dl, (%r9,%rcx)
    testq   %rax, %rax
    jz      2f
    decq    %rcx
    jns     1b
    jmp     3f
2:  decq    %rcx
    js      3f
    movb    $32, (%r9,%rcx)
    jmp     2b
3:
    movl    $0x726F7720, 164(%rbp)  /* " wor" */
    movl    $0x202C7364, 168(%rbp)  /* "ds, " */

    leaq    172(%rbp), %r9
    movq    128(%rbp), %rax         /* the lines, into bytes 12..15 */
    movq    $3, %rcx
4:  xorq    %rdx, %rdx
    movq    $10, %rsi
    divq    %rsi
    addb    $48, %dl
    movb    %dl, (%r9,%rcx)
    testq   %rax, %rax
    jz      5f
    decq    %rcx
    jns     4b
    jmp     6f
5:  decq    %rcx
    js      6f
    movb    $32, (%r9,%rcx)
    jmp     5b
6:
    movabsq $0x202073656E696C20, %rax
    movq    %rax, 176(%rbp)         /* " lines  " */

    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movq    160(%rbp), %rdx
    movq    168(%rbp), %r10
    movq    176(%rbp), %r8
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
