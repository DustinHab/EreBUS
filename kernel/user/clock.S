/*
 * clock.S -- a program that keeps the time somewhere.
 *
 * It starts knowing what time it is -- the one fact the kernel answers
 * without any capability, because the time of day belongs to nobody --
 * and holding nowhere to put it. Point it at something writable and it
 * writes the time there, eight characters, once a second, for as long
 * as the reference lasts.
 *
 * What that buys is an object that is alive. Whoever looks at the same
 * object sees the same seconds tick, not because anything is refreshed
 * or fetched but because there is only one object and the clock keeps
 * writing into it. Take the reference away and the time simply stops
 * being kept -- the last written second stands, which is what a stopped
 * clock honestly looks like.
 */

    .section .user, "ax"

    .globl user_clock
    .align 16
user_clock:
    movq    %rdi, %r12              /* the console */
    movq    %rsi, %r13              /* the letter box */
    xorq    %r14, %r14              /* where to write: nowhere yet */
    movq    $-1, %r15               /* the last second written */

    subq    $256, %rsp
    andq    $-16, %rsp
    movq    %rsp, %rbp

    /* send(console, "TEXT", "i know w", "hat time", " it is  ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x7720776F6E6B2069, %rdx
    movabsq $0x656D697420746168, %r10
    movabsq $0x2020736920746920, %r8
    syscall

.Lround:
    movq    $1, %rax                /* yield: a clock is never urgent */
    syscall

    /* Look in the letter box without waiting -- there is a clock to
     * keep, and waiting would stop it. */
    xorq    %rax, %rax
    movq    %rax,  8(%rbp)          /* nwords, ncaps */
    movq    %rax, 48(%rbp)          /* the capability */
    movq    $3, %rax
    movq    %r13, %rdi
    movq    %rbp, %rsi
    movq    $1, %rdx                /* do not wait */
    syscall
    cmpq    $0, %rax
    jne     .Ltick

    movl    12(%rbp), %ecx
    testl   %ecx, %ecx
    jz      .Ltick
    movq    48(%rbp), %r14          /* a new place to keep the time */
    movq    $-1, %r15               /* write it out at once */

    /* send(console, "TEXT", "i will k", "eep it c", "urrent  ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6B206C6C69772069, %rdx
    movabsq $0x6320746920706565, %r10
    movabsq $0x2020746E65727275, %r8
    syscall

.Ltick:
    testq   %r14, %r14
    jz      .Lround

    movq    $7, %rax                /* clock() -> seconds since midnight */
    syscall
    cmpq    %rax, %r15
    je      .Lround                 /* still the same second */
    movq    %rax, %r15

    /* Turn the seconds into "hh:mm:ss", packed little-endian so the
     * first character is the lowest byte. */
    xorq    %rdx, %rdx
    movq    $3600, %rcx
    divq    %rcx                    /* rax = hours, rdx = the rest */
    movq    %rax, %r9
    movq    %rdx, %rax
    xorq    %rdx, %rdx
    movq    $60, %rcx
    divq    %rcx                    /* rax = minutes, rdx = seconds */
    movq    %rax, %r10
    movq    %rdx, %r11

    xorq    %rbx, %rbx
    movq    $10, %rcx

    movq    %r11, %rax              /* seconds last: highest bytes */
    xorq    %rdx, %rdx
    divq    %rcx
    addq    $48, %rdx               /* ones */
    shlq    $56, %rdx
    orq     %rdx, %rbx
    addq    $48, %rax               /* tens */
    shlq    $48, %rax
    orq     %rax, %rbx

    movq    $0x3A, %rax             /* ':' between minutes and seconds */
    shlq    $40, %rax
    orq     %rax, %rbx

    movq    %r10, %rax
    xorq    %rdx, %rdx
    divq    %rcx
    addq    $48, %rdx
    shlq    $32, %rdx
    orq     %rdx, %rbx
    addq    $48, %rax
    shlq    $24, %rax
    orq     %rax, %rbx

    movq    $0x3A, %rax             /* ':' between hours and minutes */
    shlq    $16, %rax
    orq     %rax, %rbx

    movq    %r9, %rax
    xorq    %rdx, %rdx
    divq    %rcx
    addq    $48, %rdx
    shlq    $8, %rdx
    orq     %rdx, %rbx
    addq    $48, %rax
    orq     %rax, %rbx

    /* write(place, 0, "hh:mm:ss") */
    movq    $5, %rax
    movq    %r14, %rdi
    xorq    %rsi, %rsi
    movq    %rbx, %rdx
    syscall
    cmpq    $0, %rax
    je      .Lround

    /* The reference did not allow it, or was taken back. Say so once
     * and stop trying; a clock that argues is worse than one that
     * stands. */
    xorq    %r14, %r14
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6F6E2079616D2069, %rdx    /* "i may no" */
    movabsq $0x2065746972772074, %r10    /* "t write " */
    movabsq $0x2020206572656874, %r8     /* "there   " */
    syscall
    jmp     .Lround

    .section .note.GNU-stack, "", @progbits
