/*
 * proc.c -- processes, address spaces, and the checked copies across
 * the ring boundary.
 */
#include <eb/proc.h>
#include <eb/msg.h>
#include <eb/syscall.h>
#include <eb/vmm.h>
#include <eb/pmm.h>
#include <eb/kheap.h>
#include <eb/mm.h>
#include <eb/fmt.h>
#include <eb/io.h>
#include <eb/journal.h>
#include <eb/time.h>
#include <eb/panic.h>

/* The user section of the kernel image, from the linker script. */
extern char __user_start[];
extern char __user_end[];

#define PROC_MAGIC 0x50524F4300ULL   /* "PROC" */

struct process {
    u64         magic;
    u64         id;
    u64         stamp;    /* this boot, this process -- see proc_is_running */
    const char *name;
    domain     *dom;
    phys_addr   pml4;
    virt_addr   entry;    /* where in its own space the program starts */
    thread     *first;

    /* How the rest of the system reaches it. The program object is the
     * process as it appears in the graph: point that object at
     * something, and the program is handed it. */
    object     *self;
    object     *inbox;
    cap_handle  console_cap, inbox_cap;
};

extern void user_enter(u64 entry, u64 stack, u64 arg0, u64 arg1);

static u64 next_pid = 1;

/* Which processes are actually running.
 *
 * A program object can outlive the program: it is part of the graph and
 * the graph is written to disk, so a snapshot from an earlier boot
 * brings back an object whose payload points at a process that no
 * longer exists. Following that pointer would be reading freed memory
 * on the strength of something read off a disk, which is precisely the
 * kind of trust this system is built to avoid. The pointer is checked
 * against this list before it is ever used. */
#define MAX_PROCESSES 32
static process *live[MAX_PROCESSES];
static u32      live_count;

static bool is_live(const process *p)
{
    for (u32 i = 0; i < live_count; i++) if (live[i] == p) return true;
    return false;
}

/* The payload of a program object: the pointer, and a stamp that ties
 * it to one process in one boot. The pointer alone once decided this,
 * and a restored object's stale pointer happened to land on the reborn
 * process's freshly allocated struct -- same allocation order, same
 * address -- making a record from the last boot pass for alive by
 * coincidence. Identity that rests on where the allocator put things
 * is not identity. */
typedef struct {
    process *p;
    u64      stamp;
} program_ref;

bool proc_is_running(object *program)
{
    if (!program || obj_type(program) != TYPE_PROGRAM) return false;
    if (obj_size(program) < sizeof(program_ref)) return false;

    const program_ref *ref = (const program_ref *)obj_data(program);
    return is_live(ref->p) && ref->p->stamp == ref->stamp;
}
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

/* Whether a frame belongs to the shared user code -- the kernel image's
 * own pages, mapped a second time into every process. Freeing one of
 * those on a process's way out would free a piece of the running
 * kernel, for every process that ever exits. */
static bool frame_is_shared_code(phys_addr frame)
{
    phys_addr lo = kernel_virt_to_phys(__user_start);
    phys_addr hi = kernel_virt_to_phys(__user_end);
    return frame >= PAGE_DOWN(lo) && frame < PAGE_UP(hi);
}

/* Frees the lower half of an address space: the program's own frames,
 * then the table pages that mapped them, then the top table itself.
 *
 * Only the lower 256 entries are walked. The upper half points at the
 * kernel's shared tables -- the same physical pages in every process --
 * and following those down would dismantle the kernel's own mapping.
 * Never called for a space that is still loaded anywhere. */
static void free_table_level(phys_addr table, u32 level, u64 top_limit)
{
    u64 *e = (u64 *)phys_to_virt(table);
    for (u64 i = 0; i < top_limit; i++) {
        if (!(e[i] & PTE_PRESENT)) continue;
        phys_addr down = e[i] & 0x000FFFFFFFFFF000ULL;

        if (level > 1)
            free_table_level(down, level - 1, 512);
        else if (!frame_is_shared_code(down))
            pmm_free(down);
    }
    pmm_free(table);
}

static void addrspace_destroy(phys_addr pml4)
{
    if (pml4) free_table_level(pml4, 4, 256);
}

/* ------------------------------------------------------------------ */
/* Processes                                                           */
/* ------------------------------------------------------------------ */

process *proc_create(const char *name, const void *entry_point,
                     object *console)
{
    /* A full table refuses at the door. Registering nowhere and
     * running anyway would make a process the liveness check cannot
     * see -- running and invisible is the one combination this system
     * must never produce. */
    if (live_count >= MAX_PROCESSES) return NULL;

