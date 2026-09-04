/*
 * cpu.c -- processor features via CPUID.
 * - leaf 0 and 0x80000000 give the highest supported leaves; unsupported leaves return garbage
 */
#include <eb/cpu.h>

static inline void cpuid(u32 leaf, u32 sub, u32 *a, u32 *b, u32 *c, u32 *d)
{
    __asm__ volatile ("cpuid"
                      : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                      : "a"(leaf), "c"(sub));
}

/* Store four registers back to back as 16 characters. */
static void store4(char *dst, u32 a, u32 b, u32 c, u32 d)
{
    const u32 r[4] = { a, b, c, d };
    for (u32 i = 0; i < 4; i++)
        for (u32 j = 0; j < 4; j++)
            dst[i * 4 + j] = (char)((r[i] >> (j * 8)) & 0xFF);
}

/* Intel pads the marketing name with leading spaces. */
static void trim_left(char *s)
{
    char *p = s;
    while (*p == ' ') p++;
    if (p == s) return;
    while (*p) *s++ = *p++;
    *s = 0;
}

void cpu_detect(cpu_info *o)
{
    u32 a, b, c, d;

    for (u32 i = 0; i < sizeof(cpu_info); i++)
        ((u8 *)o)[i] = 0;

    /* Leaf 0: vendor string and highest standard leaf. */
    cpuid(0, 0, &a, &b, &c, &d);
    u32 max_std = a;
    store4(o->vendor, b, d, c, 0);   /* the order really is EBX, EDX, ECX */
    o->vendor[12] = 0;

    /* Leaf 1: version and the classic feature bits. */
    if (max_std >= 1) {
        cpuid(1, 0, &a, &b, &c, &d);

        u32 base_family = (a >> 8) & 0xF;
        u32 base_model  = (a >> 4) & 0xF;
        o->stepping = a & 0xF;
        o->family   = base_family;
        o->model    = base_model;
        /* Families 15 and 6 extend these fields further up. */
        if (base_family == 0xF)
            o->family += (a >> 20) & 0xFF;
        if (base_family == 0xF || base_family == 0x6)
            o->model += ((a >> 16) & 0xF) << 4;

        o->tsc        = (d >> 4)  & 1;
        o->pae        = (d >> 6)  & 1;
        o->apic       = (d >> 9)  & 1;
        o->pge        = (d >> 13) & 1;
        o->pat        = (d >> 16) & 1;
        o->x2apic     = (c >> 21) & 1;
        o->rdrand     = (c >> 30) & 1;
        o->hypervisor = (c >> 31) & 1;
    }

    /* Leaf 7, subleaf 0: the newer protection features. */
    if (max_std >= 7) {
        cpuid(7, 0, &a, &b, &c, &d);
        o->smep   = (b >> 7)  & 1;
        o->rdseed = (b >> 18) & 1;
        o->smap   = (b >> 20) & 1;
        o->umip   = (c >> 2)  & 1;
    }

    /* Extended leaves. */
    cpuid(0x80000000u, 0, &a, &b, &c, &d);
    u32 max_ext = a;

    if (max_ext >= 0x80000001u) {
        cpuid(0x80000001u, 0, &a, &b, &c, &d);
        o->syscall = (d >> 11) & 1;
        o->nx      = (d >> 20) & 1;
        o->pse1g   = (d >> 26) & 1;
    }

    if (max_ext >= 0x80000004u) {
        cpuid(0x80000002u, 0, &a, &b, &c, &d); store4(o->brand +  0, a, b, c, d);
        cpuid(0x80000003u, 0, &a, &b, &c, &d); store4(o->brand + 16, a, b, c, d);
        cpuid(0x80000004u, 0, &a, &b, &c, &d); store4(o->brand + 32, a, b, c, d);
        o->brand[48] = 0;
        trim_left(o->brand);
    }

    if (max_ext >= 0x80000007u) {
        cpuid(0x80000007u, 0, &a, &b, &c, &d);
        o->invariant_tsc = (d >> 8) & 1;
    }

    if (max_ext >= 0x80000008u) {
        cpuid(0x80000008u, 0, &a, &b, &c, &d);
        o->phys_bits = (u8)(a & 0xFF);
        o->virt_bits = (u8)((a >> 8) & 0xFF);
    } else {
        /* Without that leaf the long mode minimums apply. */
        o->phys_bits = 36;
        o->virt_bits = 48;
    }
}
