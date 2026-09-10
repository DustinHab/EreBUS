/*
 * smp.c -- bring up and run the application processors.
 *
 * Find the processors in the ACPI MADT and start each through the
 * real-mode trampoline (ap_boot.S) into ap_main, where it takes its own
 * GDT and TSS, enables its local apic, sets up its per-cpu state and
 * syscall entry, and joins the scheduler as that processor's idle thread
 * with its local apic timer running. From there the scheduler runs on
 * every processor -- kernel threads and ring-3 programs alike -- under
 * the locks in spin.h and the thread affinity in sched/thread.c, which
 * keeps device-touching threads on the boot processor. smp_selftest runs
 * workers on the aps and reports how many actually took part.
 */
#include <eb/smp.h>
#include <eb/apic.h>
#include <eb/gdt.h>
#include <eb/trap.h>
#include <eb/vmm.h>
#include <eb/mm.h>
#include <eb/pmm.h>
#include <eb/fmt.h>
#include <eb/spin.h>
#include <eb/time.h>
#include <eb/string.h>
#include <eb/io.h>
#include <eb/thread.h>
#include <eb/percpu.h>
#include <eb/syscall.h>

#define TRAMP_PA 0x8000u        /* real-mode start page (below 1 MiB), reserved at boot */

extern u8 ap_tramp_start[], ap_tramp_end[], ap_tramp_params[];

struct cpu { u32 apic_id; volatile u32 up; u64 stack; };
static struct cpu cpus[MAX_CPUS];
static u32 cpu_count = 1;                 /* the boot processor */
static bool tramp_reserved;               /* the start page is ours, not a live table */

/* The boot processor sets this once start-up is finished and the kernel
 * is safe to run on more than one core; until then the application
 * processors wait, and everything runs on the boot processor alone. */
static volatile u32 smp_go;

u32 smp_cpu_count(void) { return cpu_count; }

void smp_release(void) { __sync_synchronize(); smp_go = 1; }

/* The trampoline runs at a fixed physical address the assembly is built
 * for. Claim that page before anything else can, so the frame allocator
 * does not later hand it out as a page table or a buffer -- writing the
 * trampoline over a live page table was exactly the failure this avoids.
 * Called once, after pmm_init and before the first general allocation. */
void smp_reserve_trampoline(void)
{
    tramp_reserved = pmm_reserve(TRAMP_PA);
}

static void ap_main(u32 cpu);

static void busy_us(u64 us)
{
    u64 t0 = time_ns(), want = us * 1000ULL;
    while (time_ns() - t0 < want) __asm__ volatile ("pause");
}

/* --- ACPI: the MADT, and the local-apic ids in it -------------------- */

static const u8 *find_madt(u64 rsdp_phys)
{
    if (!rsdp_phys) return 0;
    const u8 *rsdp = (const u8 *)phys_to_virt((phys_addr)rsdp_phys);
    u8 rev = rsdp[15];
    u64 sdt_phys; int wide = 0;
    if (rev >= 2) {
        u64 xsdt = *(const u64 *)(rsdp + 24);
        if (xsdt) { sdt_phys = xsdt; wide = 1; }
        else sdt_phys = *(const u32 *)(rsdp + 16);
    } else sdt_phys = *(const u32 *)(rsdp + 16);
    if (!sdt_phys) return 0;

    const u8 *sdt = (const u8 *)phys_to_virt((phys_addr)sdt_phys);
    u32 len = *(const u32 *)(sdt + 4);
    if (len < 36) return 0;
    u32 count = (len - 36) / (wide ? 8 : 4);
    for (u32 i = 0; i < count; i++) {
        u64 tp = wide ? *(const u64 *)(sdt + 36 + i * 8)
                      : (u64)*(const u32 *)(sdt + 36 + i * 4);
        const u8 *t = (const u8 *)phys_to_virt((phys_addr)tp);
        if (t[0] == 'A' && t[1] == 'P' && t[2] == 'I' && t[3] == 'C') return t;
    }
    return 0;
}

/* The processors the MADT lists, by local-apic id, the boot processor
 * left out. Two entry kinds name them: type 0 (a local apic, an 8-bit
 * id) and type 9 (a local x2apic, a 32-bit id), and firmware may list a
 * processor under both, so an id already taken is not taken again. An
 * id above 255 can only be signalled in x2apic mode; when the boot
 * processor's controller is not in that mode such a processor is left
 * parked and named in the log. Entries past MAX_CPUS are counted, not
 * kept, so the log can say how many the machine has beyond the build. */
