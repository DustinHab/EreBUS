/*
 * courier.S -- ring-3 program: keeps a letter box it is given, passes any other cargo on read-only.
 * - the attenuation is the program's decision; the kernel only prevents giving away more than held
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
    movabsq $0x6F20646573736170, %rdx   /* "passed o" */
    movabsq $0x6F2D64616572206E, %r10   /* "n read-o" */
    movabsq $0x2020202020796C6E, %r8    /* "nly     " */
    syscall
    jmp     .Lwait

.Lfriend:
    movq    %r14, %r15
    /* send(console, "TEXT", "i can re", "ach a fr", "iend now") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x622072657474656C, %rdx   /* "letter b" */
    movabsq $0x696563657220786F, %r10   /* "ox recei" */
    movabsq $0x2020202020646576, %r8    /* "ved     " */
    syscall
    jmp     .Lwait

.Lnobody:
    /* send(console, "TEXT", "no one t", "o give i", "t to    ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x657474656C206F6E, %rdx   /* "no lette" */
    movabsq $0x657920786F622072, %r10   /* "r box ye" */
    movabsq $0x2020202020202074, %r8    /* "t       " */
    syscall
    jmp     .Lwait

.Lno_pass:
    /* send(console, "TEXT", "i may no", "t pass t", "hat on  ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6665722073736170, %rdx   /* "pass ref" */
    movabsq $0x2020202064657375, %r10   /* "used    " */
    movabsq $0x2020202020202020, %r8    /* "        " */
    syscall
    jmp     .Lwait

    .section .note.GNU-stack, "", @progbits
