/*
 * program.ld -- a program laid out as the machine loads it (MANUAL 18.1): the code and its constants at
 * 0x1000000, read and execute; the data and the zeroed room at 0x1100000, read and write. tools/mkimage.py
 * reads the two segments out of the linked ELF and writes the image head in front of them.
 */

ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

PHDRS
{
    code PT_LOAD FLAGS(5);   /* read + execute */
    data PT_LOAD FLAGS(6);   /* read + write */
}

SECTIONS
{
    . = 0x1000000;
    .text : {
        *(.text.start)
        *(.text .text.*)
    } :code
    .rodata : {
        *(.rodata .rodata.*)
    } :code

    . = 0x1100000;
    .data : {
        *(.data .data.*)
    } :data
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note*)
        *(.comment)
    }
}
