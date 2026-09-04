/*
 * trap.c -- interrupt descriptor table, dispatch, crash reporting.
 * - the table points every vector at its stub; exceptions and device interrupts are routed apart
 * - an unhandled exception is printed with place and cause instead of a silent reboot
 */
#include <eb/trap.h>
#include <eb/gdt.h>
#include <eb/pic.h>
#include <eb/thread.h>
#include <eb/vmm.h>
#include <eb/proc.h>
#include <eb/fmt.h>
#include <eb/names.h>
#include <eb/io.h>

/* ------------------------------------------------------------------ */
/* The table                                                           */
/* ------------------------------------------------------------------ */

typedef struct __attribute__((packed)) {
    u16 offset_low;
    u16 selector;
    u8  ist;          /* bits 0..2 select an emergency stack, 0 = none */
    u8  type_attr;
    u16 offset_mid;
    u32 offset_high;
    u32 zero;
} idt_entry;

typedef struct __attribute__((packed)) {
    u16 limit;
    u64 base;
} table_ptr;

_Static_assert(sizeof(idt_entry) == 16, "IDT entry must be 16 bytes");

#define GATE_INTERRUPT 0x8E   /* present, ring 0, interrupt gate */

static idt_entry idt[256];

/* Generated in isr.S: 256 slots of exactly 16 bytes each. */
extern u8 isr_stubs[];

static void set_gate(u8 vector, u8 ist, u8 type_attr)
{
    u64 handler = (u64)&isr_stubs[(u32)vector * 16];

    idt[vector].offset_low  = (u16)(handler & 0xFFFF);
    idt[vector].selector    = SEL_KERNEL_CODE;
    idt[vector].ist         = ist & 0x7;
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_mid  = (u16)((handler >> 16) & 0xFFFF);
    idt[vector].offset_high = (u32)(handler >> 32);
    idt[vector].zero        = 0;
}

/* ------------------------------------------------------------------ */
/* Device interrupts                                                   */
/* ------------------------------------------------------------------ */

#define IRQ_BASE 32
#define IRQ_COUNT 16

static irq_handler handlers[IRQ_COUNT];
static u64 irq_total;

void irq_install(u8 irq, irq_handler fn)
{
    if (irq >= IRQ_COUNT) return;
    handlers[irq] = fn;
    pic_set_mask(irq, fn == NULL);
}

u64 trap_irq_count(void) { return irq_total; }

/* ------------------------------------------------------------------ */
/* Crash reporting                                                     */
/* ------------------------------------------------------------------ */

static const char *exception_name(u64 v)
{
    switch (v) {
    case 0:  return "divide error";
    case 1:  return "debug";
    case 2:  return "non-maskable interrupt";
    case 3:  return "breakpoint";
    case 4:  return "overflow";
    case 5:  return "bound range exceeded";
    case 6:  return "invalid opcode";
    case 7:  return "device not available";
    case 8:  return "double fault";
    case 10: return "invalid TSS";
    case 11: return "segment not present";
    case 12: return "stack-segment fault";
    case 13: return "general protection fault";
    case 14: return "page fault";
    case 16: return "x87 floating-point error";
    case 17: return "alignment check";
    case 18: return "machine check";
    case 19: return "SIMD floating-point error";
    case 20: return "virtualisation exception";
    case 21: return "control protection exception";
    default: return "reserved";
    }
}

static void explain_page_fault(u64 err)
{
    const char *cause = (err & 1) ? "page protection violated"
                                  : "page not present";
    const char *op = (err & (1u << 4)) ? "instruction fetch"
                   : (err & (1u << 1)) ? "write"
                                       : "read";

    kprintf("kern: error 0x%04llx: %s, during a %s in %s mode\n",
            err, cause, op, (err & (1u << 2)) ? "user" : "kernel");

    /* A reserved bit set in a page table entry means the tables
     * themselves are damaged, not that a mapping is missing. Worth
     * saying separately -- it points at a different kind of bug. */
    if (err & (1u << 3))
        kprintf("kern: a reserved bit is set in a page table entry\n");
}

static void explain_selector(u64 err)
{
    if (err == 0) {
        kprintf("kern: error 0: no segment selector involved\n");
        return;
    }
    static const char *table[] = { "GDT", "IDT", "LDT", "IDT" };
    kprintf("kern: error 0x%04llx: selector 0x%04llx in the %s%s\n",
            err, (err & 0xFFF8) >> 3, table[(err >> 1) & 3],
            (err & 1) ? ", raised outside the kernel" : "");
}

/* Is this address actually backed by something, so that reading it will
 * not fault? Asking the page tables is the only honest answer, and in a
 * fault handler it is the difference between a report and a reboot. */
static bool readable(u64 addr)
{
    return vmm_resolve(addr, NULL, NULL);
}

/* Walks the saved frame pointer chain. We compile with frame pointers
 * kept, so every frame stores the caller's RBP followed by the return
 * address.
 *
 * Every read here is checked against the page tables first. That is not
 * caution for its own sake: the most valuable time to have a backtrace
 * is after a stack overflow, and that is exactly when the frame pointer
 * points at an unmapped guard page. Dereferencing it would fault inside
 * the fault handler, and a fault inside a double fault handler is a
 * triple fault -- the machine reboots without a word, which is the one
 * outcome this whole file exists to prevent. */
static void backtrace(u64 rbp)
{
    kprintf("kern: call trace:");

    u32 printed = 0;
    for (u32 depth = 0; depth < 12; depth++) {
        /* Only the kernel's own half is walked. A ring-3 fault hands
         * over a user stack, and a mapped user page is still not the
         * kernel's to read under SMAP -- the walker dereferencing it
         * faults inside the fault handler, which is the exact spiral
         * this file exists to prevent. The program's side of the
         * story is proc_fault's to tell, not this trace's. */
        if (rbp < 0xFFFF800000000000ULL || (rbp & 7)) break;
        if (!readable(rbp) || !readable(rbp + 8)) {
            kprintf(" <%p not mapped>", (void *)rbp);
            break;
        }
        const u64 *frame = (const u64 *)rbp;
        u64 ret = frame[1];
        if (ret < (u64)__kernel_start || ret >= (u64)__kernel_end) break;
        u64 off = 0;
        const char *nm = names_of(ret, &off);
        if (nm) kprintf(" %p (%s+0x%llx)", (void *)ret, nm, off);
        else kprintf(" %p", (void *)ret);
        printed++;
        rbp = frame[0];
    }
    if (printed == 0) kprintf(" (nothing readable)");
    kprintf("\n");
}

static void report(trap_frame *f)
{
    kprintf("\n");
    u64 off = 0;
    const char *nm = names_of(f->rip, &off);
    if (nm)
        kprintf("kern: exception %llu (%s) at %p (%s+0x%llx)\n",
                f->vector, exception_name(f->vector), (void *)f->rip, nm, off);
    else
        kprintf("kern: exception %llu (%s) at %p\n",
                f->vector, exception_name(f->vector), (void *)f->rip);

    switch (f->vector) {
    case 14:
        explain_page_fault(f->error);
        kprintf("kern: faulting address %p\n", (void *)read_cr2());
        break;
    case 10: case 11: case 12: case 13:
        explain_selector(f->error);
        break;
    case 8:
        kprintf("kern: a fault occurred while handling another fault\n");
        /* By far the most common cause: the stack ran out, so the
         * processor could not even push the frame for the first fault.
         * Saying so turns a baffling error into an obvious one. */
        if (!readable(f->rsp))
            kprintf("kern: the stack pointer %p is in unmapped memory -- "
                    "this is a stack overflow\n", (void *)f->rsp);
        break;
    default:
        if (f->error) kprintf("kern: error 0x%llx\n", f->error);
        break;
    }

    kprintf("kern: rax %p  rbx %p  rcx %p\n",
            (void *)f->rax, (void *)f->rbx, (void *)f->rcx);
    kprintf("kern: rdx %p  rsi %p  rdi %p\n",
            (void *)f->rdx, (void *)f->rsi, (void *)f->rdi);
    kprintf("kern: rbp %p  rsp %p  r8  %p\n",
            (void *)f->rbp, (void *)f->rsp, (void *)f->r8);
    kprintf("kern: r9  %p  r10 %p  r11 %p\n",
            (void *)f->r9, (void *)f->r10, (void *)f->r11);
    kprintf("kern: r12 %p  r13 %p  r14 %p\n",
            (void *)f->r12, (void *)f->r13, (void *)f->r14);
    kprintf("kern: r15 %p  cr3 %p\n",
            (void *)f->r15, (void *)read_cr3());
    kprintf("kern: cs 0x%04llx  ss 0x%04llx  rflags 0x%08llx  (ring %llu)\n",
            f->cs, f->ss, f->rflags, f->cs & 3);

    /* A protection fault in ring 0 with a selector in its error code is
     * most often an iretq refusing the frame it was handed. That frame
     * lies at the stack pointer; showing it names the culprit. */
    if (f->vector == 13 && (f->cs & 3) == 0 && f->error && readable(f->rsp) &&
        readable(f->rsp + 32)) {
        const u64 *w = (const u64 *)f->rsp;
        kprintf("kern: the frame at rsp: rip %p  cs 0x%04llx  rflags 0x%08llx  rsp %p  ss 0x%04llx\n",
                (void *)w[0], w[1], w[2], (void *)w[3], w[4]);
    }

    backtrace(f->rbp);
}

