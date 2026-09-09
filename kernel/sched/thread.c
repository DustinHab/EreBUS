/*
 * thread.c -- threads and the round-robin scheduler.
 * - each kernel stack in its own address-space slot with an unmapped guard page below; an overrun is a page fault reported from the double fault stack
 */
#include <eb/thread.h>
#include <eb/vmm.h>
#include <eb/pmm.h>
#include <eb/kheap.h>
#include <eb/mm.h>
#include <eb/fmt.h>
#include <eb/io.h>
#include <eb/gdt.h>
#include <eb/syscall.h>
#include <eb/percpu.h>
#include <eb/spin.h>
#include <eb/time.h>
#include <eb/panic.h>
#include <eb/string.h>

/* Thread stacks live in their own window, well clear of the heap. Each
 * slot holds one stack at the top of its window with an unmapped guard
 * page just below it; the window is sized for the largest stack a thread
 * may ask for, but only the pages a stack actually uses are mapped, so a
 * default-sized stack costs nothing extra for the room reserved above it.
 * Most threads take TSTACK_SIZE; a few whose work recurses deeply -- the
 * compiler's background build thread -- ask for more, up to TSTACK_MAX. */
#define TSTACK_BASE   0xFFFFFE0000000000ULL
#define TSTACK_SIZE   (16 * 1024)                 /* the default */
#define TSTACK_MAX    (128 * 1024)                /* the largest on offer */
#define TSTACK_STRIDE (TSTACK_MAX + PAGE_SIZE)    /* window per slot: max stack plus guard */
#define TSTACK_SLOTS  1024                        /* threads at once */

/* The mapped stack is filled with this byte at creation; the run leaves
 * its own marks, so the lowest address still holding the sentinel is the
 * high-water mark -- how close the thread came to its guard page. */
#define STACK_FILL_BYTE 0xA5u
#define STACK_SENTINEL  0xA5A5A5A5u

#define THREAD_MAGIC 0x54485245414400ULL   /* "THREAD" */

/* Slice length in timer ticks. At 100 Hz that is 50 ms, which is long
 * enough that switching costs nothing measurable and short enough that
 * a thread stuck in a loop does not hold the machine. */
#define SLICE_TICKS 5      /* the default; the settings may change it */

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_FINISHED
} thread_state;

struct thread {
    u64          magic;
    u64          rsp;          /* where switch_stack left it */
    u64          id;
    const char  *name;
    domain      *dom;
    thread_state state;
    bool         condemned;    /* marked to end at its next kernel step */
    bool         may_roam;     /* false: only the boot processor runs it */
    u32          slot;         /* which stack slot is ours */
    u32          stack_size;   /* mapped stack, in bytes */
    virt_addr    stack_low;    /* first mapped byte */
    thread_entry entry;
    void        *arg;
    phys_addr    pml4;         /* zero means the kernel's own space */
    u64          kstack_top;   /* where an interrupt from ring 3 lands */
    u64          ran_ns;       /* how long it has actually held the cpu */

    /* A ring-3 thread's vector registers, kept across switches: the
     * 512-byte fxsave image, 16-aligned inside a slightly larger
     * allocation. Kernel threads have none; the kernel never touches
     * those registers. */
    u8          *fx_raw;
    u8          *fx;

    /* Called after the thread is truly gone -- off the run queue, off
     * its stack -- by whoever reaps it. This is where a process hangs
     * its own teardown, which cannot run any earlier: the exiting
     * thread still stands on the stack and still runs in the address
     * space that the teardown is going to free. */
    void       (*on_reap)(void *);
    void        *on_reap_arg;

    struct thread *next;       /* run queue, circular */
    struct thread *prev;
    struct thread *wait_next;  /* whatever wait list holds us */

    /* Asleep until this moment (0: not sleeping), on the sleepers list. */
    u64            wake_at;
    struct thread *sleep_next;
};

extern void switch_stack(u64 *save_rsp, u64 load_rsp);
extern char stack_top_symbol[] __asm__("stack_top");

/* Lets a thread run on any processor. Kernel threads default to the boot
 * processor only, because they are the ones that reach the device drivers
 * and those are not built for more than one core touching them. A user
 * process reaches hardware only by sending a message to one of those
 * threads, so it is free to run anywhere. */
void thread_set_roam(thread *t, bool roam)
{
    if (!t || t->magic != THREAD_MAGIC) return;
    t->may_roam = roam;
}

void thread_set_pml4(thread *t, phys_addr pml4)
{
    if (!t || t->magic != THREAD_MAGIC) return;
    t->pml4 = pml4;

    /* A program gets a clean vector state to start from: everything
     * zero, the control words at their defaults -- all exceptions
     * masked, as a program that never asked for them expects. */
    if (pml4 && !t->fx) {
        t->fx_raw = (u8 *)kzalloc(512 + 16);
        if (t->fx_raw) {
            t->fx = (u8 *)(((u64)t->fx_raw + 15) & ~15ULL);
            t->fx[0] = 0x7F; t->fx[1] = 0x03;          /* fcw */
            t->fx[24] = 0x80; t->fx[25] = 0x1F;        /* mxcsr */
        }
    }
}

static thread *run_queue;      /* circular; ready and running threads */
static thread *boot_thread;    /* the boot thread, adopted from start.S */
static thread *bsp_idle;       /* the boot processor's idle; its time is idle time */

/* One lock over the run queue, the sleepers and finished lists, the
 * counters, and the act of switching itself. It is held with interrupts
 * off, and -- the one unusual part -- it is held across a context switch:
 * whoever the processor lands on releases it, the thread being switched
 * away from having handed it over. On one processor it is never
 * contended; it is what makes the same code correct on several. */
static spinlock sched_lock;

/* The running thread and the moment it took the processor are per cpu,
 * kept in that processor's block; the accounting reads the clock at each
 * handover and books the interval to whoever is leaving. */
static inline thread *cur(void)          { return this_cpu()->current; }
static inline void    set_cur(thread *t) { this_cpu()->current = t; }

static u32 slice_ticks = SLICE_TICKS;

static void reap_finished(void);   /* defined beside thread_exit below */
static u64 next_id = 1;
static u64 switches;
static u64 thread_count;
static u64 runnable_count;
static volatile bool started;   /* the timer may tick before sched_init runs */
static u8 slot_taken[TSTACK_SLOTS / 8];

/* ------------------------------------------------------------------ */
/* Run queue                                                           */
/* ------------------------------------------------------------------ */

static void queue_add(thread *t)
{
    if (!run_queue) {
        t->next = t->prev = t;
        run_queue = t;
    } else {
        t->next = run_queue;
        t->prev = run_queue->prev;
        run_queue->prev->next = t;
        run_queue->prev = t;
    }
    runnable_count++;
}

static void queue_remove(thread *t)
{
    if (t->next == t) {
        run_queue = NULL;
    } else {
        t->prev->next = t->next;
        t->next->prev = t->prev;
        if (run_queue == t) run_queue = t->next;
    }
    t->next = t->prev = NULL;
    runnable_count--;
}

/* ------------------------------------------------------------------ */
/* Stacks                                                              */
/* ------------------------------------------------------------------ */

static bool claim_slot(u32 *out)
{
    for (u32 i = 0; i < TSTACK_SLOTS; i++) {
        if (slot_taken[i >> 3] & (1u << (i & 7))) continue;
        slot_taken[i >> 3] |= (u8)(1u << (i & 7));
        *out = i;
        return true;
    }
    return false;
}

static void release_slot(u32 slot)
{
    slot_taken[slot >> 3] &= (u8)~(1u << (slot & 7));
}

/* Maps a stack of the given size at the top of its slot's window, leaving
 * the page just below the mapped region unmapped as the guard. Returns the
 * first mapped byte, or 0 if a frame could not be had -- unwinding what it
 * mapped so far, so a failure leaks nothing. */
static virt_addr map_stack(u32 slot, u32 size)
{
    virt_addr top = TSTACK_BASE + (u64)slot * TSTACK_STRIDE + TSTACK_STRIDE;
    virt_addr low = top - size;

    for (u64 off = 0; off < size; off += PAGE_SIZE) {
        phys_addr frame = pmm_alloc();
        if (frame != PMM_NO_FRAME &&
            vmm_map(vmm_kernel_pml4(), low + off, frame, PAGE_SIZE,
                    PAGE_KERNEL_DATA)) {
            continue;
        }
        if (frame != PMM_NO_FRAME) pmm_free(frame);
        for (u64 u = 0; u < off; u += PAGE_SIZE) {
            phys_addr f;
            if (vmm_unmap_page(vmm_kernel_pml4(), low + u, &f)) pmm_free(f);
        }
        return 0;
    }
    return low;
}

