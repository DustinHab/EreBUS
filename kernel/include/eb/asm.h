/*
 * asm.h -- the assembler: a text of instructions becomes an object.
 *
 * This is what makes the machine programmable on itself without a
 * ceiling. The script language is a jail by design; this is the
 * opposite -- anything the processor can do, written in the ordinary
 * words for it, laid into bytes and run through the same eight system
 * calls every program has. The kernel's rules still hold from the
 * outside: a program made here holds what it is given, and nothing
 * else.
 *
 * What the assembler makes is an OBJECT: the bytes of each section
 * (text, rodata, data, bss, user), the names it lays down and the
 * names it only uses, and every place in the bytes where a name's
 * address belongs. The linker (ld.h) joins objects, gives every name
 * its address, and writes those places -- into an image the loader
 * runs, or into the kernel's own shape.
 *
 * The image: a head -- "EBX2", the code's length, the data's, how
 * much zeroed room follows the data, and where in the code to begin
 * -- then the code and the data. The loader in proc.c lays the code
 * at USER_LOAD_CODE, read and execute, and the data at USER_LOAD_DATA,
 * read and write, never both. The older "EBX1" head, without the
 * entry, is still read: it begins at the code's first byte.
 */
#ifndef EB_ASM_H
#define EB_ASM_H

#include <eb/types.h>

#define USER_LOAD_CODE 0x0000000001000000ULL
#define USER_LOAD_DATA 0x0000000001100000ULL

/* The sections an object has, in the order its bytes are stored. */
enum { ASM_SEC_TEXT, ASM_SEC_RODATA, ASM_SEC_DATA, ASM_SEC_BSS, ASM_SEC_USER, ASM_NSEC };

/* How a place in the bytes refers to a name: relative to the next
 * instruction (a doubleword), the whole address (a quadword), or the
 * address as a sign-extended doubleword. */
enum { ASM_R_REL32 = 1, ASM_R_ABS64 = 2, ASM_R_ABS32S = 3 };

/* Assembles src, in the machine's own dialect, into an object in out.
 * Answers the object's length, or -1 with a one-line reason in err
 * ("line 7: i do not know 'movv'"). */
i64 asm_assemble(const u8 *src, u64 len, u8 *out, u64 max,
                 char *err, u32 errmax);

/* The same, for a text in the gnu dialect (AT&T order, % registers,
 * .directives) -- the way the kernel's own assembly files are
 * written. Translated line by line, then assembled. */
i64 asm_assemble_gnu(const u8 *src, u64 len, u8 *out, u64 max,
                     char *err, u32 errmax);

/* Whether a text reads like the gnu dialect (% registers, .directives),
 * so that a caller need not ask. */
bool gnu_looks(const u8 *src, u64 len);

/* The core both go through: gnu_names says whether a name is private
 * unless made public (the gnu way) or public unless made private. */
i64 asm_assemble_dialect(const u8 *src, u64 len, bool gnu_names,
                         u8 *out, u64 max, char *err, u32 errmax);

/* Whether these bytes are an image the loader would take, and its
 * lengths when they are. */
bool code_image_ok(const u8 *d, u64 n, u32 *code_len, u32 *data_len,
                   u32 *zero_len);

/* The same, with the head's size and where in the code to begin. */
bool code_image_read(const u8 *d, u64 n, u32 *head, u32 *code_len,
                     u32 *data_len, u32 *zero_len, u32 *entry);

#endif /* EB_ASM_H */