    process *p = (process *)kzalloc(sizeof(process));
    if (!p) return NULL;

    p->pml4 = addrspace_create();
    if (!p->pml4) { kfree(p); return NULL; }

    p->dom = domain_create(name, 64);
    if (!p->dom) { addrspace_destroy(p->pml4); kfree(p); return NULL; }

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
                     PTE_PRESENT | PTE_USER))
            goto fail;
    }

    /* The stack: writable, and emphatically not executable. This is
     * where anything a program is fed ends up, so it is the first place
     * an attacker would like to put code. */
    for (u64 off = 0; off < USER_STACK_SIZE; off += PAGE_SIZE) {
        phys_addr frame = pmm_alloc();
        if (frame == PMM_NO_FRAME) goto fail;
        virt_addr at = USER_STACK_TOP - USER_STACK_SIZE + off;
        if (!vmm_map(p->pml4, at, frame, PAGE_SIZE,
                     PTE_PRESENT | PTE_WRITE | PTE_USER | PTE_NX)) {
            pmm_free(frame);
            goto fail;
        }
    }

    /* What it starts holding: permission to speak to the console, and
     * its own letter box. Two capabilities, and nothing else in the
     * world. */
    p->inbox = port_create(8);
    if (!p->inbox) goto fail;

    p->console_cap = console ? cap_insert(p->dom, console, CAP_CALL) : 0;
    p->inbox_cap = cap_insert(p->dom, p->inbox, CAP_READ);

    /* And the program as it appears in the graph. Pointing this object
     * at something is how the program comes to hold it -- the same
     * gesture as pointing anything at anything, with the difference
     * that this particular holder is alive. */
    p->stamp = time_boot_stamp() ^ (p->id << 40) ^ p->id;

    p->self = obj_create(TYPE_PROGRAM, sizeof(program_ref), 8);
    if (!p->self) goto fail;
    program_ref *ref = (program_ref *)obj_data(p->self);
    ref->p = p;
    ref->stamp = p->stamp;
    obj_set_name(p->self, name);

    if (live_count < MAX_PROCESSES) live[live_count++] = p;
    process_count++;
    return p;

fail:
    /* A half-built process must not leave half its parts behind. The
     * same teardown the reaper uses, minus what was never made. */
    if (p->inbox) obj_release(p->inbox);
    domain_destroy(p->dom);
    addrspace_destroy(p->pml4);
    kfree(p);
    return NULL;
}

object *proc_object(process *p) { return p ? p->self : NULL; }

u32 proc_live_count(void)
{
    return live_count;
}

bool proc_live_at(u32 i, const char **name, u64 *id, u64 *holds,
                  u64 *ran_ns)
{
    /* One row of the living, for the activity table. Kernel-side only,
     * like the capability inspection: no system call leads here, and
     * what it reveals -- that programs exist -- the graph shows anyway. */
    u64 flags = irq_save();
    if (i >= live_count) { irq_restore(flags); return false; }

    process *p = live[i];
    if (name)   *name = p->name;
    if (id)     *id = p->id;
    if (holds)  *holds = domain_used(p->dom);
    if (ran_ns) *ran_ns = p->first ? thread_ran_ns(p->first) : 0;
    irq_restore(flags);
    return true;
}

domain *proc_domain_of(object *program)
{
    /* The domain behind a program object, for the shell's inspection.
     * Only for a program that is actually running; a restored record
     * has no domain, and its stale pointer is never followed. */
    if (!proc_is_running(program)) return NULL;
    return ((program_ref *)obj_data(program))->p->dom;
}

/* Hands a running program a reference.
 *
 * It arrives as a message, which is the only way anything arrives here.
 * The program was not holding this a moment ago and has no way to have
 * asked for it; somebody with the authority chose to pass it on, at
 * these rights and no more. */
/* What handing `what` to a program actually delivers.
 *
 * For most objects: the object, at the rights the giver chose. For a
 * running program: its letter box, send-only. Pointing one program at
 * another cannot mean handing over the other's insides -- a program is
 * not a thing to be read, it is a party to be spoken to, and the letter
 * box is the only door it has. A program that has ended is a record,
 * not a recipient, and handing someone a record of a conversation that
 * can no longer happen is handing them nothing. */
static bool grant_translate(object **what, u32 *rights)
{
    if (obj_type(*what) != TYPE_PROGRAM) return true;

    /* Introducing one program to another is passing the second one on,
     * and passing on is what the grant right is. Holding a program
     * without it means holding a thing to look at, not a contact to
     * hand around. */
    if (!(*rights & CAP_GRANT)) return false;

    if (!proc_is_running(*what)) return false;
    process *q = ((program_ref *)obj_data(*what))->p;
    if (!q->inbox) return false;

    *what = q->inbox;
    *rights = CAP_CALL;
    return true;
}