/* ------------------------------------------------------------------ */

static void trampoline(void)
{
    /* The switch that first ran us was holding the scheduler lock, and a
     * thread resumed through switch_stack is the one that releases it.
     * Every other thread does that on its way out of switch_to_next; a
     * brand new one has never been there, so it does it here. */
    spin_unlock(&sched_lock);

    thread *t = cur();
    cpu_sti();                 /* the first thing a new thread wants */
    t->entry(t->arg);
    thread_exit();
}

/* The domain the boot thread runs in, kept so that each application
 * processor's idle thread can be created in the same one. */
static domain *sched_kernel_domain;

/* The idle thread's body: halt until an interrupt, then look for work.
 * There is one per processor, off the run queue, and it is the only place
 * a processor halts. A thread that blocks switches here rather than
 * stopping in place, which is what keeps a woken thread from being
 * resumed on its old processor and picked up by another at the same
 * time. */
void sched_idle_run(void)
{
    for (;;) {
        __asm__ volatile ("sti; hlt");
        sched_yield();
    }
}
static void idle_entry(void *arg) { (void)arg; sched_idle_run(); }

/* Makes a dedicated idle thread and takes it off the ready ring, so the
 * scheduler reaches it only as the fallback when nothing else runs. */
static thread *make_idle(const char *name, domain *d)
{
    thread *t = thread_create(name, idle_entry, NULL, d);
    if (!t) return NULL;
    u64 flags = spin_lock_irq(&sched_lock);
    queue_remove(t);
    spin_unlock_irq(&sched_lock, flags);
    return t;
}

void sched_init(domain *boot_domain)
{
    sched_kernel_domain = boot_domain;

    thread *t = (thread *)kzalloc(sizeof(thread));
    if (!t) panic("no memory for the boot thread");

    t->magic = THREAD_MAGIC;
    t->id    = next_id++;
    t->name  = "boot";
    t->dom   = boot_domain;
    t->state = THREAD_RUNNING;
    t->slot  = 0xFFFFFFFFu;    /* runs on the stack from start.S */
    t->kstack_top = (u64)stack_top_symbol;

    set_cur(t);
    this_cpu()->switch_stamp = time_ns();
    this_cpu()->slice_left = slice_ticks;
    boot_thread = t;
    thread_count = 1;
    queue_add(t);

    /* The boot processor's own idle thread, separate from the boot
     * thread: the boot thread does real work and must never be the thing
     * a blocking thread switches away to. */
    thread *idle = make_idle("idle", boot_domain);
    if (!idle) panic("no memory for the boot processor's idle thread");
    this_cpu()->idle = idle;
    bsp_idle = idle;

    started = true;      /* from here the timer tick may schedule */
}

/* An application processor adopts the execution it is already running as
 * its own idle thread, the way sched_init does for the boot processor.
 * That idle thread is deliberately not on the run queue: the scheduler
 * falls back to this processor's idle only when it has nothing else to
 * run, so no other processor ever picks it up. Interrupts are off; called
 * once, as the processor joins the scheduler, its gs base already set. */
void sched_adopt_ap(u64 kstack_top)
{
    thread *t = (thread *)kzalloc(sizeof(thread));
    if (!t) panic("no memory for an application processor's idle thread");

    t->magic = THREAD_MAGIC;
    t->name  = "idle";
    t->dom   = sched_kernel_domain;
    t->state = THREAD_RUNNING;
    t->slot  = 0xFFFFFFFFu;        /* runs on the trampoline stack */
    t->kstack_top = kstack_top;

    u64 flags = spin_lock_irq(&sched_lock);
    t->id = next_id++;
    thread_count++;
    spin_unlock_irq(&sched_lock, flags);

    set_cur(t);
    this_cpu()->idle = t;
    this_cpu()->switch_stamp = time_ns();
    this_cpu()->slice_left = slice_ticks;
}

