/*
 * boot.c -- Erebus boot loader (UEFI application, x86_64).
 *
 * In order:
 *   1. find the graphics output and pick a usable mode
 *   2. open the file system we were loaded from
 *   3. read \erebus\kernel.elf and place its segments
 *   4. take the ACPI root pointer from the firmware configuration table
 *   5. get the memory map and shut the firmware down (ExitBootServices)
 *   6. translate the map into our own format and jump into the kernel
 *
 * From step 5 on there are no firmware services left: no AllocatePool,
 * no text output. Everything the kernel needs must already be in place.
 *
 * The loader stays silent while things go well. What the machine can do
 * is the kernel's job to report; printing it twice would only bury the
 * user in start-up text.
 */
#include "efi.h"
#include "../common/bootinfo.h"
#include "../common/elf64.h"

/* ------------------------------------------------------------------ */
/* What a C library would otherwise provide                            */
/* ------------------------------------------------------------------ */

/* clang emits calls to these names for struct assignments and
 * initialisers even under -ffreestanding, so we supply them. */
void *memset(void *dst, int c, UINTN n)
{
    UINT8 *d = (UINT8 *)dst;
    while (n--) *d++ = (UINT8)c;
    return dst;
}

void *memcpy(void *dst, const void *src, UINTN n)
{
    UINT8 *d = (UINT8 *)dst;
    const UINT8 *s = (const UINT8 *)src;
    while (n--) *d++ = *s++;
    return dst;
}

static EFI_SYSTEM_TABLE  *ST;
static EFI_BOOT_SERVICES *BS;

static void print(const CHAR16 *s)
{
    if (ST && ST->ConOut)
        ST->ConOut->OutputString(ST->ConOut, (CHAR16 *)s);
}

static void print_u64(UINT64 v, int base)
{
    CHAR16 buf[24];
    const CHAR16 *digits = u"0123456789abcdef";
    int i = 22;
    buf[23] = 0;
    if (v == 0) buf[i--] = u'0';
    while (v) { buf[i--] = digits[v % (UINT64)base]; v /= (UINT64)base; }
    print(&buf[i + 1]);
}

static void print_hex(UINT64 v) { print(u"0x"); print_u64(v, 16); }

/* There is no sensible way back from a load failure, so stop where the
 * message can still be read. */
static void halt(const CHAR16 *why, EFI_STATUS st)
{
    print(u"\r\nerebus: cannot start: ");
    print(why);
    if (st) { print(u" (status "); print_hex(st); print(u")"); }
    print(u"\r\n");
    for (;;) __asm__ volatile ("cli; hlt");
}

static int guid_eq(const EFI_GUID *a, const EFI_GUID *b)
{
    const UINT8 *x = (const UINT8 *)a, *y = (const UINT8 *)b;
    for (UINTN i = 0; i < sizeof(EFI_GUID); i++)
        if (x[i] != y[i]) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Step 1: display                                                     */
/* ------------------------------------------------------------------ */

/* As much area as we can get, but no more than a simple compositor can
 * fill comfortably. Anything larger is skipped. */
#define MAX_W 1920u
#define MAX_H 1200u

static EFI_GRAPHICS_OUTPUT_PROTOCOL *setup_graphics(void)
{
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

    EFI_STATUS st = BS->LocateProtocol(&gop_guid, NULL, (VOID **)&gop);
    if (EFI_ERROR(st) || !gop)
        halt(u"no graphics output protocol", st);

    UINT32 best = gop->Mode->Mode;
    UINT64 best_area = 0;

    for (UINT32 m = 0; m < gop->Mode->MaxMode; m++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = NULL;
        UINTN size = 0;
        if (EFI_ERROR(gop->QueryMode(gop, m, &size, &info)) || !info)
            continue;
        /* Only 32-bit formats we can write to directly. */
        if (info->PixelFormat != PixelBlueGreenRedReserved8BitPerColor &&
            info->PixelFormat != PixelRedGreenBlueReserved8BitPerColor)
            continue;
        if (info->HorizontalResolution > MAX_W ||
            info->VerticalResolution   > MAX_H)
            continue;
        UINT64 area = (UINT64)info->HorizontalResolution *
                      (UINT64)info->VerticalResolution;
        if (area > best_area) { best_area = area; best = m; }
    }

    if (best_area == 0)
        halt(u"no usable graphics mode (exotic pixel formats only)", 0);

    if (best != gop->Mode->Mode) {
        st = gop->SetMode(gop, best);
        if (EFI_ERROR(st))
            halt(u"could not set graphics mode", st);
    }
    return gop;
}

/* ------------------------------------------------------------------ */
/* Steps 2 and 3: load the kernel                                      */
/* ------------------------------------------------------------------ */

static EFI_FILE_PROTOCOL *open_boot_volume(EFI_HANDLE image)
{
    EFI_GUID li_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_FILE_PROTOCOL *root = NULL;
    EFI_STATUS st;

    st = BS->HandleProtocol(image, &li_guid, (VOID **)&li);
    if (EFI_ERROR(st)) halt(u"cannot query our own image", st);

    st = BS->HandleProtocol(li->DeviceHandle, &fs_guid, (VOID **)&fs);
    if (EFI_ERROR(st)) halt(u"boot device has no file system", st);

    st = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(st)) halt(u"cannot open root directory", st);
    return root;
}

