/*
 * asm.h -- the assembler: a text of instructions becomes a program.
 *
 * This is what makes the machine programmable on itself without a
 * ceiling. The script language is a jail by design; this is the
 * opposite -- anything the processor can do in ring 3, written in
 * the ordinary words for it, laid into a bytes object and run through
 * the same eight system calls every program has. The kernel's rules
 * still hold from the outside: a program made here holds what it is
 * given, and nothing else.
 *
 * The image it makes: a sixteen-byte head -- "EBX1", then the code's
 * length, the data's, and how much zeroed room follows the data --
 * and then the code and the data. The loader in proc.c lays the code
 * at USER_LOAD_CODE, read and execute, and the data at
 * USER_LOAD_DATA, read and write, never both.
 */
#ifndef EB_ASM_H
#define EB_ASM_H

#include <eb/types.h>

#define USER_LOAD_CODE 0x0000000001000000ULL
#define USER_LOAD_DATA 0x0000000001100000ULL

/* Assembles src into out. Answers the image's length, or -1 with a
 * one-line reason in err ("line 7: i do not know 'movv'"). */
i64 asm_assemble(const u8 *src, u64 len, u8 *out, u64 max,
                 char *err, u32 errmax);

/* Whether these bytes are an image the loader would take, and its
 * three lengths when they are. */
bool code_image_ok(const u8 *d, u64 n, u32 *code_len, u32 *data_len,
                   u32 *zero_len);

#endif /* EB_ASM_H */
