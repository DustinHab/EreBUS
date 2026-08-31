/*
* entry.S -- the ring 3 boundary, in both directions.
 *
 * Crossing into the kernel and back is the one place where a mistake is
 * not a bug but a hole, so both paths are written out rather than left
 * to a compiler.
 */

    .section .text

/* ------------------------------------------------------------------ */
/* Entering the kernel: the syscall instruction lands here.
 *
 * syscall does very little on its own. It does not switch stacks, so
 * we arrive still standing on the user's stack, which the user chose
 * and could have pointed anywhere. The first job is to get off it.
 *
 * That is what swapgs is for: it exchanges the GS base with a value
 * only the kernel can set, and behind it sits this thread's kernel
 * stack. Nothing the program controls is dereferenced before the
 * switch.                                                             */
/* ------------------------------------------------------------------ */

    .globl syscall_entry
    .type syscall_entry, @function
syscall_entry:
    swapgs
    movq    %rsp, %gs:8             /* stash the user stack pointer */
    movq    %gs:0, %rsp             /* stand on our own */

    pushq   %rcx                    /* syscall left the return address here */
    pushq   %r11                    /* and the user's flags here */
    pushq   %rbp
    movq    %rsp, %rbp
    andq    $-16, %rsp              /* the ABI wants an aligned stack */

    sti                             /* a call may block; let the timer in */

    /* The kernel side is an ordinary C function, so the arguments have
     * to move from the syscall positions into the C ones. Each source
     * is read before anything overwrites it. */
    movq    %r8,  %r9               /* arg 5 */
    movq    %r10, %r8               /* arg 4 */
    movq    %rdx, %rcx              /* arg 3 */
    movq    %rsi, %rdx              /* arg 2 */
    movq    %rdi, %rsi              /* arg 1 */
    movq    %rax, %rdi              /* the call number */
    call    syscall_dispatch        /* result comes back in rax */

    cli
    movq    %rbp, %rsp
    popq    %rbp
    popq    %r11                    /* flags for sysret */
    popq    %rcx                    /* return address for sysret */

    /* Everything the kernel was free to use on the way through goes
     * back to zero. The result is in rax and the callee-saved registers
     * still hold what the program left in them; the rest would be
     * whatever the C code happened to finish with, which is a handful of
     * kernel addresses handed out on every single call. Six
     * instructions is a small price for not telling a program where the
     * kernel lives. */
    xorq    %rdi, %rdi
    xorq    %rsi, %rsi
    xorq    %rdx, %rdx
    xorq    %r8,  %r8
    xorq    %r9,  %r9
    xorq    %r10, %r10

    movq    %gs:8, %rsp             /* back onto the user stack */
    swapgs
    sysretq
    .size syscall_entry, . - syscall_entry

/* ------------------------------------------------------------------ */
/* Leaving for ring 3 the first time.
 *
 * There is no instruction for "start running in user mode", so we build
 * the stack frame an interrupt would have left behind and return from
 * an interrupt that never happened.
 *
 * user_enter(u64 entry, u64 stack, u64 arg0, u64 arg1)               */
/* ------------------------------------------------------------------ */

    .globl user_enter
    .type user_enter, @function
user_enter:
    cli
    movq    %rdi, %r14              /* entry point */
    movq    %rsi, %r13              /* user stack */
    movq    %rdx, %r15              /* first capability */
    movq    %rcx, %r12              /* second capability */

    swapgs                          /* GS belongs to user land now */

    pushq   $0x1B                   /* SS: user data, ring 3 */
    pushq   %r13                    /* RSP */
    pushq   $0x202                  /* RFLAGS: interrupts on, nothing else */
    pushq   $0x23                   /* CS: user code, ring 3 */
    pushq   %r14                    /* RIP */

    movq    %r15, %rdi              /* what it starts holding */
    movq    %r12, %rsi

    /* Everything else goes to zero.
     *
     * Whatever is left in a register crosses into ring 3 with us. A
     * stale kernel pointer would tell the program where the kernel
     * lives, which is precisely the knowledge that turns a small bug
     * into a working exploit. Zeroing costs a dozen instructions once
     * per process. */
    xorq    %rax, %rax
    xorq    %rbx, %rbx
    xorq    %rcx, %rcx
    xorq    %rdx, %rdx
    xorq    %rbp, %rbp
    xorq    %r8,  %r8
    xorq    %r9,  %r9
    xorq    %r10, %r10
    xorq    %r11, %r11
    xorq    %r12, %r12
    xorq    %r13, %r13
    xorq    %r14, %r14
    xorq    %r15, %r15

    iretq
    .size user_enter, . - user_enter

    .section .note.GNU-stack, "", @progbits