/* Reads a file completely into freshly allocated pool memory; NULL
 * when there is no such file. */
static VOID *read_file_opt(EFI_FILE_PROTOCOL *root, const CHAR16 *path,
                           UINTN *out_size)
{
    EFI_GUID info_guid = EFI_FILE_INFO_GUID;
    EFI_FILE_PROTOCOL *f = NULL;
    EFI_STATUS st;

    st = root->Open(root, &f, (CHAR16 *)path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(st)) return NULL;

    /* Ask for the size first: GetInfo tells us through BUFFER_TOO_SMALL
     * how much room the trailing file name needs. */
    UINTN isize = 0;
    st = f->GetInfo(f, &info_guid, &isize, NULL);
    if (st != EFI_BUFFER_TOO_SMALL) halt(u"cannot determine file size", st);

    EFI_FILE_INFO *info = NULL;
    st = BS->AllocatePool(EfiLoaderData, isize, (VOID **)&info);
    if (EFI_ERROR(st)) halt(u"out of memory for file info", st);
    st = f->GetInfo(f, &info_guid, &isize, info);
    if (EFI_ERROR(st)) halt(u"cannot read file info", st);

    UINTN size = (UINTN)info->FileSize;
    BS->FreePool(info);

    VOID *buf = NULL;
    st = BS->AllocatePool(EfiLoaderData, size, &buf);
    if (EFI_ERROR(st)) halt(u"out of memory for kernel.elf", st);

    UINTN want = size;
    st = f->Read(f, &want, buf);
    if (EFI_ERROR(st) || want != size) halt(u"cannot read kernel.elf", st);

    f->Close(f);
    *out_size = size;
    return buf;
}

static VOID *read_file(EFI_FILE_PROTOCOL *root, const CHAR16 *path,
                       UINTN *out_size)
{
    VOID *b = read_file_opt(root, path, out_size);
    if (!b) halt(u"kernel.elf not found", 0);
    return b;
}

/* Writes a file whole, replacing one of the name. Failures are let
 * pass: a medium that cannot be written is still one to boot from. */
static void write_file(EFI_FILE_PROTOCOL *root, const CHAR16 *path,
                       const VOID *data, UINTN size)
{
    EFI_FILE_PROTOCOL *f = NULL;
    if (!EFI_ERROR(root->Open(root, &f, (CHAR16 *)path,
                              EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0)))
        f->Delete(f);                        /* closes the handle as well */
    if (EFI_ERROR(root->Open(root, &f, (CHAR16 *)path,
                             EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE |
                             EFI_FILE_MODE_CREATE, 0)))
        return;
    UINTN n = size;
    f->Write(f, &n, (VOID *)data);
    f->Close(f);
}

/* The count of starts since a kernel was installed: raised here before
 * the kernel runs, set back to zero by the kernel once it is up. */
static UINTN read_tries(EFI_FILE_PROTOCOL *root)
{
    UINTN n = 0;
    VOID *b = read_file_opt(root, u"\\erebus\\tries", &n);
    if (!b) return 0;
    UINTN v = n ? ((UINT8 *)b)[0] : 0;
    BS->FreePool(b);
    return v;
}

/* Copies the PT_LOAD segments to their target addresses and returns the
 * entry point. Claims the target range with AllocateAddress -- the
 * kernel is linked to a fixed address. */
static UINT64 load_elf(VOID *img, UINTN img_size,
                       UINT64 *out_base, UINT64 *out_span)
{
    Elf64_Ehdr *eh = (Elf64_Ehdr *)img;

    if (img_size < sizeof(Elf64_Ehdr) ||
        eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F')
        halt(u"kernel.elf is not an ELF file", 0);
    if (eh->e_ident[4] != ELFCLASS64 || eh->e_ident[5] != ELFDATA2LSB)
        halt(u"kernel.elf is not 64-bit little-endian", 0);
    if (eh->e_machine != EM_X86_64)
        halt(u"kernel.elf is not for x86_64", 0);

    Elf64_Phdr *ph = (Elf64_Phdr *)((UINT8 *)img + eh->e_phoff);

    /* Work out the span covered by all loadable segments first. */
    UINT64 lo = ~0ULL, hi = 0;
    for (UINT16 i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0) continue;
        if (ph[i].p_paddr < lo) lo = ph[i].p_paddr;
        if (ph[i].p_paddr + ph[i].p_memsz > hi) hi = ph[i].p_paddr + ph[i].p_memsz;
    }
    if (lo == ~0ULL) halt(u"kernel.elf has no loadable segment", 0);

    UINT64 base  = lo & ~0xFFFULL;
    UINT64 span  = ((hi + 0xFFF) & ~0xFFFULL) - base;
    UINTN  pages = (UINTN)(span >> 12);

    EFI_PHYSICAL_ADDRESS at = base;
    EFI_STATUS st = BS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &at);

    /* The firmware chooses where a pool allocation lands, and the file
     * we just read may be sitting in the very range the kernel is
     * linked for. Move the file out of the way (the vacated buffers
     * stay claimed so the next copy lands elsewhere) and try again. */
    UINTN moves = 0;
    while (EFI_ERROR(st) && moves < 8 &&
           (UINT64)(UINTN)img < base + span &&
           (UINT64)(UINTN)img + img_size > base) {
        VOID *moved = NULL;
        if (EFI_ERROR(BS->AllocatePool(EfiLoaderData, img_size, &moved))) break;
        memcpy(moved, img, img_size);
        img = moved;
        eh = (Elf64_Ehdr *)img;
        ph = (Elf64_Phdr *)((UINT8 *)img + eh->e_phoff);
        moves++;
        at = base;
        st = BS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &at);
    }
    if (EFI_ERROR(st)) {
        print(u"erebus: target address "); print_hex(base);
        print(u" is occupied\r\n");
        halt(u"kernel load address unavailable", st);
    }

    /* Zero everything first, then lay the file contents on top -- that
     * way .bss is clean without special handling. */
    memset((void *)base, 0, (UINTN)span);
    for (UINT16 i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0) continue;
        memcpy((void *)ph[i].p_paddr,
               (UINT8 *)img + ph[i].p_offset,
               (UINTN)ph[i].p_filesz);
    }

    *out_base = base;
    *out_span = span;
    return eh->e_entry;
}

/* ------------------------------------------------------------------ */
/* Step 4: ACPI root pointer                                           */
/* ------------------------------------------------------------------ */

static UINT64 find_rsdp(void)
{
    EFI_GUID acpi20 = EFI_ACPI_20_TABLE_GUID;
    EFI_GUID acpi10 = EFI_ACPI_10_TABLE_GUID;
    UINT64 fallback = 0;

    for (UINTN i = 0; i < ST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *t = &ST->ConfigurationTable[i];
        if (guid_eq(&t->VendorGuid, &acpi20))
            return (UINT64)t->VendorTable;      /* 2.0 wins */
        if (guid_eq(&t->VendorGuid, &acpi10))
            fallback = (UINT64)t->VendorTable;
    }
    return fallback;
}

/* ------------------------------------------------------------------ */
/* Step 5: the initial address space                                   */
/* ------------------------------------------------------------------ */

/*
 * The kernel is linked for the upper half but loaded low, so something
 * has to be mapping the two together before its first instruction runs.
 * Doing that here rather than in the kernel keeps the kernel's entry
 * path trivial: it wakes up already living at its own addresses.
 *
 * Three regions go in:
 *
 *   identity   physical P at virtual P, for the low addresses. The
 *              loader itself is still running from there, and the
 *              firmware left everything mapped that way, so this is
 *              what keeps the ground from vanishing mid-step.
 *   direct map physical P at PHYSMAP_BASE + P. How the kernel reaches
 *              arbitrary physical memory once identity goes away.
 *   kernel     KERNEL_BASE + P for the first gigabyte, which is what
 *              puts the image at the address it was linked for.
 *
 * All of it with 2 MiB pages. 1 GiB pages would need fewer tables, but
 * they are not on every processor, and one page table page per gigabyte
 * of memory is not worth a second code path for.
 *
 * Everything is mapped writable and executable here. That is deliberate
 * and temporary: the kernel replaces these tables with fine-grained
 * ones as soon as it has a frame allocator, and applies W^X there.
 */

#define PTE_PRESENT   0x001ULL
#define PTE_WRITE     0x002ULL
#define PTE_HUGE      0x080ULL

#define SIZE_2M       0x200000ULL
#define SIZE_1G       0x40000000ULL

/* Upper bound on how much we map. Anything past this is device space
 * the kernel will map explicitly when it needs it. */
#define MAP_SPAN_MAX  (64ULL * SIZE_1G)

static UINT64 *pt_pool;
static UINTN   pt_used, pt_total;

static UINT64 *pt_alloc(void)
{
    if (pt_used >= pt_total) return NULL;
    UINT64 *page = (UINT64 *)((UINT8 *)pt_pool + pt_used * 4096);
    pt_used++;
    memset(page, 0, 4096);
    return page;
}

/* Walks down to the page directory and drops in a 2 MiB entry. Runs
 * while the identity mapping is still in force, so table pointers can
 * be used as they are. */
static int map_2m(UINT64 *pml4, UINT64 va, UINT64 pa)
{
    UINTN i4 = (va >> 39) & 0x1FF;
    UINTN i3 = (va >> 30) & 0x1FF;
    UINTN i2 = (va >> 21) & 0x1FF;
    UINT64 *pdpt, *pd;

    if (!(pml4[i4] & PTE_PRESENT)) {
        pdpt = pt_alloc();
        if (!pdpt) return 0;
        pml4[i4] = (UINT64)pdpt | PTE_PRESENT | PTE_WRITE;
    } else {
        pdpt = (UINT64 *)(pml4[i4] & ~0xFFFULL);
    }

    if (!(pdpt[i3] & PTE_PRESENT)) {
        pd = pt_alloc();
        if (!pd) return 0;
        pdpt[i3] = (UINT64)pd | PTE_PRESENT | PTE_WRITE;
    } else {
        pd = (UINT64 *)(pdpt[i3] & ~0xFFFULL);
    }

    pd[i2] = (pa & ~(SIZE_2M - 1)) | PTE_PRESENT | PTE_WRITE | PTE_HUGE;
    return 1;
}

static int map_range(UINT64 *pml4, UINT64 va, UINT64 pa, UINT64 size)
{
    UINT64 end = (pa + size + SIZE_2M - 1) & ~(SIZE_2M - 1);
    UINT64 start = pa & ~(SIZE_2M - 1);
    va &= ~(SIZE_2M - 1);

    for (UINT64 off = 0; start + off < end; off += SIZE_2M)
        if (!map_2m(pml4, va + off, start + off)) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Steps 6 and 7: shut the firmware down and jump                      */
/* ------------------------------------------------------------------ */

/* One block for boot_info and the memory map, claimed before we lose
 * the firmware. 64 KiB is room for well over 2000 ranges. */
#define BOOTDATA_PAGES 16
#define BOOTDATA_BYTES (BOOTDATA_PAGES * 4096)

/* Page table pool. Mapping 64 GiB twice with 2 MiB pages needs 64 page
 * directories each, plus a handful of upper level tables; 192 pages
 * leaves comfortable room above that. */
#define PAGETAB_PAGES 192
#define PAGETAB_BYTES (PAGETAB_PAGES * 4096)

static eb_u32 classify(UINT32 efi_type)
{
    switch (efi_type) {
    case EfiConventionalMemory:
    case EfiBootServicesCode:
    case EfiBootServicesData:
        return EB_MEM_FREE;          /* boot services memory is ours now */
    case EfiLoaderCode:
    case EfiLoaderData:
        return EB_MEM_LOADER;        /* holds the kernel and boot_info */
    case EfiACPIReclaimMemory:
        return EB_MEM_ACPI;
    case EfiMemoryMappedIO:
    case EfiMemoryMappedIOPortSpace:
        return EB_MEM_MMIO;
    default:
        return EB_MEM_RESERVED;
    }
}

typedef void (*kernel_entry_t)(eb_boot_info *) __attribute__((sysv_abi));

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    ST = systab;
    BS = systab->BootServices;

    /* The firmware reboots after a few minutes without a sign of life.
     * We do not need that. */
    BS->SetWatchdogTimer(0, 0, 0, NULL);
    if (ST->ConOut) ST->ConOut->ClearScreen(ST->ConOut);

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = setup_graphics();
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mi = gop->Mode->Info;

    /* Room for the handover data while there is still firmware. */
    EFI_PHYSICAL_ADDRESS bootdata = 0;
    EFI_STATUS st = BS->AllocatePages(AllocateAnyPages, EfiLoaderData,
                                      BOOTDATA_PAGES, &bootdata);
    if (EFI_ERROR(st)) halt(u"out of memory for handover data", st);
    memset((void *)bootdata, 0, BOOTDATA_BYTES);

    eb_boot_info *bi     = (eb_boot_info *)bootdata;
    eb_mem_range *ranges = (eb_mem_range *)(bootdata + sizeof(eb_boot_info));
    UINTN max_ranges = (BOOTDATA_BYTES - sizeof(eb_boot_info)) / sizeof(eb_mem_range);

    /* Page tables have to be claimed now too. After ExitBootServices
     * there is no allocator left, and the tables are built after it. */
    EFI_PHYSICAL_ADDRESS pagetab = 0;
    st = BS->AllocatePages(AllocateAnyPages, EfiLoaderData,
                           PAGETAB_PAGES, &pagetab);
    if (EFI_ERROR(st)) halt(u"out of memory for page tables", st);
    memset((void *)pagetab, 0, PAGETAB_BYTES);
    pt_pool  = (UINT64 *)pagetab;
    pt_total = PAGETAB_PAGES;

    /* Read and place the kernel.
     *
     * Two starts that never cleared the count mean the installed kernel
     * does not come up. The previous one, kept beside it as kernel.old,
     * is put back under the name the loader reads, and boots; the
     * kernel is told, so that it can say so. */
    EFI_FILE_PROTOCOL *root = open_boot_volume(image);
    UINT32 flags = 0;
    UINTN tries = read_tries(root);
    UINTN old_size = 0;
    VOID *old = tries >= 2 ? read_file_opt(root, u"\\erebus\\kernel.old", &old_size) : NULL;
    if (old) {
        print(u"erebus: the installed kernel did not come up twice; the previous one boots\r\n");
        write_file(root, u"\\erebus\\kernel.elf", old, old_size);
        BS->FreePool(old);
        UINT8 zero = 0;
        write_file(root, u"\\erebus\\tries", &zero, 1);
        flags |= EB_BOOT_FELL_BACK;
    } else {
        UINT8 next = (UINT8)(tries < 255 ? tries + 1 : 255);
        write_file(root, u"\\erebus\\tries", &next, 1);
    }
    UINTN elf_size = 0;
    VOID *elf = read_file(root, u"\\erebus\\kernel.elf", &elf_size);

    UINT64 kbase = 0, kspan = 0;
    UINT64 entry = load_elf(elf, elf_size, &kbase, &kspan);
    BS->FreePool(elf);

    UINT64 rsdp = find_rsdp();

    /* Capture the display details before the firmware goes away. */
    bi->magic     = EREBUS_BOOT_MAGIC;
    bi->version   = EREBUS_BOOT_VERSION;
    bi->fb_base   = gop->Mode->FrameBufferBase;
    bi->fb_size   = (eb_u64)gop->Mode->FrameBufferSize;
    bi->fb_width  = mi->HorizontalResolution;
    bi->fb_height = mi->VerticalResolution;
    bi->fb_stride = mi->PixelsPerScanLine;
    bi->fb_format = (mi->PixelFormat == PixelRedGreenBlueReserved8BitPerColor)
                    ? EB_FB_RGBX8888 : EB_FB_BGRX8888;
    bi->flags     = flags;
    bi->kernel_phys      = kbase;
    bi->kernel_virt      = EB_KERNEL_BASE + kbase;
    bi->kernel_size      = kspan;
    bi->acpi_rsdp        = rsdp;
    bi->efi_system_table = (eb_u64)ST;
    bi->mem_ranges       = (eb_u64)ranges;
    bi->physmap_base     = EB_PHYSMAP_BASE;
    bi->pagetab_base     = (eb_u64)pagetab;
    bi->pagetab_size     = PAGETAB_BYTES;

    /* Buffer for the raw EFI map. Ask for the size, then allocate with
     * headroom -- allocating changes the map again. */
    UINTN map_size = 0, map_key = 0, desc_size = 0;
    UINT32 desc_ver = 0;
    BS->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_ver);
    map_size += 8 * desc_size;

    EFI_MEMORY_DESCRIPTOR *map = NULL;
    st = BS->AllocatePool(EfiLoaderData, map_size, (VOID **)&map);
    if (EFI_ERROR(st)) halt(u"out of memory for the memory map", st);

    /* GetMemoryMap hands back a key that stays valid only while the map
     * is unchanged. If ExitBootServices fails, both must be repeated. */
    UINTN entries = 0;
    for (int attempt = 0; attempt < 8; attempt++) {
        UINTN this_size = map_size;
        st = BS->GetMemoryMap(&this_size, map, &map_key, &desc_size, &desc_ver);
        if (EFI_ERROR(st)) halt(u"cannot read the memory map", st);

        entries = this_size / desc_size;
        st = BS->ExitBootServices(image, map_key);
        if (!EFI_ERROR(st)) break;
        if (attempt == 7) halt(u"ExitBootServices keeps failing", st);
    }

    /* ==== no firmware beyond this point: no output, no services ==== */

    UINTN n = 0;
    eb_u64 free_bytes = 0;
    for (UINTN i = 0; i < entries && n < max_ranges; i++) {
        EFI_MEMORY_DESCRIPTOR *d =
            (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)map + i * desc_size);

        eb_u32 type = classify(d->Type);

        /* The range holding the kernel and the handover data is off
         * limits to the memory manager, whatever the firmware says. */
        UINT64 s = d->PhysicalStart;
        UINT64 e = s + (d->NumberOfPages << 12);
        int hits_kernel   = (s < kbase + kspan) && (e > kbase);
        int hits_bootdata = (s < bootdata + BOOTDATA_BYTES) && (e > bootdata);
        int hits_pagetab  = (s < pagetab + PAGETAB_BYTES) && (e > pagetab);
        if (hits_kernel || hits_bootdata || hits_pagetab) type = EB_MEM_KERNEL;

        /* The first page stays blocked: a null pointer should raise a
         * fault, not quietly hit valid memory. */
        if (s == 0 && type == EB_MEM_FREE) {
            if (d->NumberOfPages <= 1) type = EB_MEM_RESERVED;
            else { s += 4096; }
        }

        ranges[n].base  = s;
        ranges[n].pages = (e - s) >> 12;
        ranges[n].type  = type;
        ranges[n]._pad  = 0;
        if (type == EB_MEM_FREE) free_bytes += (e - s);
        n++;
    }

    bi->mem_count = n;
    bi->mem_free  = free_bytes;

    /* How far up does anything worth mapping reach. Reserved regions
     * count: the framebuffer and other device windows live in them. */
    UINT64 span = 4ULL * SIZE_1G;
    for (UINTN i = 0; i < n; i++) {
        UINT64 end = ranges[i].base + ranges[i].pages * 4096;
        if (end > span) span = end;
    }
    if (span > MAP_SPAN_MAX) span = MAP_SPAN_MAX;
    span = (span + SIZE_2M - 1) & ~(SIZE_2M - 1);

    UINT64 *pml4 = pt_alloc();

    int ok = pml4 != NULL;
    /* Identity, so the ground does not disappear under the loader. */
    ok = ok && map_range(pml4, 0, 0, span);
    /* Direct map, the kernel's future window onto physical memory. */
    ok = ok && map_range(pml4, EB_PHYSMAP_BASE, 0, span);
    /* The kernel's own window: the first gigabyte is plenty, the image
     * sits at 2 MiB inside it. */
    ok = ok && map_range(pml4, EB_KERNEL_BASE, 0, SIZE_1G);

    /* On some machines the framebuffer sits above whatever the memory
     * map described. Map it explicitly rather than hope. */
    if (ok && bi->fb_base + bi->fb_size > span) {
        ok = map_range(pml4, bi->fb_base, bi->fb_base, bi->fb_size)
          && map_range(pml4, EB_PHYSMAP_BASE + bi->fb_base,
                       bi->fb_base, bi->fb_size);
    }

    if (!ok) {
        /* Out of table pages, and no firmware left to say so with. The
         * kernel never starts; stopping here is all that is left. */
        for (;;) __asm__ volatile ("cli; hlt");
    }

    bi->pml4         = (eb_u64)pml4;
    bi->physmap_size = span;

    /* From the next instruction on, addresses go through our tables.
     * The loader survives it because the identity mapping above covers
     * exactly where it is running. */
    __asm__ volatile ("movq %0, %%cr3" :: "r"((UINT64)pml4) : "memory");

    /* entry is a virtual address in the upper half, reachable only now. */
    ((kernel_entry_t)entry)(bi);

    /* The kernel does not return. If it does, stop safely. */
    for (;;) __asm__ volatile ("cli; hlt");
    return EFI_SUCCESS;
}
