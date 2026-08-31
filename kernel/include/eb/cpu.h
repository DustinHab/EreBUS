#ifndef EB_CPU_H
#define EB_CPU_H

#include <eb/types.h>

/* Was CPUID über den Prozessor verrät. Nicht bloß zur Anzeige: ob NX,
 * SMEP und SMAP vorhanden sind, entscheidet darüber, welche Sperren wir
 * beim Aufbau der Seitentabellen überhaupt setzen können. */
typedef struct {
    char vendor[13];       /* "GenuineIntel", "AuthenticAMD", ...        */
    char brand[49];        /* Klartextname, falls der Prozessor ihn hat  */

    u32  family, model, stepping;
    u8   phys_bits;        /* nutzbare physische Adressbits             */
    u8   virt_bits;        /* nutzbare virtuelle Adressbits             */

    /* Für die Speicherverwaltung */
    bool pae;              /* physische Adresserweiterung               */
    bool pge;              /* globale Seiten                            */
    bool pse1g;            /* 1-GiB-Seiten                              */
    bool nx;               /* Seiten als nicht ausführbar markierbar    */

    /* Für die Härtung */
    bool smep;             /* Kernel kann Nutzercode nicht ausführen    */
    bool smap;             /* Kernel kann Nutzerdaten nicht lesen       */
    bool umip;             /* Nutzercode kann Systemtabellen nicht sehen*/
    bool rdrand, rdseed;   /* Zufall aus der Hardware                   */

    /* Sonstiges */
    bool apic, x2apic;
    bool tsc, invariant_tsc;
    bool syscall;
    bool hypervisor;       /* laufen wir in einer virtuellen Maschine?  */
} cpu_info;

void cpu_detect(cpu_info *out);

#endif /* EB_CPU_H */