/* ------------------------------------------------------------------ */
/* Dispatch                                                            */
/* ------------------------------------------------------------------ */

void trap_dispatch(trap_frame *f);   /* called from isr.S */

void trap_dispatch(trap_frame *f)
{
    if (f->vector < 32) {
        /* Where the fault came from decides what it means. A fault in
         * ring 3 is one program overstepping: report it, end that
         * thread, and carry on. The same fault in ring 0 is the kernel
         * being wrong about itself, and there is nothing to carry on
         * with. */
        if ((f->cs & 3) == 3) {
            report(f);
            proc_fault(exception_name(f->vector),
                       f->vector == 14 ? read_cr2() : f->rip);
            return;
        }
        report(f);
        kprintf("kern: system halted\n");
        cpu_stop();
    }

    if (f->vector < IRQ_BASE + IRQ_COUNT) {
        u8 irq = (u8)(f->vector - IRQ_BASE);

        /* Noise on lines 7 and 15 must not be acknowledged as real. */
        if (pic_spurious(irq)) return;

        irq_total++;
        if (handlers[irq]) handlers[irq](f);
        pic_eoi(irq);

        /* The one place a thread switch can happen behind a thread's
         * back. It is safe here and nowhere else in the handler: the
         * interrupted state is already complete on this thread's stack,
         * so leaving now and coming back later resumes it exactly.
         * After the controller has been told, so an interrupt is not
         * left unacknowledged while another thread runs. */
        sched_preempt_if_due();
        return;
    }

    /* Nothing is configured to deliver anything above 47 yet, so this
     * is worth saying out loud rather than swallowing. */
    kprintf("kern: unexpected interrupt %llu, ignored\n", f->vector);
}

/* ------------------------------------------------------------------ */

void trap_init(void)
{
    for (u32 v = 0; v < 256; v++)
        set_gate((u8)v, 0, GATE_INTERRUPT);

    /* The three cases where the ordinary stack cannot be trusted get
     * their own, courtesy of the TSS. */
    set_gate(2,  IST_NMI,          GATE_INTERRUPT);
    set_gate(1,  IST_DEBUG,        GATE_INTERRUPT);
    set_gate(3,  IST_DEBUG,        GATE_INTERRUPT);
    set_gate(8,  IST_DOUBLE_FAULT, GATE_INTERRUPT);
    set_gate(18, IST_DOUBLE_FAULT, GATE_INTERRUPT);  /* machine check */

    table_ptr idtr = { .limit = sizeof(idt) - 1, .base = (u64)idt };
    __asm__ volatile ("lidt %0" :: "m"(idtr));

    kprintf("cpu0: IDT loaded, 256 vectors, %u on separate stacks\n", 5u);
}