static thread *create(const char *name, thread_entry entry, void *arg,
                      domain *d, u32 size)
{
    if (size < TSTACK_SIZE) size = TSTACK_SIZE;
    if (size > TSTACK_MAX)  size = TSTACK_MAX;
    size = (u32)((size + PAGE_SIZE - 1) & ~(u64)(PAGE_SIZE - 1));

    thread *t = (thread *)kzalloc(sizeof(thread));
    if (!t) return NULL;

    /* The slot map and the id counter are shared; claiming a slot and
     * taking an id are quick and go under the lock. Mapping the stack
     * afterwards is not: it asks the frame allocator and the page tables
     * for memory, each with its own lock, so it runs outside this one. */
    u64 flags = spin_lock_irq(&sched_lock);
    bool got = claim_slot(&t->slot);
    if (got) t->id = next_id++;
    spin_unlock_irq(&sched_lock, flags);
    if (!got) { kfree(t); return NULL; }

    virt_addr low = map_stack(t->slot, size);
    if (!low) {
        flags = spin_lock_irq(&sched_lock);
        release_slot(t->slot);
        spin_unlock_irq(&sched_lock, flags);
        kfree(t);
        return NULL;
    }

    /* Sentinel-fill for the high-water mark, then lay the first frame in
     * over the top of it. */
    memset((void *)low, STACK_FILL_BYTE, size);

    t->magic = THREAD_MAGIC;
    t->name  = name;
    t->dom   = d;
    t->entry = entry;
    t->arg   = arg;
    t->state = THREAD_READY;
    t->stack_size = size;
    t->stack_low = low;
    t->kstack_top = low + size;

    /* Lay out the stack so that switch_stack's epilogue walks off it
     * straight into the trampoline: flags first, then the six
     * callee-saved registers, then the address it will return to. */
    u64 *sp = (u64 *)(low + size);
    *--sp = (u64)trampoline;   /* ret target */
    *--sp = 0;                 /* rbp */
    *--sp = 0;                 /* rbx */
    *--sp = 0;                 /* r12 */
    *--sp = 0;                 /* r13 */
    *--sp = 0;                 /* r14 */
    *--sp = 0;                 /* r15 */
    *--sp = 0x002;             /* rflags, interrupts still off */
    t->rsp = (u64)sp;

    flags = spin_lock_irq(&sched_lock);
    queue_add(t);
    thread_count++;
    spin_unlock_irq(&sched_lock, flags);

    return t;
}

thread *thread_create(const char *name, thread_entry entry, void *arg,
                      domain *d)
{
    return create(name, entry, arg, d, TSTACK_SIZE);
}

thread *thread_create_stack(const char *name, thread_entry entry, void *arg,
                            domain *d, u32 stack_bytes)
{
    return create(name, entry, arg, d, stack_bytes);
}

/* The deepest a thread's stack has been used, in bytes: the mapped region
 * is sentinel-filled at creation, so the lowest word that no longer holds
 * the sentinel is as far as the stack ever grew. An approximation -- a run
 * that happened to leave the sentinel value on the stack reads as untouched
 * there -- but close enough to watch the guard from. */
u64 thread_stack_highwater(const thread *t)
{
    if (!t || t->slot == 0xFFFFFFFFu || !t->stack_low) return 0;
    const u32 *p = (const u32 *)t->stack_low;
    u64 words = t->stack_size / 4;
    u64 i = 0;
    while (i < words && p[i] == STACK_SENTINEL) i++;
    return t->stack_size - i * 4;
}

u64 thread_stack_size(const thread *t) { return t ? t->stack_size : 0; }

/* Finished threads, waiting to be reaped. Declared here because a
 * thread condemned but never reaching a syscall is retired straight
 * from switch_to_next, onto this same list. */
static thread *finished_list;

/* Whether the boot thread has finished starting up and only halts now. */
static bool boot_idle;

void sched_idle_from_here(void) { boot_idle = true; }

/* Picks the next thread this processor should run and switches to it.
 *
 * Called with sched_lock held and interrupts off. It does not release the
 * lock: the lock crosses the switch. When switch_stack lands the
 * processor on another thread, that thread is somewhere in its own
 * switch_to_next (or, brand new, in trampoline) and releases the lock
 * there. Execution returns here only when some processor switches back to
 * us, still holding the lock, and the caller lets it go. */
static void switch_to_next(void)
{
    thread *from = cur();
    percpu *me = this_cpu();

    /* Walk the ring for a thread that is ready and not already running on
     * another processor. Starting after the current one keeps it round
     * robin. The idle thread is passed over while any real work waits,
     * and taken only when nothing else is ready. */
    thread *to = NULL;
    if (run_queue) {
        thread *start = (from->state == THREAD_RUNNING && from->next)
                        ? from->next : run_queue;
        thread *p = start;
        do {
            /* A thread that may not roam runs only on the boot processor,
             * which is where the device interrupts land: its drivers keep
             * their single-processor habits and stay correct. */
            if (p->state == THREAD_READY &&
                !(boot_idle && p == boot_thread) &&
                (p->may_roam || me->index == 0)) { to = p; break; }
            p = p->next;
        } while (p != start);

        /* Nothing else was ready: keep running the current thread if it
         * still can, otherwise fall to this processor's own idle. */
        if (!to) {
            if (from->state == THREAD_RUNNING && !from->condemned) to = from;
            else to = this_cpu()->idle;
        }
    }
    if (!to) to = this_cpu()->idle;   /* empty run queue: idle */

    if (!to || to == from) {
        me->slice_left = slice_ticks;
        return;                        /* nothing to switch to; lock stays held */
    }

    /* Book the interval to whoever is leaving. */
    u64 now = time_ns();
    from->ran_ns += now - me->switch_stamp;
    me->switch_stamp = now;

    if (from->state == THREAD_RUNNING) {
        if (from->condemned) {
            /* Condemned but still running: it never reached a syscall to
             * end itself -- a compute-bound ring-3 job with no system
             * calls, for instance. End it here, on preemption, so it
             * cannot outlive its deadline. This is the same retirement a
             * voluntary exit takes; the kernel stack it is standing on is
             * freed later by reap_finished, in a calm context. */
            from->state = THREAD_FINISHED;
            queue_remove(from);
            thread_count--;
            from->wait_next = finished_list;
            finished_list = from;
        } else {
            from->state = THREAD_READY;
        }
    }
    to->state = THREAD_RUNNING;
    set_cur(to);
    switches++;
    me->slice_left = slice_ticks;
    me->resched = 0;

    /* Two things have to follow the thread, not the code: the address
     * space it runs in, and where the processor should land if an
     * interrupt arrives while it is in ring 3. Both are per thread, and
     * both are wrong the instant a switch forgets them.
     *
     * The address space is reloaded on every switch, even between two
     * kernel threads that share the kernel's tables. Writing cr3 flushes
     * this processor's translations, and that flush is what lets several
     * processors share one address space safely without sending each
     * other messages to invalidate it: a thread's stack is freed only
     * after it stops running, by which point every processor that ran it
     * has switched away and flushed. It costs a flush per switch; the
     * alternative is a round of inter-processor interrupts per unmap. */
    u64 target = to->pml4 ? to->pml4 : vmm_kernel_pml4();
    __asm__ volatile ("movq %0, %%cr3" :: "r"(target) : "memory");

    tss_set_kernel_stack(to->kstack_top);
    percpu_set_kernel_stack(to->kstack_top);

    /* The vector registers follow the program too. Saved from the one
     * leaving while it still owns them, restored for the one arriving
     * before it runs; the kernel between them never uses the unit, so
     * doing both here, eagerly, is correct and simple. */
    if (from->fx) __asm__ volatile ("fxsave (%0)" :: "r"(from->fx) : "memory");
    if (to->fx)   __asm__ volatile ("fxrstor (%0)" :: "r"(to->fx) : "memory");

    switch_stack(&from->rsp, to->rsp);
    /* Execution resumes here when somebody switches back to us, with the
     * lock held; our caller releases it. */
}

/* The scheduler lock, lent to code that has to block a thread on a wait
 * list of its own (a port with nobody sending yet, say). */
u64  sched_lock_hold(void)        { return spin_lock_irq(&sched_lock); }
void sched_lock_drop(u64 flags)   { spin_unlock_irq(&sched_lock, flags); }