static u32 read_ap_ids(u64 rsdp, u32 bsp, u32 *ids, u32 max, u32 *beyond)
{
    const u8 *madt = find_madt(rsdp);
    *beyond = 0;
    if (!madt) return 0;
    u32 len = *(const u32 *)(madt + 4);
    u32 off = 44, n = 0;                  /* header 36 + apic address 4 + flags 4 */
    while (off + 2 <= len) {
        u8 type = madt[off], elen = madt[off + 1];
        if (elen < 2 || off + elen > len) break;
        u32 aid = 0, flags = 0;
        bool listed = false;
        if (type == 0 && elen >= 8) {     /* processor local apic */
            aid = madt[off + 3];
            flags = *(const u32 *)(madt + off + 4);
            listed = true;
        } else if (type == 9 && elen >= 16) {   /* processor local x2apic */
            aid = *(const u32 *)(madt + off + 4);
            flags = *(const u32 *)(madt + off + 8);
            listed = true;
        }
        off += elen;
        if (!listed || !(flags & 1) || aid == bsp) continue;   /* bit 0: enabled */
        bool seen = false;
        for (u32 i = 0; i < n; i++) if (ids[i] == aid) seen = true;
        if (seen) continue;
        if (aid > 255 && !lapic_x2apic()) {
            kprintf("smp:  apic id %u needs x2apic mode, which the firmware did not set; left parked\n", aid);
            continue;
        }
        if (n < max) ids[n++] = aid;
        else (*beyond)++;
    }
    return n;
}

/* --- a pml4 that survives the switch to paging ----------------------- */

static phys_addr tramp_pml4(void)
{
    phys_addr t = pmm_alloc();
    if (!t) return 0;
    u64 *tt = (u64 *)phys_to_virt(t);
    memset(tt, 0, PAGE_SIZE);
    const u64 *k = (const u64 *)phys_to_virt(vmm_kernel_pml4());
    for (u32 i = 256; i < 512; i++) tt[i] = k[i];      /* the kernel's high half */
    vmm_map(t, 0, 0, 0x200000, PAGE_KERNEL_CODE);      /* low identity, executable */
    return t;
}

/* --- the boot processor's turn --------------------------------------- */

u32 smp_start(u64 acpi_rsdp)
{
    if (!lapic_present()) { kprintf("smp:  no local apic; one processor\n"); return 1; }
    u32 bsp = lapic_id();
    cpus[0].apic_id = bsp; cpus[0].up = 1;

    u32 ids[MAX_CPUS];
    u32 beyond = 0;
    u32 n = read_ap_ids(acpi_rsdp, bsp, ids, MAX_CPUS - 1, &beyond);
    if (beyond)
        kprintf("smp:  the machine lists %u processors more than this kernel is built for (%u); they stay parked\n",
                beyond, (u32)MAX_CPUS);
    if (n == 0) { kprintf("smp:  1 processor (apic id %u)\n", bsp); return 1; }

    if (!tramp_reserved) {
        kprintf("smp:  start page not reserved; one processor\n");
        return 1;
    }

    phys_addr tp = tramp_pml4();
    if (!tp) { kprintf("smp:  out of memory for the trampoline tables\n"); return 1; }

    memcpy((void *)phys_to_virt(TRAMP_PA), ap_tramp_start, (u64)(ap_tramp_end - ap_tramp_start));
    u8 *params = (u8 *)phys_to_virt(TRAMP_PA) + (u64)(ap_tramp_params - ap_tramp_start);
    *(u64 *)(params + 0)  = (u64)tp;
    *(u64 *)(params + 16) = (u64)(void *)&ap_main;

    for (u32 i = 0; i < n; i++) {
        u32 idx = cpu_count;
        phys_addr st = pmm_alloc_contig(4);            /* a 16 KiB stack for the ap */
        if (!st) break;
        cpus[idx].apic_id = ids[i]; cpus[idx].up = 0;
        cpus[idx].stack = (u64)phys_to_virt(st) + 4 * PAGE_SIZE;
        *(u64 *)(params + 8)  = cpus[idx].stack;
        *(u32 *)(params + 24) = idx;

        lapic_send_init(ids[i]);
        busy_us(10000);
        lapic_send_sipi(ids[i], (u8)(TRAMP_PA >> 12));
        busy_us(200);
        if (!cpus[idx].up) lapic_send_sipi(ids[i], (u8)(TRAMP_PA >> 12));

        u64 t0 = time_ns();
        while (!cpus[idx].up && time_ns() - t0 < 100000000ULL) __asm__ volatile ("pause");
        if (cpus[idx].up) cpu_count++;
        else kprintf("smp:  apic id %u did not answer the start\n", ids[i]);
    }

    kprintf("smp:  %u of %u processors up\n", cpu_count, n + 1);
    return cpu_count;
}

