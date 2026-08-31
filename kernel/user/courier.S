/*
 * courier.S -- a program that gives things away.
 *
 * It waits, like the agent. What makes it different is what it does
 * with what arrives. A capability that lets it speak -- a letter box --
 * it keeps: that is somebody it can now reach. Anything else it treats
 * as cargo, and the moment it holds cargo and knows somebody, it passes
 * the cargo on -- read-only, whatever it was handed itself.
 *
 * That last part is the point of its existence. The attenuation is not
 * chosen by the shell and not chosen by the kernel; a program decided,
 * on its own, to give away less than it holds, and the kernel's only
 * involvement is making sure it could not have given away more. The
 * receiving side cannot tell and cannot argue: what arrives is what
 * arrives.
 */

    .section .user, "ax"

    .globl user_courier
    .align 16
user_courier:
    movq    %rdi, %r12              /* the console */
    movq    %rsi, %r13              /* the letter box */
    xorq    %r15, %r15              /* who it can reach: nobody yet */

    subq    $256, %rsp
    andq    $-16, %rsp
    movq    %rsp, %rbp

.Lwait:
    /* Stand aside; waiting is not urgent. */
    movq    $1, %rax
    syscall

    /* Clear the head of the buffer before receiving into it. */
    xorq    %rax, %rax
    movq    %rax,  0(%rbp)          /* tag */
    movq    %rax,  8(%rbp)          /* nwords, ncaps */
    movq    %rax, 16(%rbp)          /* words[0]: the rights that came */
    movq    %rax, 48(%rbp)          /* the capability */

    /* receive(inbox, buffer, wait) -- and this program waits. */
    movq    $3, %rax
    movq    %r13, %rdi
    movq    %rbp, %rsi
    xorq    %rdx, %rdx
    syscall
    cmpq    $0, %rax
    jne     .Lwait

    movl    12(%rbp), %ecx          /* how many capabilities */
    testl   %ecx, %ecx
    jz      .Lwait
    movq    48(%rbp), %r14          /* the capability itself */
    movq    16(%rbp), %rbx          /* the rights it came with */

    /* A capability for speaking is a person, not a parcel. */
    testq   $8, %rbx                /* CAP_CALL */
    jnz     .Lfriend

    /* Cargo. Without somebody to give it to, there is nothing to do
     * with it -- this program does not read things, it moves them. */
    testq   %r15, %r15
    jz      .Lnobody

    /* pass(friend, tag, capability, CAP_READ, 0).
     *
     * The mask is the decision: whatever this arrived with, it leaves
     * as read-only. The kernel intersects the mask with what is
     * actually held, so this line could never smuggle out more. */
    movq    $6, %rax
    movq    %r15, %rdi
    movq    $0x53534150, %rsi       /* "PASS" */
    movq    %r14, %rdx
    movq    $1, %r10                /* CAP_READ and nothing else */
    xorq    %r8, %r8
    syscall

    cmpq    $0, %rax
    jne     .Lno_pass

    /* send(console, "TEXT", "passed i", "t on, re", "ad only ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6920646573736170, %rdx
    movabsq $0x6572202C6E6F2074, %r10
    movabsq $0x20796C6E6F206461, %r8
    syscall
    jmp     .Lwait

.Lfriend:
    movq    %r14, %r15
    /* send(console, "TEXT", "i can re", "ach a fr", "iend now") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6572206E61632069, %rdx
    movabsq $0x7266206120686361, %r10
    movabsq $0x776F6E20646E6569, %r8
    syscall
    jmp     .Lwait

.Lnobody:
    /* send(console, "TEXT", "no one t", "o give i", "t to    ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x7420656E6F206F6E, %rdx
    movabsq $0x692065766967206F, %r10
    movabsq $0x202020206F742074, %r8
    syscall
    jmp     .Lwait

.Lno_pass:
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