void sched_yield(void)
{
    /* Yielding is the one moment every thread reliably passes through
     * in a calm state, which makes it the right place to sweep up after
     * the ones that have finished. */
    reap_finished();

    u64 flags = spin_lock_irq(&sched_lock);
    switch_to_next();
    spin_unlock_irq(&sched_lock, flags);
}

/* Called with sched_lock held and interrupts off, having already put the
 * caller on whatever wait list it waits on, so a wakeup cannot slip in
 * between. Returns, still holding the lock, once the thread is running
 * again. */
void sched_block(void)
{
    thread *me = cur();
    me->state = THREAD_BLOCKED;
    queue_remove(me);

    /* Switch away -- to another ready thread, or to this processor's idle
     * thread, which is where the processor halts. It never halts in place
     * here: a thread that halted in place would still be this processor's
     * current thread while a wakeup put it back on the ready ring, and
     * another processor could then run it from its stale saved context at
     * the same time. Going through switch_to_next means a woken thread is
     * only ever claimed once, under the lock. */
    switch_to_next();

    /* Resumed: a wakeup marked us ready and some processor picked us up. */
    me->state = THREAD_RUNNING;
}

/* ------------------------------------------------------------------ */
/* Sleeping and events                                                 */
/* ------------------------------------------------------------------ */

/* Threads with a deadline, unordered: there are a handful at most, and
 * the tick walks them all. */
static thread *sleepers;

static void wake_locked(thread *t);   /* defined with sched_wake below */

/* Interrupts off. */
static void sleepers_add(thread *t, u64 wake_at)
{
    t->wake_at = wake_at;
    t->sleep_next = sleepers;
    sleepers = t;
}

/* Interrupts off. Absent is fine: whoever woke the thread first may
 * have taken it off already. */
static void sleepers_remove(thread *t)
{
    thread **p = &sleepers;
    while (*p) {
        if (*p == t) { *p = t->sleep_next; break; }
        p = &(*p)->sleep_next;
    }
    t->sleep_next = NULL;
    t->wake_at = 0;
}

/* Called from the tick with interrupts off: wakes what is due. */
static void sleepers_tick(void)
{
    if (!sleepers) return;
    u64 now = time_ns();
    thread **p = &sleepers;
    while (*p) {
        thread *t = *p;
        if (t->wake_at && t->wake_at <= now) {
            *p = t->sleep_next;
            t->sleep_next = NULL;
            t->wake_at = 0;
            wake_locked(t);
        } else {
            p = &t->sleep_next;
        }
    }
}

void sched_sleep_ns(u64 ns)
{
    reap_finished();
    u64 flags = spin_lock_irq(&sched_lock);
    sleepers_add(cur(), time_ns() + ns);
    sched_block();
    sleepers_remove(cur());
    spin_unlock_irq(&sched_lock, flags);
}

void event_signal(event *e)
{
    if (!e) return;
    u64 flags = spin_lock_irq(&sched_lock);
    e->pending = 1;
    thread *woken[EVENT_WAITERS];
    u32 n = e->nwaiters;
    for (u32 i = 0; i < n; i++) woken[i] = e->waiters[i];
    e->nwaiters = 0;
    for (u32 i = 0; i < n; i++) wake_locked(woken[i]);
    spin_unlock_irq(&sched_lock, flags);
}

/* Interrupts off. */
static void event_forget(event *e, thread *t)
{
    for (u32 i = 0; i < e->nwaiters; i++) {
        if (e->waiters[i] != t) continue;
        for (u32 k = i + 1; k < e->nwaiters; k++) e->waiters[k - 1] = e->waiters[k];
        e->nwaiters--;
        return;
    }
}