/* --- an application processor's first breath ------------------------- */

static void ap_main(u32 cpu)
{
    /* leave the trampoline's tables for the kernel's own */
    __asm__ volatile ("movq %0, %%cr3" :: "r"((u64)vmm_kernel_pml4()) : "memory");
    gdt_load_ap();
    if (!gdt_setup_ap(cpu)) {       /* this processor's own task segment and fault stacks */
        /* No memory for them: park for good. The boot processor times
         * out waiting for this one and names it in the log. */
        for (;;) __asm__ volatile ("cli; hlt");
    }
    idt_load();
    u32 aid = lapic_enable_ap();
    percpu_init(cpu, aid);          /* this processor's gs base, before it schedules */
    syscall_init();                 /* its own syscall entry, for ring-3 threads */
    cpus[cpu].apic_id = aid;
    __sync_synchronize();
    cpus[cpu].up = 1;
    kprintf("cpu%u: up, apic id %u\n", cpu, aid);

    /* Wait until the boot processor has finished the single-threaded part
     * of start-up and released us into the scheduler. */
    while (!smp_go) __asm__ volatile ("pause");

    /* Join the scheduler for good: adopt this execution as the idle
     * thread, start the local timer for preemption, and run whatever is
     * ready -- kernel threads and user processes alike. */
    sched_adopt_ap(cpus[cpu].stack);
    lapic_timer_start();
    cpu_sti();

    for (;;) {
        __asm__ volatile ("hlt");   /* wait for the next tick, then look for work */
        sched_yield();
    }
}

/* --- proving they run in parallel ------------------------------------ */

static volatile u64  smp_seen;    /* one bit per processor that ran a worker */
static volatile u64  smp_work;    /* iterations the workers completed */
static volatile bool smp_stop;

static void smp_worker(void *arg)
{
    (void)arg;
    while (!smp_stop) {
        __sync_fetch_and_or(&smp_seen, 1ULL << (this_cpu_id() & 63u));
        __sync_fetch_and_add(&smp_work, 1u);
        for (volatile u32 i = 0; i < 3000; i++) { }   /* a little work */
        sched_yield();
    }
}

bool smp_selftest(void)
{
    if (cpu_count < 2) return true;    /* one processor: nothing to prove */

    smp_seen = 0; smp_work = 0; smp_stop = false;

    domain *d = thread_domain(sched_current());
    const u32 workers = 6;
    for (u32 i = 0; i < workers; i++) {
        thread *w = thread_create("smp-worker", smp_worker, NULL, d);
        if (!w) return false;
        thread_set_roam(w, true);   /* the point is to run them on the aps */
    }

    /* Pin the boot thread here with interrupts off while the measurement
     * runs, so it cannot itself migrate and the count reflects only the
     * application processors. They are already released by now. */
    u64 flags = irq_save();

    u64 t0 = time_ns();
    while (smp_work < 200000u && time_ns() - t0 < 400000000ULL)
        __asm__ volatile ("pause");

    smp_stop = true;

    /* Let the workers reach their exit before looking at the tally. */
    u64 t1 = time_ns();
    while (time_ns() - t1 < 40000000ULL) __asm__ volatile ("pause");

    irq_restore(flags);

    u64 seen = smp_seen;
    u32 ran = 0;
    for (u32 i = 0; i < 64; i++) if (seen & (1ULL << i)) ran++;
    kprintf("smp:  kernel work ran on %u application processor%s, %llu iterations\n",
            ran, ran == 1 ? "" : "s", smp_work);

    /* And the user side: which processors have run a ring-3 system call,
     * from the programs that started while the boot went on. */
    u64 umask = syscall_cpu_mask();
    u32 ucount = 0, uaps = 0;
    for (u32 i = 0; i < 64; i++)
        if (umask & (1ULL << i)) { ucount++; if (i != 0) uaps++; }
    kprintf("smp:  ring-3 system calls ran on %u processor%s (%u application)\n",
            ucount, ucount == 1 ? "" : "s", uaps);

    return ran >= 1;
}
