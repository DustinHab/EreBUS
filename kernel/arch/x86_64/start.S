/*
 * start.S -- erster Kernelcode nach dem Sprung aus dem Lader.
 *
 * Der Lader ruft uns nach SysV-ABI auf, das heisst: RDI zeigt bereits
 * auf die eb_boot_info. Wir brauchen also nur einen eigenen Stapel --
 * der Stapel des Laders gehoert zu Speicher, den wir gleich freigeben
 * wollen.
 */

    .section .text.boot, "ax"
    .global _start
    .type _start, @function
_start:
    cli                         /* Unterbrechungen aus, bis die IDT steht */
    cld                         /* Richtungsflagge definiert setzen --
                                   das ABI verlangt sie geloescht, und
                                   manche Firmware laesst sie gesetzt */

    leaq    stack_top(%rip), %rsp
    andq    $-16, %rsp          /* 16-Byte-Ausrichtung fuer das ABI */
    xorq    %rbp, %rbp          /* Ende der Aufrufkette markieren */

    pushq   $0                  /* falsche Ruecksprungadresse, damit ein
                                   Stapelabzug sauber endet */
    pushq   $0

    /* RDI (boot_info) ist unveraendert -- direkt weiterreichen. */
    call    kmain

    /* kmain kehrt nicht zurueck. Falls doch, sicher anhalten. */
.Lhang:
    cli
    hlt
    jmp     .Lhang
    .size _start, . - _start


    .section .bss
    .align 16
    .global stack_bottom
stack_bottom:
    .skip 64 * 1024             /* 64 KiB Kernelstapel */
    .global stack_top
stack_top:

/* Kennzeichnet den Stapel als nicht ausfuehrbar; unterdrueckt eine
 * Warnung des Binders. */
    .section .note.GNU-stack, "", @progbits