bool event_wait(event *e, u64 timeout_ns)
{
    if (!e) { sched_sleep_ns(timeout_ns); return false; }
    reap_finished();

    u64 flags = spin_lock_irq(&sched_lock);
    if (e->pending) {
        e->pending = 0;
        spin_unlock_irq(&sched_lock, flags);
        return true;
    }
    if (e->nwaiters >= EVENT_WAITERS) {
        /* More waiters than the event has room for: not a state the
         * kernel gets into on purpose, so the wait becomes a sleep. */
        spin_unlock_irq(&sched_lock, flags);
        sched_sleep_ns(timeout_ns);
        return false;
    }
    e->waiters[e->nwaiters++] = cur();
    if (timeout_ns) sleepers_add(cur(), time_ns() + timeout_ns);
    sched_block();

    /* Awake: by the signal, by the deadline, or to be ended. Off both
     * lists either way -- whichever woke us left the other one. */
    sleepers_remove(cur());
    event_forget(e, cur());
    bool signalled = e->pending != 0;
    e->pending = 0;
    spin_unlock_irq(&sched_lock, flags);
    return signalled;
}

/* Puts a blocked thread back on the run queue. Assumes sched_lock is held
 * and interrupts are off. */
static void wake_locked(thread *t)
{
    if (!t || t->magic != THREAD_MAGIC) return;
    if (t->state != THREAD_BLOCKED) return;
    t->state = THREAD_READY;
    queue_add(t);
    /* If this processor is idle, it owes itself a reschedule so the woken
     * thread gets a processor on the way out. Idle processors elsewhere
     * pick it up at their next tick. */
    if (cur() == this_cpu()->idle) this_cpu()->resched = 1;
}

void sched_wake(thread *t)
{
    u64 flags = spin_lock_irq(&sched_lock);
    wake_locked(t);
    spin_unlock_irq(&sched_lock, flags);
}

/* Marks a thread to end. It finishes itself at its next step into
 * the kernel -- the check at the syscall door -- so the ending always
 * runs in the thread's own context, on its own stack, through the
 * same exit and reaping a voluntary end takes. A blocked thread is
 * woken so that step comes; the wait it was in answers false. */
void thread_condemn(thread *t)
{
    if (!t || t->magic != THREAD_MAGIC) return;
    t->condemned = true;
    sched_wake(t);
}

bool thread_condemned(const thread *t)
{
    return t && t->magic == THREAD_MAGIC && t->condemned;
}

void sched_tick(void)
{
    if (!started) return;               /* the timer runs before we do */
    u64 flags = spin_lock_irq(&sched_lock);
    sleepers_tick();
    percpu *me = this_cpu();
    if (me->slice_left > 0) me->slice_left--;
    if (me->slice_left == 0) me->resched = 1;
    spin_unlock_irq(&sched_lock, flags);
}

void sched_preempt_if_due(void)
{
    if (!started || !this_cpu()->resched) return;
    u64 flags = spin_lock_irq(&sched_lock);
    if (this_cpu()->resched) {
        this_cpu()->resched = 0;
        switch_to_next();
    }
    spin_unlock_irq(&sched_lock, flags);
}

/* The exiting thread puts itself on finished_list (declared above) with
 * interrupts off and switches away in the same breath, so nothing can
 * free the stack it is still standing on. */
void thread_exit(void)
{
    u64 flags = spin_lock_irq(&sched_lock);

    thread *t = cur();
    t->state = THREAD_FINISHED;
    queue_remove(t);
    thread_count--;

    t->wait_next = finished_list;
    finished_list = t;

    /* The stack cannot be released here: we are standing on it. The next
     * thread that passes through sched_yield picks it up. switch_to_next
     * hands the lock across to whoever runs next and never returns to a
     * finished thread. */
    switch_to_next();

    spin_unlock_irq(&sched_lock, flags);
    panic("a finished thread was scheduled again");
}

void thread_on_reap(thread *t, void (*fn)(void *), void *arg)
{
    if (!t || t->magic != THREAD_MAGIC) return;
    t->on_reap = fn;
    t->on_reap_arg = arg;
}

/* Frees what a finished thread could not free itself: its kernel stack,
 * its struct, and -- through the hook -- whatever the thread was the
 * last living part of. Runs in an ordinary thread context, never from
 * an interrupt: the teardown allocates and frees, and doing that on top
 * of somebody's half-finished allocation is how heaps die. */
