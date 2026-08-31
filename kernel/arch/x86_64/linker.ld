/*
 * linker.ld -- the kernel's memory layout.
 *
 * The kernel lives in the upper half of the address space, at
 * -2 GiB, and is loaded at 2 MiB physically. The AT() clauses keep
 * those two apart: the ELF program headers carry the virtual address
 * for execution and the physical one for loading, so the boot loader
 * can place the image low while the code is linked high.
 *
 * Why the upper half at all: the lower half belongs to user processes.
 * Every address space will map the kernel into the same high window and
 * a different program into the low one, so a system call needs no
 * change of page tables. Keeping the kernel at 2 MiB would put it right
 * where user programs have to go.
 *
 * -2 GiB specifically, rather than anywhere else up there, because the
 * compiler's "kernel" code model assumes exactly that: it lets calls
 * and data references use 32-bit offsets, which keeps the code smaller
 * and faster than fully general 64-bit addressing.
 */

ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

KERNEL_VMA = 0xFFFFFFFF80000000;
KERNEL_LMA = 2M;

/* Separate program segments so W^X can be enforced:
 * 5 = read+execute, 4 = read only, 6 = read+write. */
PHDRS
{
    text   PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data   PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = KERNEL_VMA + KERNEL_LMA;
    __kernel_start = .;

    .text : AT(ADDR(.text) - KERNEL_VMA) {
        *(.text.boot)          /* _start has to come first */
        *(.text .text.*)
        . = ALIGN(4K);
        __text_end = .;
    } :text

    .rodata : AT(ADDR(.rodata) - KERNEL_VMA) {
        __rodata_start = .;
        *(.rodata .rodata.*)

        /* Programs that run outside the kernel. They travel inside the
         * kernel image but are mapped into a process's own address
         * space, and the kernel's view of them stays read-only and
         * non-executable -- only the user mapping may run them. Page
         * aligned at both ends because that is the granularity a
         * mapping can have. */
        . = ALIGN(4K);
        __user_start = .;
        *(.user .user.*)
        . = ALIGN(4K);
        __user_end = .;

        . = ALIGN(4K);
        __rodata_end = .;
    } :rodata

    .data : AT(ADDR(.data) - KERNEL_VMA) {
        __data_start = .;
        *(.data .data.*)
    } :data

    .bss : AT(ADDR(.bss) - KERNEL_VMA) {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        . = ALIGN(4K);
        __bss_end = .;
    } :data

    . = ALIGN(4K);
    __kernel_end = .;

    /* Physical address the image was loaded at, so the kernel can work
     * out the offset between the two views of itself. */
    __kernel_phys = KERNEL_LMA;

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
