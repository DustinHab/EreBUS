/*
 * programs.S -- the first programs that run outside the kernel.
 *
 * These live in their own section of the kernel image and are mapped
 * into a process's low half as read-and-execute. They are written in
 * assembly and reference nothing absolute, so the same bytes work
 * wherever they are mapped.
 *
 * The system call convention:
 *
 *   rax  the call number
 *   rdi rsi rdx r10 r8   the arguments
 *   rax  the result
 *
 * r10 rather than rcx for the fourth argument because the syscall
 * instruction puts the return address in rcx, and the flags in r11.
 * Both are gone by the time the kernel sees the call.
 *
 *   0  exit(code)
 *   1  yield()
 *   2  send(handle, tag, word0, word1, word2)
 *   3  receive(handle, buffer)
 *
 * That is the entire interface. There is nothing here to open a file
 * with, no way to ask for a resource by name, and no call that grants
 * anything. A program starts holding whatever capabilities it was
 * handed -- the first of them arrives in rdi -- and that is the extent
 * of what it can ever do.
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
    movabsq $0x7266206F6C6C6568, %rdx
    movabsq $0x20676E6972206D6F, %r10
    movabsq $0x6370692079622033, %r8
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
    movabsq $0x20776F6E20646E61, %rdx    /* "and now " */
    movabsq $0x20676E697661656C, %r10    /* "leaving " */
    movabsq $0x20203320676E6972, %r8     /* "ring 3  " */
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
    movabsq $0x676E696863616572, %rdx    /* "reaching" */
    movabsq $0x72656B20726F6620, %r10    /* " for ker" */
    movabsq $0x206D656D206C656E, %r8     /* "nel mem " */
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
    movabsq $0x2064616572206920, %rdx    /* " i read " */
    movabsq $0x206C656E72656B20, %r10    /* " kernel " */
    movabsq $0x2079726F6D656D20, %r8     /* " memory " */
    syscall

    movq    $0, %rax
    movq    $1, %rdi
    syscall
.Lhalt_trespass:
    jmp     .Lhalt_trespass

    .section .note.GNU-stack, "", @progbits