static void reap_finished(void)
{
    for (;;) {
        u64 flags = spin_lock_irq(&sched_lock);
        thread *t = finished_list;
        if (t) finished_list = t->wait_next;
        spin_unlock_irq(&sched_lock, flags);
        if (!t) return;

        if (t->on_reap) t->on_reap(t->on_reap_arg);

        if (t->slot != 0xFFFFFFFFu) {
            for (u64 off = 0; off < t->stack_size; off += PAGE_SIZE) {
                phys_addr frame;
                if (vmm_unmap_page(vmm_kernel_pml4(),
                                   t->stack_low + off, &frame))
                    pmm_free(frame);
            }
            flags = spin_lock_irq(&sched_lock);
            release_slot(t->slot);
            spin_unlock_irq(&sched_lock, flags);
        }

        if (t->fx_raw) kfree(t->fx_raw);
        t->magic = 0;
        kfree(t);
    }
}

thread     *sched_current(void)              { return cur(); }
domain     *thread_domain(const thread *t)   { return t ? t->dom : NULL; }
const char *thread_name(const thread *t)     { return t ? t->name : "?"; }
u64         thread_id(const thread *t)       { return t ? t->id : 0; }
u64         sched_switches(void)             { return switches; }
u64         thread_ran_ns(const thread *t)   { return t ? t->ran_ns : 0; }
u64         sched_idle_ns(void)              { return bsp_idle ? bsp_idle->ran_ns : 0; }

void sched_set_slice_ticks(u32 t)
{
    if (t < 1)   t = 1;
    if (t > 100) t = 100;
    slice_ticks = t;
}
u64         sched_threads(void)              { return thread_count; }
u64         sched_runnable(void)             { return runnable_count; }

/* ------------------------------------------------------------------ */
/* Self test                                                           */
/* ------------------------------------------------------------------ */

static volatile u64 tally[3];
static volatile u64 order_marks;
static volatile bool spinner_should_stop;
static volatile u64 spinner_laps;

static void counter_thread(void *arg)
{
    u64 which = (u64)arg;
    for (u32 i = 0; i < 40; i++) {
        tally[which]++;
        /* One bit per visit, so the pattern shows whether the three
         * really interleaved or just ran one after another. */
        order_marks = (order_marks << 2) | which;
        sched_yield();
    }
}

/* Never yields. If it still gets interrupted, preemption works. */
static void spinner_thread(void *arg)
{
    (void)arg;
    while (!spinner_should_stop) {
        spinner_laps++;
        __asm__ volatile ("pause");
    }
}

bool sched_selftest(void)
{
    domain *d = thread_domain(cur());

    tally[0] = tally[1] = tally[2] = 0;
    order_marks = 0;

    for (u64 i = 0; i < 3; i++)
        if (!thread_create("counter", counter_thread, (void *)i, d))
            return false;

    /* Yield until they are all done, but not forever. */
    for (u32 spin = 0; spin < 100000; spin++) {
        if (tally[0] >= 40 && tally[1] >= 40 && tally[2] >= 40) break;
        sched_yield();
    }

    if (tally[0] < 40 || tally[1] < 40 || tally[2] < 40) {
        kprintf("sched: threads did not all finish (%llu %llu %llu)\n",
                tally[0], tally[1], tally[2]);
        return false;
    }

    /* Three different thread numbers in the last six visits means they
     * were genuinely taking turns, not running to completion one after
     * another. */
    bool seen[3] = { false, false, false };
    u64 marks = order_marks;
    for (u32 i = 0; i < 6; i++) { seen[marks & 3] = true; marks >>= 2; }
    if (!seen[0] || !seen[1] || !seen[2]) {
        kprintf("sched: threads ran in sequence, not interleaved\n");
        return false;
    }

    /* Preemption: a thread that never yields must still be taken off
     * the processor, or the whole machine is at the mercy of one loop. */
    spinner_should_stop = false;
    spinner_laps = 0;
    if (!thread_create("spinner", spinner_thread, NULL, d)) return false;

    u64 before = sched_switches();
    for (u32 i = 0; i < 20; i++) sched_yield();

    /* Burn wall time without yielding, so only the timer can take the
     * processor away from us and hand it to the spinner. */
    for (volatile u64 i = 0; i < 20000000ULL; i++) { }

    spinner_should_stop = true;
    sched_yield();

    if (spinner_laps == 0) {
        kprintf("sched: the spinning thread never ran\n");
        return false;
    }
    if (sched_switches() <= before) {
        kprintf("sched: no context switches happened\n");
        return false;
    }
    return true;
}
