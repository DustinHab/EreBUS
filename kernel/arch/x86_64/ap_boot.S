/*
 * ap_boot.S -- the application-processor trampoline: real mode -> long mode.
 *
 * The blob is copied to physical TRAMP_PA (a page below 1 MiB) and an AP
 * is started there by a SIPI. The boot processor fills ap_tramp_params (a
 * transient pml4, a stack, the entry, and the cpu index) before each
 * start. The pml4 identity-maps low memory AND carries the kernel's high
 * half, so execution survives paging coming on and can reach ap_main.
 *
 * The trampoline is 16- and 32-bit code, which the machine's own
 * assembler does not do (it has no .code16). So the assembled bytes are
 * checked in below as a table -- the machine can build its own kernel
 * without a 16-bit assembler -- and the source they came from is kept
 * here, both to read and to regenerate from.
 *
 * To regenerate after changing the trampoline: put the source below back
 * into a .S with the .code16/.code32/.code64 directives, build it with
 * clang for x86_64-unknown-none-elf, then
 *   objcopy -O binary --only-section=.rodata ap_boot.o - | od -An -v -tx1
 * The first 192 bytes (up to ap_tramp_params, whose offset readelf gives)
 * are the code table; the rest is the zeroed params.
 *
 * The source (do not assemble this copy; it is the reference):
 *
 *     .code16
 *   ap_tramp_start:
 *     cli
 *     cld
 *     movw    %cs, %ax
 *     movw    %ax, %ds
 *     lgdt    (gdt_desc - ap_tramp_start)
 *     movl    %cr0, %eax
 *     orl     $1, %eax                 ; protected mode on
 *     movl    %eax, %cr0
 *     .byte 0xEA                        ; far jump to 32-bit code
 *     .word (pm32 - ap_tramp_start + TRAMP_PA)
 *     .word 0x08
 *     .code32
 *   pm32:
 *     movw    $0x10, %ax
 *     movw    %ax, %ds/%es/%ss/%fs/%gs
 *     movl    %cr4, %eax ; orl $(1<<5), %eax ; movl %eax, %cr4      ; PAE
 *     movl    (ap_tramp_params - ap_tramp_start + TRAMP_PA), %eax   ; cr3
 *     movl    %eax, %cr3
 *     movl    $0xC0000080, %ecx ; rdmsr ; orl $((1<<8)|(1<<11)), %eax ; wrmsr ; EFER LME|NX
 *     movl    %cr0, %eax ; orl $0x80000001, %eax ; movl %eax, %cr0  ; paging
 *     .byte 0xEA ; .long (lm64 - ap_tramp_start + TRAMP_PA) ; .word 0x18
 *     .code64
 *   lm64:
 *     movw    $0x20, %ax ; movw %ax, %ds/%es/%ss/%fs/%gs
 *     movq    (ap_tramp_params - ap_tramp_start + TRAMP_PA + 8),  %rsp   ; stack top
 *     movl    (ap_tramp_params - ap_tramp_start + TRAMP_PA + 24), %edi   ; cpu index
 *     movq    (ap_tramp_params - ap_tramp_start + TRAMP_PA + 16), %rax   ; ap_main
 *     callq   *%rax
 *   1:  hlt ; jmp 1b
 *     ; gdt32: null, 0x08 32-bit code, 0x10 32-bit data, 0x18 64-bit code, 0x20 64-bit data
 *     ; gdt_desc: .word gdt32_end-gdt32-1 ; .long gdt32 - ap_tramp_start + TRAMP_PA
 */

    .section .rodata
    .globl ap_tramp_start
    .globl ap_tramp_end
    .globl ap_tramp_params
    .balign 16

ap_tramp_start:
    /* The assembled trampoline: real mode -> protected -> long mode, then
     * the gdt and its descriptor. Regenerate as the header describes. */
    .byte 0xfa,0xfc,0x8c,0xc8,0x8e,0xd8,0x0f,0x01,0x16,0xb8,0x00,0x0f,0x20,0xc0,0x66,0x83
    .byte 0xc8,0x01,0x0f,0x22,0xc0,0xea,0x1a,0x80,0x08,0x00,0x66,0xb8,0x10,0x00,0x8e,0xd8
    .byte 0x8e,0xc0,0x8e,0xd0,0x8e,0xe0,0x8e,0xe8,0x0f,0x20,0xe0,0x83,0xc8,0x20,0x0f,0x22
    .byte 0xe0,0xa1,0xc0,0x80,0x00,0x00,0x0f,0x22,0xd8,0xb9,0x80,0x00,0x00,0xc0,0x0f,0x32
    .byte 0x0d,0x00,0x09,0x00,0x00,0x0f,0x30,0x0f,0x20,0xc0,0x0d,0x01,0x00,0x00,0x80,0x0f
    .byte 0x22,0xc0,0xea,0x59,0x80,0x00,0x00,0x18,0x00,0x66,0xb8,0x20,0x00,0x8e,0xd8,0x8e
    .byte 0xc0,0x8e,0xd0,0x8e,0xe0,0x8e,0xe8,0x48,0x8b,0x24,0x25,0xc8,0x80,0x00,0x00,0x8b
    .byte 0x3c,0x25,0xd8,0x80,0x00,0x00,0x48,0x8b,0x04,0x25,0xd0,0x80,0x00,0x00,0xff,0xd0
    .byte 0xf4,0xeb,0xfd,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0x00,0x00,0x00,0x9a,0xcf,0x00
    .byte 0xff,0xff,0x00,0x00,0x00,0x92,0xcf,0x00,0xff,0xff,0x00,0x00,0x00,0x9a,0xaf,0x00
    .byte 0xff,0xff,0x00,0x00,0x00,0x92,0xaf,0x00,0x27,0x00,0x90,0x80,0x00,0x00,0x00,0x00

    /* offset 192: the boot processor writes these in the copy at TRAMP_PA
     * before each SIPI. Zeroed here. */
ap_tramp_params:
    .quad   0    /* +0  pml4 (transient) */
    .quad   0    /* +8  stack top */
    .quad   0    /* +16 entry (ap_main) */
    .long   0    /* +24 cpu index */
    .long   0    /* +28 pad */
ap_tramp_end:
