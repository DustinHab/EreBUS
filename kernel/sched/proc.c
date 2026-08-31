/*
 * proc.c -- processes, address spaces, and the checked copies across
 * the ring boundary.
 */
#include <eb/proc.h>
#include <eb/syscall.h>
#include <eb/vmm.h>
#include <eb/pmm.h>
#include <eb/kheap.h>
#include <eb/mm.h>
#include <eb/fmt.h>
#include <eb/io.h>
#include <eb/panic.h>

/* The user section of the kernel image, from the linker script. */
extern char __user_start[];
extern char __user_end[];

#define PROC_MAGIC 0x50524F4300ULL   /* "PROC" */

struct process {
    u64         magic;
    u64         id;
    const char *name;
    domain     *dom;
    phys_addr   pml4;
    virt_addr   entry;    /* where in its own space the program starts */
    thread     *first;
};

extern void user_enter(u64 entry, u64 stack, u64 first_argument);

static u64 next_pid = 1;
static u64 process_count;
static u64 fault_count;

/* ------------------------------------------------------------------ */
/* Address spaces                                                      */
/* ------------------------------------------------------------------ */

/* A fresh address space: the kernel's upper half, shared, and an empty
 * lower half.
 *
 * Shared by copying the top level entries, which works only because
 * vmm_init filled every one of them in advance. The kernel half is
 * therefore the same tables in every process -- one kernel, seen
 * identically from everywhere, and unreachable from ring 3 because none
 * of those entries carries the user bit. */
static phys_addr addrspace_create(void)
{
    phys_addr root = pmm_alloc();
    if (root == PMM_NO_FRAME) return 0;

    u64 *dst = (u64 *)phys_to_virt(root);
    const u64 *src = (const u64 *)phys_to_virt(vmm_kernel_pml4());

    for (u32 i = 0; i < 256; i++) dst[i] = 0;          /* the program's */
    for (u32 i = 256; i < 512; i++) dst[i] = src[i];   /* the kernel's  */

    return root;
}

/* ------------------------------------------------------------------ */
/* Processes                                                           */
/* ------------------------------------------------------------------ */

process *proc_create(const char *name, const void *entry_point)
{
    process *p = (process *)kzalloc(sizeof(process));
    if (!p) return NULL;

    p->pml4 = addrspace_create();
    if (!p->pml4) { kfree(p); return NULL; }

    p->dom = domain_create(name, 64);
    if (!p->dom) { kfree(p); return NULL; }

    p->magic = PROC_MAGIC;
    p->id = next_pid++;
    p->name = name;

    /* Every program is mapped the same way: the whole user section at
     * one fixed address, and the entry point wherever inside it this
     * particular program happens to begin. Sharing the mapping costs
     * nothing -- the pages are read-only -- and it keeps the layout
     * identical from one process to the next. */
    p->entry = USER_CODE_BASE +
               ((const u8 *)entry_point - (const u8 *)__user_start);

    /* The program: readable and executable, never writable. The pages
     * are the same physical ones the kernel image occupies, mapped a
     * second time with the user bit set. Two views, two sets of
     * permissions, one copy of the bytes. */
    u64 size = PAGE_UP((u64)(__user_end - __user_start));
    for (u64 off = 0; off < size; off += PAGE_SIZE) {
        phys_addr phys = kernel_virt_to_phys(__user_start + off);
        if (!vmm_map(p->pml4, USER_CODE_BASE + off, phys, PAGE_SIZE,
                     PTE_PRESENT | PTE_USER)) {
            kfree(p);
            return NULL;
        }
    }

    /* The stack: writable, and emphatically not executable. This is
     * where anything a program is fed ends up, so it is the first place
     * an attacker would like to put code. */
    for (u64 off = 0; off < USER_STACK_SIZE; off += PAGE_SIZE) {
        phys_addr frame = pmm_alloc();
        if (frame == PMM_NO_FRAME) return NULL;
        virt_addr at = USER_STACK_TOP - USER_STACK_SIZE + off;
        if (!vmm_map(p->pml4, at, frame, PAGE_SIZE,
                     PTE_PRESENT | PTE_WRITE | PTE_USER | PTE_NX)) {
            pmm_free(frame);
            return NULL;
        }
    }

    process_count++;
    return p;
}

domain     *proc_domain(process *p)       { return p ? p->dom : NULL; }
const char *proc_name(const process *p)   { return p ? p->name : "?"; }
u64         proc_id(const process *p)     { return p ? p->id : 0; }
phys_addr   proc_pml4(const process *p)   { return p ? p->pml4 : 0; }
u64         proc_count(void)              { return process_count; }
u64         proc_faults(void)             { return fault_count; }

/* The kernel-side landing pad for a new process: still in ring 0, on
 * the thread's own kernel stack, with the address space already
 * switched. All that is left is to stop being the kernel. */
static void user_launch(void *arg)
{
    process *p = (process *)arg;

    /* Whatever the process starts holding is handle number one in its
     * own table: slot 1, first generation. It is told that and nothing
     * else -- no environment, no arguments, no inherited anything. */
    user_enter(p->entry, USER_STACK_TOP - 16, (u64)1 | ((u64)1 << 32));
}

bool proc_start(process *p)
{
    if (!p || p->magic != PROC_MAGIC) return false;

    p->first = thread_create(p->name, user_launch, p, p->dom);
    if (!p->first) return false;

    thread_set_pml4(p->first, p->pml4);
    return true;
}

/* ------------------------------------------------------------------ */
/* Reaching into a program's memory                                    */
/* ------------------------------------------------------------------ */

static inline void smap_open(void)  { __asm__ volatile ("stac" ::: "cc"); }
static inline void smap_close(void) { __asm__ volatile ("clac" ::: "cc"); }

/* Is this whole range memory the program actually owns?
 *
 * Checked page by page against the live tables, and specifically for
 * the user bit. Without that last part the check would pass for kernel
 * addresses too, and a program that talked the kernel into copying
 * to or from one of those would have exactly the hole SMAP exists to
 * close. */
static bool range_is_user(virt_addr addr, u64 len, bool need_write)
{
    if (len == 0) return true;
    if (addr >= EB_PHYSMAP_BASE) return false;      /* not the lower half */
    if (addr + len < addr) return false;            /* wrapped */

    virt_addr first = PAGE_DOWN(addr);
    virt_addr last  = PAGE_DOWN(addr + len - 1);

    for (virt_addr page = first; page <= last; page += PAGE_SIZE) {
        u64 flags;
        if (!vmm_resolve(page, NULL, &flags)) return false;
        if (!(flags & PTE_USER)) return false;
        if (need_write && !(flags & PTE_WRITE)) return false;
    }
    return true;
}

bool copy_from_user(void *dst, virt_addr src, u64 len)
{
    if (!range_is_user(src, len, false)) return false;

    bool smap = vmm_active_protections().smap;
    if (smap) smap_open();
    const u8 *s = (const u8 *)src;
    u8 *d = (u8 *)dst;
    for (u64 i = 0; i < len; i++) d[i] = s[i];
    if (smap) smap_close();
    return true;
}

bool copy_to_user(virt_addr dst, const void *src, u64 len)
{
    if (!range_is_user(dst, len, true)) return false;

    bool smap = vmm_active_protections().smap;
    if (smap) smap_open();
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    for (u64 i = 0; i < len; i++) d[i] = s[i];
    if (smap) smap_close();
    return true;
}

/* ------------------------------------------------------------------ */

void proc_fault(const char *what, virt_addr where)
{
    thread *t = sched_current();
    fault_count++;

    kprintf("proc: thread %llu (%s) faulted in ring 3: %s at %p\n",
            thread_id(t), thread_name(t), what, (void *)where);
    kprintf("proc: ending that thread; the rest of the system continues\n");

    /* This is the difference the address space buys. A program reaching
     * where it should not is one program's problem, reported and over
     * with -- not a kernel panic, and not a machine that has to be
     * restarted to find out what happened. */
    thread_exit();
}
