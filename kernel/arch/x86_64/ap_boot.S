/*
 * ap_boot.S -- the application-processor trampoline: real mode -> long mode.
 *
 * The blob is copied to physical TRAMP_PA (a page below 1 MiB) and an AP
 * is started there by a SIPI. The boot processor fills ap_tramp_params
 * (a transient pml4, a stack, the entry, and the cpu index) before each
 * start. The pml4 identity-maps low memory AND carries the kernel's high
 * half, so execution survives paging coming on and can reach ap_main,
 * which then switches to the kernel's own tables.
 *
 * Far jumps and the gdt base are hand-encoded against TRAMP_PA, because
 * the blob runs where it is copied, not where it was linked.
 */

#define TRAMP_PA 0x8000

    .section .rodata
    .globl ap_tramp_start
    .globl ap_tramp_end
    .globl ap_tramp_params
    .balign 16

    .code16
ap_tramp_start:
    cli
    cld
    movw    %cs, %ax
    movw    %ax, %ds
    lgdt    (gdt_desc - ap_tramp_start)
    movl    %cr0, %eax
    orl     $1, %eax                 /* protected mode on */
    movl    %eax, %cr0
    /* far jump to 32-bit code: EA, off16, sel16 */
    .byte   0xEA
    .word   (pm32 - ap_tramp_start + TRAMP_PA)
    .word   0x08

    .code32
pm32:
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    %ax, %fs
    movw    %ax, %gs
    movl    %cr4, %eax
    orl     $(1 << 5), %eax          /* PAE */
    movl    %eax, %cr4
    movl    (ap_tramp_params - ap_tramp_start + TRAMP_PA), %eax   /* cr3 = pml4 */
    movl    %eax, %cr3
    movl    $0xC0000080, %ecx        /* EFER */
    rdmsr
    orl     $((1 << 8) | (1 << 11)), %eax   /* long mode enable, nx enable */
    wrmsr
    movl    %cr0, %eax
    orl     $0x80000001, %eax        /* paging + protection */
    movl    %eax, %cr0
    /* far jump to 64-bit code: EA, off32, sel16 */
    .byte   0xEA
    .long   (lm64 - ap_tramp_start + TRAMP_PA)
    .word   0x18

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
gdt32:
    .quad   0x0000000000000000       /* null */
    .quad   0x00CF9A000000FFFF       /* 0x08 32-bit code */
    .quad   0x00CF92000000FFFF       /* 0x10 32-bit data */
    .quad   0x00AF9A000000FFFF       /* 0x18 64-bit code */
    .quad   0x00AF92000000FFFF       /* 0x20 64-bit data */
gdt32_end:
gdt_desc:
    .word   gdt32_end - gdt32 - 1
    .long   gdt32 - ap_tramp_start + TRAMP_PA

    .balign 16
ap_tramp_params:
    .quad   0    /* +0  pml4 (transient) */
    .quad   0    /* +8  stack top */
    .quad   0    /* +16 entry (ap_main) */
    .long   0    /* +24 cpu index */
    .long   0    /* +28 pad */
ap_tramp_end:
