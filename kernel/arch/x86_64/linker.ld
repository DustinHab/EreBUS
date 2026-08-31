/*
 * linker.ld -- the kernel's memory layout.
 *
 * The kernel sits at a fixed 2 MiB physically and runs there. UEFI maps
 * everything one to one before ExitBootServices, so virtual and
 * physical addresses agree -- which keeps start-up simple and every
 * pointer traceable.
 *
 * Moving into the upper half of the address space comes once we build
 * our own page tables. At that point the base here changes and little
 * else does.
 */

ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

/* Separate program segments so W^X becomes possible later:
 * 5 = read+execute, 4 = read only, 6 = read+write. */
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
        *(.text.boot)          /* _start has to come first */
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

    /* None of this is needed at run time and all of it makes the image
     * bigger. */
    /DISCARD/ : {
        *(.eh_frame*)
        *(.note*)
        *(.comment)
        *(.gnu.hash)
        *(.dynsym)
    }
}
