/*
 * io.h -- access to the x86 I/O address space.
 *
 * A separate I/O space is a leftover from 1978, but the serial port and
 * the PS/2 controller hang off it, so here we are.
 */
#ifndef EB_IO_H
#define EB_IO_H

#include <eb/types.h>

static inline void outb(u16 port, u8 val)
{
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port)
{
    u8 v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void outw(u16 port, u16 val)
{
    __asm__ volatile ("outw %0, %1" :: "a"(val), "Nd"(port));
}

static inline u16 inw(u16 port)
{
    u16 v;
    __asm__ volatile ("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void outl(u16 port, u32 val)
{
    __asm__ volatile ("outl %0, %1" :: "a"(val), "Nd"(port));
}

static inline u32 inl(u16 port)
{
    u32 v;
    __asm__ volatile ("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

/* A short delay by touching an unused port -- some old chips need time
 * between two commands. */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

static inline u64 rdtsc(void)
{
    u32 lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}

static inline u64 read_cr2(void)
{
    u64 v;
    __asm__ volatile ("movq %%cr2, %0" : "=r"(v));
    return v;
}

static inline u64 read_cr3(void)
{
    u64 v;
    __asm__ volatile ("movq %%cr3, %0" : "=r"(v));
    return v;
}

static inline void cpu_halt(void) { __asm__ volatile ("hlt"); }
static inline void cpu_cli(void)  { __asm__ volatile ("cli"); }
static inline void cpu_sti(void)  { __asm__ volatile ("sti"); }

/* Turns interrupts off and reports whether they were on, so the caller
 * can put things back exactly as they were. Nesting these is safe;
 * plain cli/sti pairs are not, because the inner sti would re-enable
 * interrupts in the middle of the outer critical section. */
static inline u64 irq_save(void)
{
    u64 flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static inline void irq_restore(u64 flags)
{
    if (flags & (1ULL << 9)) __asm__ volatile ("sti" ::: "memory");
}

static inline bool interrupts_enabled(void)
{
    u64 flags;
    __asm__ volatile ("pushfq; popq %0" : "=r"(flags));
    return (flags & (1u << 9)) != 0;
}

/* Final stop -- never returns. */
static inline __attribute__((noreturn)) void cpu_stop(void)
{
    for (;;) { __asm__ volatile ("cli; hlt"); }
}

#endif /* EB_IO_H */
