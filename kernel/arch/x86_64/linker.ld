/*
 * linker.ld -- Speicherbild des Kernels.
 *
 * Der Kernel liegt fest bei 2 MiB physisch und wird dort auch
 * ausgefuehrt. UEFI bildet vor ExitBootServices alles eins zu eins ab,
 * also stimmen virtuelle und physische Adresse ueberein -- das haelt den
 * Start einfach und macht jeden Zeiger nachvollziehbar.
 *
 * Ein Umzug in die obere Haelfte des Adressraums kommt erst, wenn wir
 * eigene Seitentabellen aufbauen. Dann aendert sich hier die Basis und
 * sonst fast nichts.
 */

ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

/* Getrennte Programmsegmente, damit spaeter W^X moeglich ist:
 * 5 = lesen+ausfuehren, 4 = nur lesen, 6 = lesen+schreiben. */
PHDRS
{
    text   PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data   PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 2M;
    __kernel_start = .;

    .text : ALIGN(4K) {
        *(.text.boot)          /* _start muss zuerst kommen */
        *(.text .text.*)
    } :text

    .rodata : ALIGN(4K) {
        *(.rodata .rodata.*)
    } :rodata

    .data : ALIGN(4K) {
        *(.data .data.*)
    } :data

    .bss : ALIGN(4K) {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        . = ALIGN(4K);
        __bss_end = .;
    } :data

    . = ALIGN(4K);
    __kernel_end = .;

    /* Nichts davon wird zur Laufzeit gebraucht und alles davon macht
     * das Abbild groesser. */
    /DISCARD/ : {
        *(.eh_frame*)
        *(.note*)
        *(.comment)
        *(.gnu.hash)
        *(.dynsym)
    }
}
