/*
 * apic.c -- the local interrupt controller: enabled for message-signalled interrupts, legacy lines untouched.
 * - xapic: memory window from IA32_APIC_BASE; x2apic: the same registers as msrs, when the firmware chose that
 * - lint0 kept as the 8259's way in (ExtINT), so the timer and the keyboard keep arriving as before
 * - no timer, no ipi: one processor, and the pit is the clock
 */
#include <eb/apic.h>
#include <eb/vmm.h>
#include <eb/mm.h>
#include <eb/io.h>
#include <eb/fmt.h>

#define MSR_APIC_BASE 0x1Bu
#define BASE_X2APIC   (1u << 10)
#define BASE_ENABLE   (1u << 11)

#define REG_ID     0x020
#define REG_TPR    0x080
#define REG_EOI    0x0B0
#define REG_SVR    0x0F0
#define REG_LINT0  0x350
#define REG_LINT1  0x360

#define SVR_ENABLE  (1u << 8)
#define SPURIOUS    0xFFu
#define LVT_EXTINT  0x700u
#define LVT_NMI     0x400u

static bool present, x2;
static volatile u32 *regs;
static u32 id;

static u32 rd(u32 reg)
{
    if (x2) return (u32)rdmsr(0x800u + (reg >> 4));
    return regs[reg / 4];
}

static void wr(u32 reg, u32 v)
{
    if (x2) wrmsr(0x800u + (reg >> 4), v);
    else regs[reg / 4] = v;
}

bool lapic_init(void)
{
    u32 a, b, c, d;
    __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    if (!((d >> 9) & 1)) return false;

    u64 base = rdmsr(MSR_APIC_BASE);
    x2 = (base & BASE_X2APIC) != 0;
    if (!(base & BASE_ENABLE)) {
        base |= BASE_ENABLE;
        wrmsr(MSR_APIC_BASE, base);
    }
    if (!x2) {
        phys_addr pa = base & 0xFFFFFF000ULL;
        if (!vmm_map(vmm_kernel_pml4(), (virt_addr)phys_to_virt(pa), pa, PAGE_SIZE, PAGE_KERNEL_MMIO))
            return false;
        regs = (volatile u32 *)phys_to_virt(pa);
    }

    id = x2 ? rd(REG_ID) : (rd(REG_ID) >> 24);

    /* Everything accepted, the controller on, and the legacy pair's
     * line kept open: the timer arrives through it, and a machine whose
     * timer stops here would never say why. */
    wr(REG_TPR, 0);
    wr(REG_SVR, (rd(REG_SVR) & ~0xFFu) | SVR_ENABLE | SPURIOUS);
    wr(REG_LINT0, LVT_EXTINT);
    wr(REG_LINT1, LVT_NMI);
    present = true;
    kprintf("apic: local controller %u on, %s; message-signalled interrupts possible\n",
            id, x2 ? "x2apic" : "xapic");
    return true;
}

bool lapic_present(void) { return present; }
u32  lapic_id(void)      { return id; }

void lapic_eoi(void)
{
    if (present) wr(REG_EOI, 0);
}

/* Fixed delivery, physical destination, edge: the message a device
 * sends is a write of the data word to this address. */
u32 lapic_msi_address(void)  { return 0xFEE00000u | ((id & 0xFFu) << 12); }
u32 lapic_msi_data(u8 vector) { return vector; }
