/*
 * thread.c -- threads and the round-robin scheduler.
 *
 * Stacks get a guard page.
 *
 * A kernel stack taken from the heap has heap blocks either side of it,
 * so running off the end quietly rewrites somebody else's allocation
 * and the damage surfaces somewhere unrelated, much later. Here each
 * stack sits in its own slot of address space with an unmapped page
 * below it. Running off the end is then a page fault at the exact
 * instruction that did it -- and one that lands on the double fault
 * stack from the TSS, so it gets reported rather than triple faulting.
 * The cost is one page of address space per thread, of which there is
 * an unlimited supply.
 */
#include <eb/thread.h>
#include <eb/vmm.h>
#include <eb/pmm.h>
#include <eb/kheap.h>
#include <eb/mm.h>
#include <eb/fmt.h>
#include <eb/io.h>
#include <eb/panic.h>

/* Thread stacks live in their own window, well clear of the heap. */
#define TSTACK_BASE   0xFFFFFE0000000000ULL
#define TSTACK_SIZE   (16 * 1024)
#define TSTACK_STRIDE (TSTACK_SIZE + PAGE_SIZE)   /* stack plus guard */
#define TSTACK_MAX    1024                        /* threads at once */

#define THREAD_MAGIC 0x54485245414400ULL   /* "THREAD" */

/* Slice length in timer ticks. At 100 Hz that is 50 ms, which is long
 * enough that switching costs nothing measurable and short enough that
 * a thread stuck in a loop does not hold the machine. */
#define SLICE_TICKS 5

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
    u32          slot;         /* which stack slot is ours */
    virt_addr    stack_low;    /* first mapped byte */
    thread_entry entry;
    void        *arg;

    struct thread *next;       /* run queue, circular */
    struct thread *prev;
    struct thread *wait_next;  /* whatever wait list holds us */
};

extern void switch_stack(u64 *save_rsp, u64 load_rsp);

static thread *current;
static thread *run_queue;      /* circular; points at some ready thread */
static u64 next_id = 1;
static u64 switches;
static u64 thread_count;
static u64 runnable_count;
static u32 slice_left;
static bool resched_due;
static u8 slot_taken[TSTACK_MAX / 8];

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
    for (u32 i = 0; i < TSTACK_MAX; i++) {
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

/* Maps a stack, leaving the page below it unmapped as the guard. */
static virt_addr map_stack(u32 slot)
{
    virt_addr guard = TSTACK_BASE + (u64)slot * TSTACK_STRIDE;
    virt_addr low   = guard + PAGE_SIZE;

    for (u64 off = 0; off < TSTACK_SIZE; off += PAGE_SIZE) {
        phys_addr frame = pmm_alloc();
        if (frame == PMM_NO_FRAME) return 0;
        if (!vmm_map(vmm_kernel_pml4(), low + off, frame, PAGE_SIZE,
                     PAGE_KERNEL_DATA)) {
            pmm_free(frame);
            return 0;
        }
    }
    return low;
}

/* ------------------------------------------------------------------ */

static void trampoline(void)
{
    thread *t = current;
    cpu_sti();                 /* the first thing a new thread wants */
    t->entry(t->arg);
    thread_exit();
}

void sched_init(domain *boot_domain)
{
    thread *t = (thread *)kzalloc(sizeof(thread));
    if (!t) panic("no memory for the boot thread");

    t->magic = THREAD_MAGIC;
    t->id    = next_id++;
    t->name  = "boot";
    t->dom   = boot_domain;
    t->state = THREAD_RUNNING;
    t->slot  = 0xFFFFFFFFu;    /* runs on the stack from start.S */

    current = t;
    thread_count = 1;
    slice_left = SLICE_TICKS;
    queue_add(t);
}

thread *thread_create(const char *name, thread_entry entry, void *arg,
                      domain *d)
{
    thread *t = (thread *)kzalloc(sizeof(thread));
    if (!t) return NULL;

    if (!claim_slot(&t->slot)) { kfree(t); return NULL; }

    virt_addr low = map_stack(t->slot);
    if (!low) { release_slot(t->slot); kfree(t); return NULL; }

    t->magic = THREAD_MAGIC;
    t->id    = next_id++;
    t->name  = name;
    t->dom   = d;
    t->entry = entry;
    t->arg   = arg;
    t->state = THREAD_READY;
    t->stack_low = low;

    /* Lay out the stack so that switch_stack's epilogue walks off it
     * straight into the trampoline: flags first, then the six
     * callee-saved registers, then the address it will return to. */
    u64 *sp = (u64 *)(low + TSTACK_SIZE);
    *--sp = (u64)trampoline;   /* ret target */
    *--sp = 0;                 /* rbp */
    *--sp = 0;                 /* rbx */
    *--sp = 0;                 /* r12 */
    *--sp = 0;                 /* r13 */
    *--sp = 0;                 /* r14 */
    *--sp = 0;                 /* r15 */
    *--sp = 0x002;             /* rflags, interrupts still off */
    t->rsp = (u64)sp;

    u64 flags = irq_save();
    queue_add(t);
    thread_count++;
    irq_restore(flags);

    return t;
}

/* Moves to the next runnable thread. Interrupts must be off. */
static void switch_to_next(void)
{
    if (!run_queue) return;

    thread *from = current;
    thread *to;

    if (from->state == THREAD_RUNNING && from->next) {
        to = from->next;
    } else {
        to = run_queue;
    }
    if (to == from) { slice_left = SLICE_TICKS; return; }

    if (from->state == THREAD_RUNNING) from->state = THREAD_READY;
    to->state = THREAD_RUNNING;
    current = to;
    switches++;
    slice_left = SLICE_TICKS;
    resched_due = false;

    switch_stack(&from->rsp, to->rsp);
    /* Execution resumes here when somebody switches back to us. */
}

void sched_yield(void)
{
    u64 flags = irq_save();
    switch_to_next();
    irq_restore(flags);
}

void sched_block(void)
{
    /* The caller has interrupts off and has already put us on a wait
     * list, so a wakeup cannot slip between the two. */
    current->state = THREAD_BLOCKED;
    queue_remove(current);
    switch_to_next();
}

void sched_wake(thread *t)
{
    if (!t || t->magic != THREAD_MAGIC) return;
    if (t->state != THREAD_BLOCKED) return;

    u64 flags = irq_save();
    t->state = THREAD_READY;
    queue_add(t);
    irq_restore(flags);
}

void sched_tick(void)
{
    if (!current) return;               /* timer runs before we do */
    if (slice_left > 0) slice_left--;
    if (slice_left == 0) resched_due = true;
}

void sched_preempt_if_due(void)
{
    if (!current || !resched_due) return;
    resched_due = false;
    switch_to_next();
}

void thread_exit(void)
{
    u64 flags = irq_save();

    thread *t = current;
    t->state = THREAD_FINISHED;
    queue_remove(t);
    thread_count--;

    /* The stack cannot be released here: we are standing on it. It is
     * reclaimed by whoever notices the thread has finished. For now
     * that is nobody, and a finished thread keeps its 16 KiB -- honest,
     * and cheap to fix once there is something to reap it. */
    switch_to_next();

    irq_restore(flags);
    panic("a finished thread was scheduled again");
}

thread     *sched_current(void)              { return current; }
domain     *thread_domain(const thread *t)   { return t ? t->dom : NULL; }
const char *thread_name(const thread *t)     { return t ? t->name : "?"; }
u64         thread_id(const thread *t)       { return t ? t->id : 0; }
u64         sched_switches(void)             { return switches; }
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
    domain *d = thread_domain(current);

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
