/*
 * ap_boot_ref.S -- the assemblable reference for the AP-trampoline byte
 * table checked into kernel/arch/x86_64/ap_boot.S.
 *
 * That table is machine code the kernel copies to low memory and starts
 * each application processor at. It is kept as raw bytes because the
 * machine's own assembler has no 16-bit mode, so it cannot build a
 * real-mode trampoline from source. This file is the source those bytes
 * came from, in a form clang can assemble; tools/tramp-check.sh assembles
 * it and compares, so the table cannot drift from its stated source
 * unnoticed. Edit this file to change the trampoline, then rebuild the
 * table from it as the check prints.
 *
 * TRAMP_PA must match kernel/hw/smp.c.
 */
    .set TRAMP_PA, 0x8000

    .section .rodata
    .globl ap_tramp_start
    .globl ap_tramp_params
    .globl ap_tramp_end
    .balign 16

    .code16
ap_tramp_start:
    cli
    cld
    movw    %cs, %ax
    movw    %ax, %ds
    lgdt    (gdt_desc - ap_tramp_start)
    movl    %cr0, %eax
    orl     $1, %eax                          /* protected mode on */
    movl    %eax, %cr0
    .byte 0xEA                                /* far jump to 32-bit code */
    .word (pm32 - ap_tramp_start + TRAMP_PA)
    .word 0x08

    .code32
pm32:
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    %ax, %fs
    movw    %ax, %gs
    movl    %cr4, %eax
    orl     $(1 << 5), %eax                   /* PAE */
    movl    %eax, %cr4
    movl    (ap_tramp_params - ap_tramp_start + TRAMP_PA), %eax   /* cr3 */
    movl    %eax, %cr3
    movl    $0xC0000080, %ecx
    rdmsr
    orl     $((1 << 8) | (1 << 11)), %eax     /* EFER LME|NX */
    wrmsr
    movl    %cr0, %eax
    orl     $0x80000001, %eax                 /* paging | protected */
    movl    %eax, %cr0
    .byte 0xEA                                /* far jump to 64-bit code */
    .long (lm64 - ap_tramp_start + TRAMP_PA)
    .word 0x18

    .code64
lm64:
    movw    $0x20, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    %ax, %fs
    movw    %ax, %gs
    movq    (ap_tramp_params - ap_tramp_start + TRAMP_PA + 8),  %rsp   /* stack top */
    movl    (ap_tramp_params - ap_tramp_start + TRAMP_PA + 24), %edi   /* cpu index */
    movq    (ap_tramp_params - ap_tramp_start + TRAMP_PA + 16), %rax   /* ap_main */
    callq   *%rax
1:  hlt
    jmp     1b

    .balign 16
    /* null, 0x08 32-bit code, 0x10 32-bit data, 0x18 64-bit code, 0x20 64-bit data */
gdt32:
    .quad 0x0000000000000000
    .quad 0x00CF9A000000FFFF
    .quad 0x00CF92000000FFFF
    .quad 0x00AF9A000000FFFF
    .quad 0x00AF92000000FFFF
gdt32_end:
gdt_desc:
    .word gdt32_end - gdt32 - 1
    .long gdt32 - ap_tramp_start + TRAMP_PA

    .balign 16
ap_tramp_params:
    .quad   0    /* +0  pml4 (transient) */
    .quad   0    /* +8  stack top */
    .quad   0    /* +16 entry (ap_main) */
    .long   0    /* +24 cpu index */
    .long   0    /* +28 pad */
ap_tramp_end:
