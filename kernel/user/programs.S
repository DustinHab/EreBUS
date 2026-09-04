/*
 * programs.S -- the ring-3 programs: own section of the kernel image, mapped read-and-execute, position-independent.
 *
 * System call convention:
 *   rax  the call number
 *   rdi rsi rdx r10 r8   the arguments
 *   rax  the result
 * - r10 instead of rcx: syscall leaves the return address in rcx and the flags in r11
 *
 *   0  exit(code)
 *   1  yield()
 *   2  send(handle, tag, word0, word1, word2)
 *   3  receive(handle, buffer)
 * - no open, no lookup by name, no granting call; the first capability arrives in rdi
 */

    .section .user, "ax"

/* ------------------------------------------------------------------ */
/* A well-behaved program: says hello through the capability it was
 * given, yields a few times, and leaves.                              */
/* ------------------------------------------------------------------ */

    .globl user_hello
    .align 16
user_hello:
    movq    %rdi, %rbx              /* the console capability we were given */

    /* send(console, tag "TEXT", "hello fr", "om ring ", "3 by ipc") */
    movq    $2, %rax
    movq    %rbx, %rdi
    movq    $0x54584554, %rsi       /* "TEXT" */
    movabsq $0x75203320676E6972, %rdx   /* "ring 3 u" */
    movabsq $0x7320637069202C70, %r10   /* "p, ipc s" */
    movabsq $0x20206B6F20646E65, %r8    /* "end ok  " */
    syscall

    /* Give the rest of the system a few turns, to show that a process
     * in ring 3 takes part in scheduling like anything else. */
    movq    $3, %rbp
.Lyield_loop:
    movq    $1, %rax
    syscall
    decq    %rbp
    jnz     .Lyield_loop

    /* send(console, "TEXT", "and now", " leaving", "  ring 3") */
    movq    $2, %rax
    movq    %rbx, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x70203320676E6972, %rdx   /* "ring 3 p" */
    movabsq $0x65206D6172676F72, %r10   /* "rogram e" */
    movabsq $0x2020202073746978, %r8    /* "xits    " */
    syscall

    movq    $0, %rax                /* exit(0) */
    movq    $0, %rdi
    syscall

    /* exit does not return. If it somehow does, stop rather than run
     * into whatever follows. */
.Lhalt_hello:
    jmp     .Lhalt_hello

/* ------------------------------------------------------------------ */
/* A misbehaving program: reaches for the kernel.
 *
 * Nothing about this is clever. It is the obvious thing an attacker
 * tries first, and the point is what happens next: the process is
 * ended and the machine carries on.                                   */
/* ------------------------------------------------------------------ */

    .globl user_trespass
    .align 16
user_trespass:
    movq    %rdi, %rbx

    /* Announce the attempt through the one capability we hold, so the
     * log shows the order of events. */
    movq    $2, %rax
    movq    %rbx, %rdi
    movq    $0x54584554, %rsi
    movabsq $0x20676E6964616572, %rdx   /* "reading " */
    movabsq $0x6D206C656E72656B, %r10   /* "kernel m" */
    movabsq $0x20202079726F6D65, %r8    /* "emory   " */
    syscall

    /* The kernel's own code, at the address the log prints on every
     * boot. Reading it must fail. */
    movabsq $0xFFFFFFFF80200000, %rax
    movq    (%rax), %rcx

    /* Not reached. If the read ever succeeds, say so loudly rather than
     * letting the test pass by accident. */
    movq    $2, %rax
    movq    %rbx, %rdi
    movq    $0x4641494C, %rsi            /* "FAIL" */
    movabsq $0x6D206C656E72656B, %rdx   /* "kernel m" */
    movabsq $0x61772079726F6D65, %r10   /* "emory wa" */
    movabsq $0x2020646165722073, %r8    /* "s read  " */
    syscall

    movq    $0, %rax
    movq    $1, %rdi
    syscall
.Lhalt_trespass:
    jmp     .Lhalt_trespass

    .section .note.GNU-stack, "", @progbits
