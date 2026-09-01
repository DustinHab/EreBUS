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
#include <eb/time.h>
#include <eb/settings.h>

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

static u64 do_receive(domain *d, u64 handle, u64 user_buffer, u64 no_wait)
{
    message m = { 0 };

    /* Waiting is the default; a program with other work to do -- a
     * clock has a clock to keep -- asks not to, and an empty letter box
     * answers immediately instead of holding it. */
    if (no_wait) {
        if (!port_try_receive(d, (cap_handle)handle, &m))
            return SYS_WOULDFAIL;
    } else {
        if (!port_receive(d, (cap_handle)handle, &m)) return SYS_DENIED;
    }

    /* Writing into the program's memory is the one place the kernel
     * touches an address the program chose, so it goes through the
     * checked path rather than a plain store. */
    if (!copy_to_user(user_buffer, &m, sizeof(m))) return SYS_DENIED;
    return SYS_OK;
}

/* Reading and writing an object's payload, eight bytes at a time.
 *
 * A capability is not a permission slip to be shown at a door -- it is
 * the only way to name the thing at all. There is no path here that
 * takes an object and asks whether it may be read; the lookup either
 * produces the object or it does not, and it does not when the right is
 * missing. The check and the naming are the same step. */
static u64 do_read(domain *d, u64 handle, u64 offset)
{
    object *o = cap_lookup(d, (cap_handle)handle, CAP_READ);
    if (!o) return SYS_DENIED;

    const u8 *data = (const u8 *)obj_data(o);
    if (!data || offset + 8 > obj_size(o)) return SYS_DENIED;

    u64 v = 0;
    for (u32 i = 0; i < 8; i++) v |= (u64)data[offset + i] << (i * 8);
    return v;
}

static u64 do_write(domain *d, u64 handle, u64 offset, u64 value)
{
    object *o = cap_lookup(d, (cap_handle)handle, CAP_WRITE);
    if (!o) return SYS_DENIED;

    u8 *data = (u8 *)obj_data(o);
    if (!data || offset + 8 > obj_size(o)) return SYS_DENIED;

    for (u32 i = 0; i < 8; i++) data[offset + i] = (u8)(value >> (i * 8));

    /* A change the shell never saw still deserves to survive the next
     * boot; the touch is what tells persistence to look. */
    obj_touch(o);
    return SYS_OK;
}

/* Passing a held capability onward, inside a message.
 *
 * This is delegation from below: not the shell handing something to a
 * program, but one program handing something to another. The checks
 * are not here -- they are in port_send, on the same path every other
 * capability transfer takes. The sender must hold the right to grant
 * what it passes, and what arrives is the intersection of what it held
 * with what it offered. A program can only ever give away less than it
 * has, exactly like everyone else. */
static u64 do_pass(domain *d, u64 port, u64 tag, u64 cap, u64 mask, u64 w0)
{
    message m = { 0 };
    m.tag = tag;
    m.nwords = 1;
    m.words[0] = w0;
    m.ncaps = 1;
    m.caps[0] = (cap_handle)cap;
    m.cap_mask[0] = (u32)mask;

    if (!port_send(d, (cap_handle)port, &m)) return SYS_DENIED;
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
        return do_receive(d, a0, a1, a2);

    case SYS_READ:
        return do_read(d, a0, a1);

    case SYS_WRITE:
        return do_write(d, a0, a1, a2);

    case SYS_PASS:
        return do_pass(d, a0, a1, a2, a3, a4);

    case SYS_CLOCK: {
        /* The one call that needs no capability. The time of day is a
         * fact about the world, not about any object; answering it
         * grants nothing and names nothing. The settings may shift it
         * -- the hardware clock keeps its own time, and where the
         * machine actually stands is the person's to say. */
        u32 hh, mm, ss;
        time_wall(&hh, &mm, &ss);
        i64 s = (i64)hh * 3600 + (i64)mm * 60 + ss +
                settings_clock_offset_min() * 60;
        s = ((s % 86400) + 86400) % 86400;
        return (u64)s;
    }

    default:
        return SYS_BADCALL;
    }
}
