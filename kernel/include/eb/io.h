/*
 * io.h -- Zugriff auf den x86-Ein-/Ausgabeadressraum.
 *
 * Der getrennte E/A-Adressraum ist ein Relikt, aber die serielle
 * Schnittstelle und der PS/2-Baustein hängen nun einmal daran.
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

/* Kurze Verzögerung durch einen Zugriff auf einen ungenutzten Port --
 * manche alten Bausteine brauchen Zeit zwischen zwei Befehlen. */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

static inline void cpu_halt(void)  { __asm__ volatile ("hlt"); }
static inline void cpu_cli(void)   { __asm__ volatile ("cli"); }
static inline void cpu_sti(void)   { __asm__ volatile ("sti"); }

/* Endgültiger Stillstand -- kehrt nie zurück. */
static inline __attribute__((noreturn)) void cpu_stop(void)
{
    for (;;) { __asm__ volatile ("cli; hlt"); }
}

#endif /* EB_IO_H */
