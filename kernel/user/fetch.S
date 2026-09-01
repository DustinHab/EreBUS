/*
 * fetch.S -- the program that asks the network for pages.
 *
 * It holds three things at most: the console, its letter box, and --
 * given to it by the kernel the moment it starts -- a send-only way
 * to the wire. That third capability IS the internet here: whoever
 * holds the fetch program can ask for pages, and whoever does not
 * cannot, and there is no call either could make to change that.
 *
 * The work is one gesture: whatever it is handed after the wire, it
 * passes on to the wire, rights and all. The network service reads
 * the first line of the text, fetches, and writes the page back into
 * the same object. fetch never sees a byte of it -- it is a doorman,
 * not a reader.
 */

    .section .user, "ax"

    .globl user_fetch
    .align 16
user_fetch:
    movq    %rdi, %r12              /* the console */
    movq    %rsi, %r13              /* the letter box */
    xorq    %r14, %r14              /* the wire, once it arrives */

    subq    $256, %rsp
    andq    $-16, %rsp
    movq    %rsp, %rbp

    /* send(console, "TEXT", "i fetch ", "what is ", "asked   ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x2068637465662069, %rdx
    movabsq $0x2073692074616877, %r10
    movabsq $0x20202064656B7361, %r8
    syscall

.Lwait:
    movq    $1, %rax                /* yield */
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

    movl    12(%rbp), %ecx          /* how many capabilities came */
    testl   %ecx, %ecx
    jz      .Lwait
    movq    48(%rbp), %rbx          /* the first of them */

    testq   %r14, %r14
    jnz     .Lask

    /* The first gift is the wire. Everything after it is a page to
     * go and get. */
    movq    %rbx, %r14

    /* send(console, "TEXT", "i can re", "ach outs", "ide now ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6572206E61632069, %rdx
    movabsq $0x7374756F20686361, %r10
    movabsq $0x20776F6E20656469, %r8
    syscall
    jmp     .Lwait

.Lask:
    /* pass(wire, "PAGE", the text, reading and writing, 0) */
    movq    $6, %rax
    movq    %r14, %rdi
    movq    $0x45474150, %rsi
    movq    %rbx, %rdx
    movq    $3, %r10
    xorq    %r8, %r8
    syscall
    cmpq    $0, %rax
    jne     .Lrefused

    /* send(console, "TEXT", "asked fo", "r the pa", "ge      ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6F662064656B7361, %rdx
    movabsq $0x6170206568742072, %r10
    movabsq $0x2020202020206567, %r8
    syscall
    jmp     .Lwait

.Lrefused:
    /* send(console, "TEXT", "i may no", "t pass t", "hat on  ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6F6E2079616D2069, %rdx
    movabsq $0x7420737361702074, %r10
    movabsq $0x20206E6F20746168, %r8
    syscall
    jmp     .Lwait

    .section .note.GNU-stack, "", @progbits
