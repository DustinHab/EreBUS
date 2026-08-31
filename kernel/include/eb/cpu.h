#ifndef EB_CPU_H
#define EB_CPU_H

#include <eb/types.h>

/* What CPUID says about the processor. Not merely for display: whether
 * NX, SMEP and SMAP exist decides which protections we can switch on
 * when the page tables go up. */
typedef struct {
    char vendor[13];       /* "GenuineIntel", "AuthenticAMD", ...       */
    char brand[49];        /* marketing name, if the CPU carries one    */

    u32  family, model, stepping;
    u8   phys_bits;        /* usable physical address bits              */
    u8   virt_bits;        /* usable virtual address bits               */

    /* For the memory manager */
    bool pae;              /* physical address extension                */
    bool pge;              /* global pages                              */
    bool pse1g;            /* 1 GiB pages                               */
    bool nx;               /* pages can be marked non-executable        */

    /* For hardening */
    bool smep;             /* kernel cannot execute user code           */
    bool smap;             /* kernel cannot read user data unguarded    */
    bool umip;             /* user code cannot see the system tables    */
    bool rdrand, rdseed;   /* randomness from the hardware              */

    /* Everything else */
    bool apic, x2apic;
    bool tsc, invariant_tsc;
    bool syscall;
    bool hypervisor;       /* are we running inside a virtual machine?  */
} cpu_info;

void cpu_detect(cpu_info *out);

#endif /* EB_CPU_H */
