/*
 * elf64.h -- so viel vom ELF64-Format, wie zum Laden eines Abbilds nötig ist.
 */
#ifndef EREBUS_ELF64_H
#define EREBUS_ELF64_H

typedef unsigned char      elf_u8;
typedef unsigned short     elf_u16;
typedef unsigned int       elf_u32;
typedef unsigned long long elf_u64;

#define EI_NIDENT 16

typedef struct {
    elf_u8  e_ident[EI_NIDENT];
    elf_u16 e_type;
    elf_u16 e_machine;
    elf_u32 e_version;
    elf_u64 e_entry;
    elf_u64 e_phoff;
    elf_u64 e_shoff;
    elf_u32 e_flags;
    elf_u16 e_ehsize;
    elf_u16 e_phentsize;
    elf_u16 e_phnum;
    elf_u16 e_shentsize;
    elf_u16 e_shnum;
    elf_u16 e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    elf_u32 p_type;
    elf_u32 p_flags;
    elf_u64 p_offset;
    elf_u64 p_vaddr;
    elf_u64 p_paddr;
    elf_u64 p_filesz;
    elf_u64 p_memsz;
    elf_u64 p_align;
} Elf64_Phdr;

#define PT_NULL 0
#define PT_LOAD 1

#define ET_EXEC     2
#define EM_X86_64  62
#define ELFCLASS64  2
#define ELFDATA2LSB 1

#endif /* EREBUS_ELF64_H */
