/*
 * syscall.c -- the kernel side of the ring 3 boundary.
 */
#include <eb/syscall.h>
#include <eb/proc.h>
#include <eb/msg.h>
#include <eb/thread.h>
#include <eb/gdt.h>
#include <eb/fmt.h>
#include <eb/io.h>

/* Per-processor data. syscall.S reads gs:0 and gs:8 directly, so the
 * layout is not free to change. */
typedef struct {
    u64 kernel_rsp;   /* offset 0: where a system call lands */
    u64 user_rsp;     /* offset 8: where it came from */
} percpu;

static percpu cpu0;

_Static_assert(sizeof(percpu) == 16, "syscall.S depends on this layout");

#define MSR_EFER          0xC0000080u
#define MSR_STAR          0xC0000081u
#define MSR_LSTAR         0xC0000082u
#define MSR_SFMASK        0xC0000084u
#define MSR_GS_BASE       0xC0000101u
#define MSR_KERNEL_GS_BASE 0xC0000102u

#define EFER_SCE (1ULL << 0)

static u64 rdmsr(u32 msr)
{
    u32 lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | lo;
}

static void wrmsr(u32 msr, u64 v)
{
    __asm__ volatile ("wrmsr" :: "c"(msr), "a"((u32)v), "d"((u32)(v >> 32)));
}

extern void syscall_entry(void);

void percpu_init(void)
{
    /* While in the kernel GS points at this structure; while in user
     * mode it points at whatever the program set, and ours is parked in
     * the shadow register. swapgs exchanges the two, which is how the
     * entry path finds a trustworthy stack without dereferencing
     * anything the program controls. */
    wrmsr(MSR_GS_BASE, (u64)&cpu0);
    wrmsr(MSR_KERNEL_GS_BASE, 0);
}

void percpu_set_kernel_stack(u64 stack_top)
{
    cpu0.kernel_rsp = stack_top;
}

void syscall_init(void)
{
    /* STAR holds two selector bases. The kernel one is used on entry.
     * The user one is what sysret adds to: code becomes base + 16 and
     * stack becomes base + 8, which is why the descriptor table has
     * user data sitting immediately before user code. */
    u64 star = ((u64)(SEL_KERNEL_CODE) << 32) |
               ((u64)(SEL_USER_DATA - 8) << 48);
    wrmsr(MSR_STAR, star);
    wrmsr(MSR_LSTAR, (u64)syscall_entry);

    /* Flags cleared on the way in. Interrupts first: the entry path is
     * still on the user's stack for two instructions, and an interrupt
     * there would push onto it. Direction, because the ABI wants it
     * clear and the program is under no obligation to oblige. And the
     * alignment check flag, which doubles as the SMAP override -- a
     * program that could leave it set would hand the kernel's own
     * accesses free rein over user memory. */
    wrmsr(MSR_SFMASK, 0x200 | 0x400 | 0x40000 | 0x100);

    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_SCE);
}

/* ------------------------------------------------------------------ */

static u64 do_send(domain *d, u64 handle, u64 tag,
                   u64 w0, u64 w1, u64 w2)
{
    message m = { 0 };
    m.tag = tag;
    m.nwords = 3;
    m.words[0] = w0;
    m.words[1] = w1;
    m.words[2] = w2;
    m.ncaps = 0;

    if (!port_send(d, (cap_handle)handle, &m)) {
        /* Either the capability is not there, or the port is full. The
         * caller is not told which: the difference is information about
         * what it does not hold. */
        return SYS_DENIED;
    }
    return SYS_OK;
}

static u64 do_receive(domain *d, u64 handle, u64 user_buffer)
{
    message m = { 0 };
    if (!port_receive(d, (cap_handle)handle, &m)) return SYS_DENIED;

    /* Writing into the program's memory is the one place the kernel
     * touches an address the program chose, so it goes through the
     * checked path rather than a plain store. */
    if (!copy_to_user(user_buffer, &m, sizeof(m))) return SYS_DENIED;
    return SYS_OK;
}

u64 syscall_dispatch(u64 nr, u64 a0, u64 a1, u64 a2, u64 a3, u64 a4);

u64 syscall_dispatch(u64 nr, u64 a0, u64 a1, u64 a2, u64 a3, u64 a4)
{
    domain *d = thread_domain(sched_current());
    if (!d) return SYS_DENIED;

    switch (nr) {
    case SYS_EXIT:
        thread_exit();              /* does not return */
        return SYS_OK;

    case SYS_YIELD:
        sched_yield();
        return SYS_OK;

    case SYS_SEND:
        return do_send(d, a0, a1, a2, a3, a4);

    case SYS_RECEIVE:
        return do_receive(d, a0, a1);

    default:
        return SYS_BADCALL;
    }
}
