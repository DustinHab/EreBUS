/*
 * boot.c -- Erebus-Startlader (UEFI-Anwendung, x86_64).
 *
 * Aufgabe, in dieser Reihenfolge:
 *   1. Grafikausgabe suchen und eine gute Auflösung einstellen
 *   2. Das Dateisystem öffnen, von dem wir selbst geladen wurden
 *   3. \Erebus\kernel.elf lesen und die ELF-Segmente an ihren Platz kopieren
 *   4. Den ACPI-Wurzelzeiger aus der Firmware-Konfigurationstabelle holen
 *   5. Speicherkarte holen und die Firmware abschalten (ExitBootServices)
 *   6. Die Karte ins Erebus-Format umschreiben und in den Kernel springen
 *
 * Ab Schritt 5 gibt es keine Firmware-Dienste mehr -- kein AllocatePool,
 * keine Textausgabe. Alles, was der Kernel danach braucht, muss vorher
 * belegt worden sein.
 */
#include "efi.h"
#include "../common/bootinfo.h"
#include "../common/elf64.h"

/* ------------------------------------------------------------------ */
/* Kleinkram, den uns sonst die C-Bibliothek geben würde               */
/* ------------------------------------------------------------------ */

/* clang erzeugt für Strukturzuweisungen und Initialisierer Aufrufe auf
 * diese Namen, auch bei -ffreestanding. Also selbst bereitstellen. */
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

static void print_dec(UINT64 v) { print_u64(v, 10); }
static void print_hex(UINT64 v) { print(u"0x"); print_u64(v, 16); }

/* Anhalten mit Meldung: es gibt keinen sinnvollen Rückweg aus einem
 * Ladefehler, also bleiben wir stehen, damit man den Text lesen kann. */
static void halt(const CHAR16 *why, EFI_STATUS st)
{
    print(u"\r\n[Erebus] Start abgebrochen: ");
    print(why);
    if (st) { print(u" (Status "); print_hex(st); print(u")"); }
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
/* Schritt 1: Bildschirm                                               */
/* ------------------------------------------------------------------ */

/* Wir wollen möglichst viel Fläche, aber nicht mehr, als ein einfacher
 * Compositor flüssig füllen kann. Alles darüber wird übersprungen. */
#define MAX_W 1920u
#define MAX_H 1200u

static EFI_GRAPHICS_OUTPUT_PROTOCOL *setup_graphics(void)
{
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

    EFI_STATUS st = BS->LocateProtocol(&gop_guid, NULL, (VOID **)&gop);
    if (EFI_ERROR(st) || !gop)
        halt(u"keine Grafikausgabe (GOP) vorhanden", st);

    UINT32 best = gop->Mode->Mode;
    UINT64 best_area = 0;

    for (UINT32 m = 0; m < gop->Mode->MaxMode; m++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = NULL;
        UINTN size = 0;
        if (EFI_ERROR(gop->QueryMode(gop, m, &size, &info)) || !info)
            continue;
        /* Nur 32-Bit-Formate, die wir direkt beschreiben können. */
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
        halt(u"kein brauchbarer Grafikmodus (nur exotische Pixelformate)", 0);

    if (best != gop->Mode->Mode) {
        st = gop->SetMode(gop, best);
        if (EFI_ERROR(st))
            halt(u"Grafikmodus liess sich nicht setzen", st);
    }
    return gop;
}

/* ------------------------------------------------------------------ */
/* Schritt 2+3: Kernel laden                                           */
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
    if (EFI_ERROR(st)) halt(u"eigenes Abbild nicht abfragbar", st);

    st = BS->HandleProtocol(li->DeviceHandle, &fs_guid, (VOID **)&fs);
    if (EFI_ERROR(st)) halt(u"Startdatentraeger hat kein Dateisystem", st);

    st = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(st)) halt(u"Wurzelverzeichnis nicht zu oeffnen", st);
    return root;
}

/* Liest eine Datei vollständig in frisch belegten Pool-Speicher. */
static VOID *read_file(EFI_FILE_PROTOCOL *root, const CHAR16 *path,
                       UINTN *out_size)
{
    EFI_GUID info_guid = EFI_FILE_INFO_GUID;
    EFI_FILE_PROTOCOL *f = NULL;
    EFI_STATUS st;

    st = root->Open(root, &f, (CHAR16 *)path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(st)) halt(u"kernel.elf nicht gefunden", st);

    /* Erst die Größe erfragen: GetInfo sagt uns per BUFFER_TOO_SMALL,
     * wie viel Platz der Name am Ende der Struktur braucht. */
    UINTN isize = 0;
    st = f->GetInfo(f, &info_guid, &isize, NULL);
    if (st != EFI_BUFFER_TOO_SMALL) halt(u"Dateigroesse nicht ermittelbar", st);

    EFI_FILE_INFO *info = NULL;
    st = BS->AllocatePool(EfiLoaderData, isize, (VOID **)&info);
    if (EFI_ERROR(st)) halt(u"kein Speicher fuer Dateiinfo", st);
    st = f->GetInfo(f, &info_guid, &isize, info);
    if (EFI_ERROR(st)) halt(u"Dateiinfo nicht lesbar", st);

    UINTN size = (UINTN)info->FileSize;
    BS->FreePool(info);

    VOID *buf = NULL;
    st = BS->AllocatePool(EfiLoaderData, size, &buf);
    if (EFI_ERROR(st)) halt(u"kein Speicher fuer kernel.elf", st);

    UINTN want = size;
    st = f->Read(f, &want, buf);
    if (EFI_ERROR(st) || want != size) halt(u"kernel.elf nicht lesbar", st);

    f->Close(f);
    *out_size = size;
    return buf;
}

/* Kopiert die PT_LOAD-Segmente an ihre Zieladressen und liefert den
 * Einsprungpunkt. Belegt den Zielbereich fest über AllocateAddress --
 * der Kernel ist auf eine feste Adresse gebunden. */
static UINT64 load_elf(VOID *img, UINTN img_size,
                       UINT64 *out_base, UINT64 *out_span)
{
    Elf64_Ehdr *eh = (Elf64_Ehdr *)img;

    if (img_size < sizeof(Elf64_Ehdr) ||
        eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F')
        halt(u"kernel.elf ist keine ELF-Datei", 0);
    if (eh->e_ident[4] != ELFCLASS64 || eh->e_ident[5] != ELFDATA2LSB)
        halt(u"kernel.elf ist nicht 64-Bit-little-endian", 0);
    if (eh->e_machine != EM_X86_64)
        halt(u"kernel.elf ist nicht fuer x86_64", 0);

    Elf64_Phdr *ph = (Elf64_Phdr *)((UINT8 *)img + eh->e_phoff);

    /* Erst die Spannweite aller Ladesegmente bestimmen. */
    UINT64 lo = ~0ULL, hi = 0;
    for (UINT16 i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0) continue;
        if (ph[i].p_paddr < lo) lo = ph[i].p_paddr;
        if (ph[i].p_paddr + ph[i].p_memsz > hi) hi = ph[i].p_paddr + ph[i].p_memsz;
    }
    if (lo == ~0ULL) halt(u"kernel.elf enthaelt kein Ladesegment", 0);

    UINT64 base  = lo & ~0xFFFULL;
    UINT64 span  = ((hi + 0xFFF) & ~0xFFFULL) - base;
    UINTN  pages = (UINTN)(span >> 12);

    EFI_PHYSICAL_ADDRESS at = base;
    EFI_STATUS st = BS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &at);
    if (EFI_ERROR(st)) {
        print(u"[Erebus] Zieladresse "); print_hex(base);
        print(u" ist belegt\r\n");
        halt(u"Kernel-Ladeadresse nicht verfuegbar", st);
    }

    /* Erst alles nullen, dann die Dateiteile darüberlegen -- damit ist
     * .bss automatisch sauber. */
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
/* Schritt 4: ACPI-Wurzelzeiger                                        */
/* ------------------------------------------------------------------ */

static UINT64 find_rsdp(void)
{
    EFI_GUID acpi20 = EFI_ACPI_20_TABLE_GUID;
    EFI_GUID acpi10 = EFI_ACPI_10_TABLE_GUID;
    UINT64 fallback = 0;

    for (UINTN i = 0; i < ST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *t = &ST->ConfigurationTable[i];
        if (guid_eq(&t->VendorGuid, &acpi20))
            return (UINT64)t->VendorTable;      /* 2.0 hat Vorrang */
        if (guid_eq(&t->VendorGuid, &acpi10))
            fallback = (UINT64)t->VendorTable;
    }
    return fallback;
}

/* ------------------------------------------------------------------ */
/* Schritt 5+6: Firmware abschalten und springen                       */
/* ------------------------------------------------------------------ */

/* Ein Block für boot_info und die Speicherkarte, belegt bevor wir die
 * Firmware verlieren. 64 KiB reichen für gut 2000 Bereiche. */
#define BOOTDATA_PAGES 16
#define BOOTDATA_BYTES (BOOTDATA_PAGES * 4096)

static eb_u32 classify(UINT32 efi_type)
{
    switch (efi_type) {
    case EfiConventionalMemory:
    case EfiBootServicesCode:
    case EfiBootServicesData:
        return EB_MEM_FREE;          /* Boot-Services-Speicher gehört ab
                                        jetzt uns */
    case EfiLoaderCode:
    case EfiLoaderData:
        return EB_MEM_LOADER;        /* enthält u.a. Kernel und boot_info */
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

    /* Die Firmware startet nach ein paar Minuten ohne Rückmeldung neu.
     * Das brauchen wir nicht. */
    BS->SetWatchdogTimer(0, 0, 0, NULL);
    if (ST->ConOut) ST->ConOut->ClearScreen(ST->ConOut);

    print(u"Erebus -- Startlader\r\n");

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = setup_graphics();
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mi = gop->Mode->Info;

    print(u"  Bild    : ");
    print_dec(mi->HorizontalResolution); print(u"x");
    print_dec(mi->VerticalResolution);
    print(u" @ "); print_hex(gop->Mode->FrameBufferBase); print(u"\r\n");

    /* Platz für die Übergabedaten, solange es noch Firmware gibt. */
    EFI_PHYSICAL_ADDRESS bootdata = 0;
    EFI_STATUS st = BS->AllocatePages(AllocateAnyPages, EfiLoaderData,
                                      BOOTDATA_PAGES, &bootdata);
    if (EFI_ERROR(st)) halt(u"kein Speicher fuer Uebergabedaten", st);
    memset((void *)bootdata, 0, BOOTDATA_BYTES);

    eb_boot_info *bi     = (eb_boot_info *)bootdata;
    eb_mem_range *ranges = (eb_mem_range *)(bootdata + sizeof(eb_boot_info));
    UINTN max_ranges = (BOOTDATA_BYTES - sizeof(eb_boot_info)) / sizeof(eb_mem_range);

    /* Kernel einlesen und ablegen. */
    EFI_FILE_PROTOCOL *root = open_boot_volume(image);
    UINTN elf_size = 0;
    VOID *elf = read_file(root, u"\\erebus\\kernel.elf", &elf_size);

    UINT64 kbase = 0, kspan = 0;
    UINT64 entry = load_elf(elf, elf_size, &kbase, &kspan);
    BS->FreePool(elf);

    print(u"  Kernel  : "); print_dec(elf_size); print(u" Bytes nach ");
    print_hex(kbase); print(u", Einsprung "); print_hex(entry); print(u"\r\n");

    UINT64 rsdp = find_rsdp();
    print(u"  ACPI    : "); print_hex(rsdp); print(u"\r\n");

    /* Bildschirmdaten festhalten, bevor die Firmware geht. */
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

    /* Puffer für die rohe EFI-Karte. Erst fragen, wie groß sie ist,
     * dann mit Reserve belegen -- das Belegen selbst verändert die
     * Karte wieder. */
    UINTN map_size = 0, map_key = 0, desc_size = 0;
    UINT32 desc_ver = 0;
    BS->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_ver);
    map_size += 8 * desc_size;

    EFI_MEMORY_DESCRIPTOR *map = NULL;
    st = BS->AllocatePool(EfiLoaderData, map_size, (VOID **)&map);
    if (EFI_ERROR(st)) halt(u"kein Speicher fuer die Speicherkarte", st);

    print(u"  Firmware wird abgeschaltet ...\r\n");

    /* GetMemoryMap liefert einen Schlüssel, der nur gültig bleibt,
     * solange sich die Karte nicht ändert. Schlägt ExitBootServices
     * fehl, muss man beides wiederholen. */
    UINTN entries = 0;
    for (int versuch = 0; versuch < 8; versuch++) {
        UINTN this_size = map_size;
        st = BS->GetMemoryMap(&this_size, map, &map_key, &desc_size, &desc_ver);
        if (EFI_ERROR(st)) halt(u"Speicherkarte nicht lesbar", st);

        entries = this_size / desc_size;
        st = BS->ExitBootServices(image, map_key);
        if (!EFI_ERROR(st)) break;
        if (versuch == 7) halt(u"ExitBootServices schlaegt fehl", st);
    }

    /* ==== ab hier keine Firmware mehr: keine Ausgabe, keine Dienste ==== */

    UINTN n = 0;
    eb_u64 free_bytes = 0;
    for (UINTN i = 0; i < entries && n < max_ranges; i++) {
        EFI_MEMORY_DESCRIPTOR *d =
            (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)map + i * desc_size);

        eb_u32 type = classify(d->Type);

        /* Der Bereich, in dem Kernel und Übergabedaten liegen, ist für
         * den Speicherverwalter tabu -- unabhängig davon, was die
         * Firmware dazu sagt. */
        UINT64 s = d->PhysicalStart;
        UINT64 e = s + (d->NumberOfPages << 12);
        int hits_kernel   = (s < kbase + kspan) && (e > kbase);
        int hits_bootdata = (s < bootdata + BOOTDATA_BYTES) && (e > bootdata);
        if (hits_kernel || hits_bootdata) type = EB_MEM_KERNEL;

        /* Die erste Seite bleibt immer gesperrt: ein Nullzeiger soll
         * eine Ausnahme auslösen und nicht stillschweigend gültigen
         * Speicher treffen. */
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

    /* Der Kernel kehrt nicht zurück. Falls doch: anhalten. */
    for (;;) __asm__ volatile ("cli; hlt");
    return EFI_SUCCESS;
}

