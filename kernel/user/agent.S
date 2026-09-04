/*
 * agent.S -- ring-3 program: starts with console and letter box, waits for a capability, reports what it sees and tries to change it.
 * - no policy check anywhere; the kernel produces the object or does not
 */

    .section .user, "ax"

    .globl user_agent
    .align 16
user_agent:
    movq    %rdi, %r12              /* the console */
    movq    %rsi, %r13              /* the letter box */

    /* One buffer for the rest of its life. The kernel writes received
     * messages here, so it has to be somewhere the program owns; the
     * stack is the only writable memory a program starts with. */
    subq    $256, %rsp
    andq    $-16, %rsp
    movq    %rsp, %rbp

    /* send(console, "TEXT", "i hold n", "othing y", "et      ") */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6261706163206F6E, %rdx   /* "no capab" */
    movabsq $0x6579207974696C69, %r10   /* "ility ye" */
    movabsq $0x2020202020202074, %r8    /* "t       " */
    syscall

.Lwait:
    /* Stand aside for everything else first. Waiting is not urgent, and
     * a program that spins on an empty letter box is a program that
     * starves the machine it is asking a favour of. */
    movq    $1, %rax
    syscall

    /* Clear the head of the buffer. A receive that finds nothing leaves
     * the buffer untouched, and acting on what happened to be there
     * before would be acting on nothing at all. */
    xorq    %rax, %rax
    movq    %rax,  0(%rbp)          /* tag */
    movq    %rax,  8(%rbp)          /* how many words, how many caps */
    movq    %rax, 48(%rbp)          /* the first capability */

    /* receive(inbox, buffer, wait). The third argument says whether an
     * empty letter box should hold us; this program has nothing better
     * to do, so it waits. */
    movq    $3, %rax
    movq    %r13, %rdi
    movq    %rbp, %rsi
    xorq    %rdx, %rdx
    syscall
    cmpq    $0, %rax
    jne     .Lwait                  /* nothing there yet */

    /* The message layout: tag at 0, how many words at 8, how many
     * capabilities at 12, the words at 16, the capabilities at 48.
     *
     * A message carrying nothing is not nothing: it says something has
     * changed, and the only way to find out what is to try again with
     * the handle already held. That handle starts at zero, which names
     * nothing, so the first attempt fails exactly as an attempt on
     * something withdrawn would. */
    movl    12(%rbp), %ecx
    testl   %ecx, %ecx
    jz      .Ltry
    movq    48(%rbp), %r14

.Ltry:

    /* read(handle, 0). Not "check whether I may read, then read" --
     * there is only the one step, and it fails or it does not. */
    movq    $4, %rax
    movq    %r14, %rdi
    movq    $0, %rsi
    syscall
    movq    %rax, %r15

    cmpq    $-1, %r15
    je      .Lno_read

    /* read(handle, 8) for the second half of the line */
    movq    $4, %rax
    movq    %r14, %rdi
    movq    $8, %rsi
    syscall
    movq    %rax, %rbx

    /* send(console, "TEXT", "i see:  ", <first eight>, <next eight>) */
    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x20203A7364616572, %rdx   /* "reads:  " */
    movq    %r15, %r10
    movq    %rbx, %r8
    syscall

    /* write(handle, 0, " MARK!  "). If the reference carried the right
     * to write, the change appears in whatever else is looking at this
     * object, immediately and with nothing saved. */
    movq    $5, %rax
    movq    %r14, %rdi
    movq    $0, %rsi
    movabsq $0x2020214B52414D20, %rdx
    syscall

    cmpq    $0, %rax
    jne     .Lno_write

    movq    $2, %rax
    movq    %r12, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x6B6F206574697277, %rdx   /* "write ok" */
    movabsq $0x2020202020202020, %r10   /* "        " */
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