bool proc_grant(object *program, object *what, u32 rights)
{
    if (!program || obj_type(program) != TYPE_PROGRAM || !what) return false;

    if (!proc_is_running(program)) return false;
    process *p = ((program_ref *)obj_data(program))->p;
    if (!p->inbox) return false;

    if (!grant_translate(&what, &rights)) return false;

    /* Whatever it held for this object before stops counting.
     *
     * Without this, narrowing a reference would not narrow anything: the
     * program would simply end up holding both the old capability and
     * the new one, and would keep using whichever was wider. Rights that
     * can only ever be added are not rights, they are a record of every
     * mistake anyone has made. */
    cap_revoke_object(p->dom, what);

    message m = { 0 };
    m.tag = 0x4556494721ULL;      /* "GIVE!" */
    m.nwords = 1;
    m.words[0] = rights;

    return port_post(p->inbox, &m, &what, &rights, 1, "you");
}

bool proc_revoke(object *program, object *what)
{
    if (!program || obj_type(program) != TYPE_PROGRAM || !what) return false;

    if (!proc_is_running(program)) return false;
    process *p = ((program_ref *)obj_data(program))->p;
    if (!p->inbox) return false;

    /* Withdrawing needs no right that giving needed: taking back what
     * one handed out is always allowed, so the translation runs with
     * the check pre-satisfied. */
    u32 any = CAP_GRANT;
    if (!grant_translate(&what, &any)) return false;

    if (cap_revoke_object(p->dom, what) == 0) return false;

    /* A message carrying nothing, which is the point: it says only that
     * something has changed. The program is free to ignore it, and if it
     * does go looking, what it finds is that the handle it kept names
     * nothing at all. */
    message m = { 0 };
    m.tag = 0x4E49414741ULL;      /* "AGAIN" */
    return port_post(p->inbox, &m, NULL, NULL, 0, "you");
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

    /* The two things it starts holding, and nothing else. No
     * environment, no arguments, no inherited anything -- a program
     * begins with exactly what it was handed. */
    user_enter(p->entry, USER_STACK_TOP - 16,
               (u64)p->console_cap, (u64)p->inbox_cap);
}

/* Everything a process was, given back.
 *
 * Runs from the reaper, after the process's thread has finished and
 * left both its stack and this address space. Order matters twice: the
 * program object's payload is cleared before anything else goes, so a
 * stale pointer read through the graph finds nothing rather than freed
 * memory; and the domain goes before the inbox, so the inbox's last
 * reference is dropped after the capabilities into it are gone. */
static void proc_reap(void *arg)
{
    process *p = (process *)arg;
    if (!p || p->magic != PROC_MAGIC) return;

    u64 flags = irq_save();
    for (u32 i = 0; i < live_count; i++) {
        if (live[i] != p) continue;
        live[i] = live[live_count - 1];
        live_count--;
        break;
    }
    irq_restore(flags);

    /* The program object stays as long as the graph points at it -- it
     * is the record that a program was here. What must not stay is the
     * pointer to a process that no longer exists. */
    if (p->self) {
        program_ref *gone = (program_ref *)obj_data(p->self);
        gone->p = NULL;
        gone->stamp = 0;
        obj_release(p->self);
    }

    /* The domain releases every capability the program held: console,
     * inbox, and anything it was granted along the way. Then the
     * inbox's own reference goes, and if nothing else holds the port it
     * dies -- letting go of any undelivered mail with it. */
    domain_destroy(p->dom);
    if (p->inbox) obj_release(p->inbox);

    addrspace_destroy(p->pml4);

    kprintf("proc: %llu (%s) ended; everything it held has been let go\n",
            p->id, p->name);
    journal_says(p->name, "ended; everything it held has been let go");

    p->magic = 0;
    kfree(p);
}

bool proc_start(process *p)
{
    if (!p || p->magic != PROC_MAGIC) return false;

    p->first = thread_create(p->name, user_launch, p, p->dom);
    if (!p->first) return false;

    thread_set_pml4(p->first, p->pml4);
    thread_on_reap(p->first, proc_reap, p);
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
    journal_says(thread_name(t), "reached where it may not; it was ended");

    /* This is the difference the address space buys. A program reaching
     * where it should not is one program's problem, reported and over
     * with -- not a kernel panic, and not a machine that has to be
     * restarted to find out what happened. */
    thread_exit();
}
