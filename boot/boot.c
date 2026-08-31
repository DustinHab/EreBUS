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

/* Reads a file completely into freshly allocated pool memory. */
static VOID *read_file(EFI_FILE_PROTOCOL *root, const CHAR16 *path,
                       UINTN *out_size)
{
    EFI_GUID info_guid = EFI_FILE_INFO_GUID;
    EFI_FILE_PROTOCOL *f = NULL;
    EFI_STATUS st;

    st = root->Open(root, &f, (CHAR16 *)path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(st)) halt(u"kernel.elf not found", st);

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
/* Steps 5 and 6: shut the firmware down and jump                      */
/* ------------------------------------------------------------------ */

/* One block for boot_info and the memory map, claimed before we lose
 * the firmware. 64 KiB is room for well over 2000 ranges. */
#define BOOTDATA_PAGES 16
#define BOOTDATA_BYTES (BOOTDATA_PAGES * 4096)

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

    /* Read and place the kernel. */
    EFI_FILE_PROTOCOL *root = open_boot_volume(image);
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
    bi->kernel_phys      = kbase;
    bi->kernel_size      = kspan;
    bi->acpi_rsdp        = rsdp;
    bi->efi_system_table = (eb_u64)ST;
    bi->mem_ranges       = (eb_u64)ranges;

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
        if (hits_kernel || hits_bootdata) type = EB_MEM_KERNEL;

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

    ((kernel_entry_t)entry)(bi);

    /* The kernel does not return. If it does, stop safely. */
    for (;;) __asm__ volatile ("cli; hlt");
    return EFI_SUCCESS;
}
