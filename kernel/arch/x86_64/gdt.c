/*
 * gdt.c -- global descriptor table and task state segment.
 * - descriptors written as finished numbers
 * - the TSS holds the emergency stacks
 */
#include <eb/gdt.h>
#include <eb/percpu.h>
#include <eb/fmt.h>

#define MAX_CPUS 8

/* 0x00AF9A000000FFFF -- executable, ring 0, L bit set
 * 0x00CF92000000FFFF -- writable,   ring 0
 * 0x00CFF2000000FFFF -- writable,   ring 3
 * 0x00AFFA000000FFFF -- executable, ring 3, L bit set */
#define D_KERNEL_CODE 0x00AF9A000000FFFFULL
#define D_KERNEL_DATA 0x00CF92000000FFFFULL
#define D_USER_DATA   0x00CFF2000000FFFFULL
#define D_USER_CODE   0x00AFFA000000FFFFULL

/* The task state segment. Field order and padding are dictated by the
 * hardware; the struct has to come out at exactly 104 bytes. */
typedef struct __attribute__((packed)) {
    u32 reserved0;
    u64 rsp[3];        /* stacks for ring 0, 1, 2 */
    u64 reserved1;
    u64 ist[7];        /* emergency stacks 1 through 7 */
    u64 reserved2;
    u16 reserved3;
    u16 iomap_base;
} tss64;

_Static_assert(sizeof(tss64) == 104, "TSS must be 104 bytes");

typedef struct __attribute__((packed)) {
    u16 limit;
    u64 base;
} table_ptr;

/* Five ordinary descriptors, then one task-segment descriptor per
 * processor -- each is twice as wide in long mode, two slots. */
static u64   gdt[5 + 2 * MAX_CPUS];
static tss64 tss[MAX_CPUS];

/* Separate stacks for the exceptions where the regular stack can no
 * longer be trusted, one set per processor so two cores faulting at once
 * do not land on the same emergency stack. 16 KiB each. */
static u8 stack_double_fault[MAX_CPUS][16 * 1024] __attribute__((aligned(16)));
static u8 stack_nmi[MAX_CPUS][16 * 1024]          __attribute__((aligned(16)));
static u8 stack_debug[MAX_CPUS][16 * 1024]        __attribute__((aligned(16)));

/* The task-segment selector for a processor: after the five ordinary
 * descriptors, two slots each. cpu 0's is SEL_TSS. */
static inline u16 tss_selector(u32 cpu) { return (u16)((5 + cpu * 2) * 8); }

/* Writes a processor's task segment and its descriptor in the shared
 * table: the three emergency stacks, an empty I/O bitmap, and a type-9
 * available 64-bit TSS descriptor at that processor's two slots. */
static void build_tss(u32 cpu)
{
    tss[cpu].ist[IST_DOUBLE_FAULT - 1] =
        (u64)stack_double_fault[cpu] + sizeof(stack_double_fault[cpu]);
    tss[cpu].ist[IST_NMI - 1]   = (u64)stack_nmi[cpu]   + sizeof(stack_nmi[cpu]);
    tss[cpu].ist[IST_DEBUG - 1] = (u64)stack_debug[cpu] + sizeof(stack_debug[cpu]);

    /* Pointing the I/O permission bitmap past the end of the segment
     * makes it count as empty: user code will not be able to touch a
     * single port. Which is exactly what we want. */
    tss[cpu].iomap_base = sizeof(tss64);

    u64 base  = (u64)&tss[cpu];
    u64 limit = sizeof(tss64) - 1;
    u32 d = 5 + cpu * 2;

    gdt[d] = (limit & 0xFFFFULL)
           | ((base & 0xFFFFFFULL) << 16)
           | (0x89ULL << 40)
           | (((limit >> 16) & 0xFULL) << 48)
           | (((base >> 24) & 0xFFULL) << 56);
    gdt[d + 1] = (base >> 32) & 0xFFFFFFFFULL;
}

void tss_set_kernel_stack(u64 rsp0)
{
    tss[this_cpu_id()].rsp[0] = rsp0;
}

void gdt_setup_ap(u32 cpu)
{
    if (cpu >= MAX_CPUS) return;
    build_tss(cpu);
    __asm__ volatile ("ltr %0" :: "r" (tss_selector(cpu)));
}

void gdt_init(void)
{
    gdt[0] = 0;
    gdt[1] = D_KERNEL_CODE;
    gdt[2] = D_KERNEL_DATA;
    gdt[3] = D_USER_DATA;
    gdt[4] = D_USER_CODE;

    build_tss(0);

    table_ptr gdtr = { .limit = sizeof(gdt) - 1, .base = (u64)gdt };

    /* After loading, CS has to be reloaded, and the only way to do that
     * is a jump carrying the new selector -- here a far return to a
     * target we push ourselves. */
    __asm__ volatile (
        "lgdt   %[gdtr]                 \n"
        "movw   %[data], %%ax           \n"
        "movw   %%ax, %%ds              \n"
        "movw   %%ax, %%es              \n"
        "movw   %%ax, %%ss              \n"
        "movw   %%ax, %%fs              \n"
        "movw   %%ax, %%gs              \n"
        "pushq  %[code]                 \n"
        "leaq   1f(%%rip), %%rax        \n"
        "pushq  %%rax                   \n"
        "lretq                          \n"
        "1:                             \n"
        :
        : [gdtr] "m" (gdtr),
          [data] "i" ((u16)SEL_KERNEL_DATA),
          [code] "i" ((u64)SEL_KERNEL_CODE)
        : "rax", "memory");

    __asm__ volatile ("ltr %0" :: "r" ((u16)SEL_TSS));

    kprintf("cpu0: GDT loaded, TSS with three %u KiB fault stacks\n",
            (u32)(sizeof(stack_double_fault[0]) / 1024));
}

/* An application processor loading the shared descriptor table and its
 * kernel selectors. The task register comes later, in gdt_setup_ap, once
 * the processor has a task segment of its own to load. */
void gdt_load_ap(void)
{
    table_ptr gdtr = { .limit = sizeof(gdt) - 1, .base = (u64)gdt };
    __asm__ volatile (
        "lgdt   %[gdtr]                 \n"
        "movw   %[data], %%ax           \n"
        "movw   %%ax, %%ds              \n"
        "movw   %%ax, %%es              \n"
        "movw   %%ax, %%ss              \n"
        "movw   %%ax, %%fs              \n"
        "movw   %%ax, %%gs              \n"
        "pushq  %[code]                 \n"
        "leaq   1f(%%rip), %%rax        \n"
        "pushq  %%rax                   \n"
        "lretq                          \n"
        "1:                             \n"
        :
        : [gdtr] "m" (gdtr),
          [data] "i" ((u16)SEL_KERNEL_DATA),
          [code] "i" ((u64)SEL_KERNEL_CODE)
        : "rax", "memory");
}
