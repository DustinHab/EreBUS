/*
 * ahci.c -- SATA disks through AHCI, polled (works in early start-up and fault handlers).
 * - up to 8 disks; roles: boot disk (port 0), store, exchange disk
 * - store: GPT partition of the store type on the boot disk or the next, else a marked or blank next disk
 * - foreign disks are never written
 */
#include <eb/blk.h>
#include <eb/pci.h>
#include <eb/vmm.h>
#include <eb/pmm.h>
#include <eb/mm.h>
#include <eb/io.h>
#include <eb/fmt.h>
#include <eb/string.h>

/* --- register layout ------------------------------------------------ */

typedef volatile struct {
    u32 clb, clbu, fb, fbu;
    u32 is, ie, cmd, reserved0;
    u32 tfd, sig, ssts, sctl, serr, sact, ci, sntf, fbs;
    u32 reserved1[11];
    u32 vendor[4];
} hba_port;

typedef volatile struct {
    u32 cap, ghc, is, pi, vs, ccc_ctl, ccc_ports, em_loc, em_ctl, cap2, bohc;
    u8  reserved[0xA0 - 0x2C];
    u8  vendor[0x100 - 0xA0];
    hba_port ports[32];
} hba_mem;

typedef struct {
    u8  flags;         /* command FIS length in dwords, plus write bit */
    u8  flags2;
    u16 prdtl;
    volatile u32 prdbc;
    u32 ctba, ctbau;
    u32 reserved[4];
} cmd_header;

typedef struct {
    u32 dba, dbau, reserved;
    u32 dbc;           /* byte count minus one, interrupt bit at 31 */
} prdt_entry;

typedef struct {
    u8 cfis[64];
    u8 acmd[16];
    u8 reserved[48];
    prdt_entry prdt[1];
} cmd_table;

/* Host to device register FIS -- the shape an ATA command travels in. */
typedef struct {
    u8 type;           /* 0x27 */
    u8 pm_and_c;       /* bit 7 marks it as a command rather than a poke */
    u8 command;
    u8 feature_low;
    u8 lba0, lba1, lba2, device;
    u8 lba3, lba4, lba5, feature_high;
    u8 count_low, count_high, icc, control;
    u8 reserved[4];
} fis_h2d;

_Static_assert(sizeof(cmd_header) == 32, "command header must be 32 bytes");
_Static_assert(sizeof(fis_h2d) == 20, "host to device FIS must be 20 bytes");

#define PORT_CMD_ST  (1u << 0)
#define PORT_CMD_FRE (1u << 4)
#define PORT_CMD_FR  (1u << 14)
#define PORT_CMD_CR  (1u << 15)

#define TFD_BSY (1u << 7)
#define TFD_DRQ (1u << 3)
#define TFD_ERR (1u << 0)

#define ATA_IDENTIFY  0xEC
#define ATA_READ_DMA  0x25   /* READ DMA EXT, 48-bit */
#define ATA_WRITE_DMA 0x35   /* WRITE DMA EXT, 48-bit */
#define ATA_FLUSH     0xEA   /* FLUSH CACHE EXT */

/* --- state ---------------------------------------------------------- */

#define STAGE_SECTORS 128                       /* 64 KiB at a time */
#define STAGE_BYTES   (STAGE_SECTORS * BLK_SECTOR_SIZE)
#define DISK_MAX 8

/* One attached disk: its port and its own command machinery. Every
 * disk on the bus is brought up and numbered; the roles -- the boot
 * disk, the store, the exchange disk -- are pointers into the row. */
typedef struct {
    hba_port   *port;
    u32         index;
    bool        ready;
    u64         sectors;
    char        model[41];
    cmd_header *cmd_list;
    void       *fis_area;
    cmd_table  *table;
    u8         *stage;
    phys_addr   clp, fp, tp, sp;
} ahci_disk;

static hba_mem  *hba;
static ahci_disk disks[DISK_MAX];
static u32       ndisks;
static ahci_disk *boot_p, *aux_p;

/* Where the store lies: on which disk, from which sector, how far. A
 * store is a partition of the store's kind on any disk -- the boot
 * disk included -- or a whole disk that is ours or blank. */
static ahci_disk *store_p;
static u64 store_base, store_span;
static bool present;

/* --- helpers -------------------------------------------------------- */

static bool wait_clear(volatile u32 *reg, u32 mask, u32 spins)
{
    for (u32 i = 0; i < spins; i++) {
        if ((*reg & mask) == 0) return true;
        for (volatile u32 d = 0; d < 1000; d++) { }
    }
    return false;
}

static void port_stop(ahci_disk *d)
{
    d->port->cmd &= ~PORT_CMD_ST;
    d->port->cmd &= ~PORT_CMD_FRE;
    wait_clear(&d->port->cmd, PORT_CMD_CR | PORT_CMD_FR, 1000);
}

static void port_start(ahci_disk *d)
{
    wait_clear(&d->port->cmd, PORT_CMD_CR, 1000);
    d->port->cmd |= PORT_CMD_FRE;
    d->port->cmd |= PORT_CMD_ST;
}

/* Builds one command and waits for it. Returns false on any error the
 * controller reports, rather than leaving the caller to guess from the
 * data. */
static bool run_command(ahci_disk *d, u8 ata_command, u64 lba,
                        u32 sectors, phys_addr buffer, u32 bytes,
                        bool writing)
{
    /* Deliberately no check on ready here: the very first command
     * this issues is the IDENTIFY that decides whether a disk is
     * present at all. The public entry points check instead. */
    if (!d->port) return false;

    /* A busy port means the last command has not finished. Nothing good
     * comes of issuing into that. */
    if (!wait_clear(&d->port->tfd, TFD_BSY | TFD_DRQ, 1000)) return false;

    d->port->is = (u32)-1;          /* clear stale status */
    d->port->serr = d->port->serr;

    cmd_header *h = &d->cmd_list[0];
    h->flags  = (u8)((sizeof(fis_h2d) / 4) & 0x1F);
    if (writing) h->flags |= (1u << 6);
    h->flags2 = 0;
    h->prdtl  = bytes ? 1 : 0;
    h->prdbc  = 0;
    h->ctba   = (u32)d->tp;
    h->ctbau  = (u32)(d->tp >> 32);

    for (u32 i = 0; i < sizeof(cmd_table); i++) ((u8 *)d->table)[i] = 0;

    if (bytes) {
        d->table->prdt[0].dba  = (u32)buffer;
        d->table->prdt[0].dbau = (u32)(buffer >> 32);
        d->table->prdt[0].dbc  = (bytes - 1) & 0x3FFFFF;
    }

    fis_h2d *fis = (fis_h2d *)d->table->cfis;
    fis->type = 0x27;
    fis->pm_and_c = 0x80;
    fis->command = ata_command;
    fis->lba0 = (u8)(lba);
    fis->lba1 = (u8)(lba >> 8);
    fis->lba2 = (u8)(lba >> 16);
    fis->device = 0x40;             /* LBA mode */
    fis->lba3 = (u8)(lba >> 24);
    fis->lba4 = (u8)(lba >> 32);
    fis->lba5 = (u8)(lba >> 40);
    fis->count_low  = (u8)(sectors);
    fis->count_high = (u8)(sectors >> 8);

    d->port->ci = 1;                /* slot zero, and we only use slot zero */

    for (u32 i = 0; i < 1000000; i++) {
        if ((d->port->ci & 1) == 0) break;
        if (d->port->is & (1u << 30)) return false; /* task file error */
    }
    if (d->port->ci & 1) return false;              /* never finished */
    if (d->port->tfd & TFD_ERR) return false;

    return true;
}

/* --- setting up ------------------------------------------------------ */

static bool claim_memory(ahci_disk *d)
{
    /* The command list wants 1 KiB aligned to 1 KiB and the received
     * FIS area 256 bytes aligned to 256, so one page holds both with
     * room to spare. */
    phys_addr page = pmm_alloc();
    if (page == PMM_NO_FRAME) return false;
    d->clp = page;
    d->fp = page + 1024;
    d->cmd_list = (cmd_header *)phys_to_virt(d->clp);
    d->fis_area = phys_to_virt(d->fp);

    d->tp = pmm_alloc();
    if (d->tp == PMM_NO_FRAME) return false;
    d->table = (cmd_table *)phys_to_virt(d->tp);

    /* The staging area has to be contiguous in physical memory: the
     * controller is given an address and a length and knows nothing
     * about page tables. */
    d->sp = pmm_alloc_contig(STAGE_BYTES / PAGE_SIZE);
    if (d->sp == PMM_NO_FRAME) return false;
    d->stage = (u8 *)phys_to_virt(d->sp);

    return true;
}

static u64 identity_sectors(const u16 *id)
{
    u64 s = (u64)id[100] | ((u64)id[101] << 16) |
            ((u64)id[102] << 32) | ((u64)id[103] << 48);
    if (s == 0) s = (u64)id[60] | ((u64)id[61] << 16);
    return s;
}

/* Brings one port up and asks the disk who it is. */
static bool disk_up(ahci_disk *d, hba_port *p, u32 index)
{
    d->port = p;
    d->index = index;
    if (!claim_memory(d)) return false;

    port_stop(d);
    p->clb  = (u32)d->clp;
    p->clbu = (u32)(d->clp >> 32);
    p->fb   = (u32)d->fp;
    p->fbu  = (u32)(d->fp >> 32);
    for (u32 i = 0; i < 1024; i++) ((u8 *)d->cmd_list)[i] = 0;
    for (u32 i = 0; i < 256; i++) ((u8 *)d->fis_area)[i] = 0;
    port_start(d);

    if (!run_command(d, ATA_IDENTIFY, 0, 0, d->sp, 512, false))
        return false;

    const u16 *id = (const u16 *)d->stage;
    d->sectors = identity_sectors(id);

    /* The model name is twenty words with the bytes the other way
     * round, and padded with spaces rather than terminated. */
    for (u32 i = 0; i < 20; i++) {
        d->model[i * 2]     = (char)(id[27 + i] >> 8);
        d->model[i * 2 + 1] = (char)(id[27 + i] & 0xFF);
    }
    d->model[40] = 0;
    for (i32 i = 39; i >= 0 && d->model[i] == ' '; i--) d->model[i] = 0;

    d->ready = true;
    return true;
}

/* --- reading and writing --------------------------------------------- */

static bool disk_read(ahci_disk *d, u64 lba, u32 count, void *dst)
{
    if (!d || !d->ready || count == 0) return false;

    u8 *out = (u8 *)dst;
    while (count > 0) {
        u32 chunk = count > STAGE_SECTORS ? STAGE_SECTORS : count;
        u32 bytes = chunk * BLK_SECTOR_SIZE;

        if (!run_command(d, ATA_READ_DMA, lba, chunk, d->sp, bytes,
                         false))
            return false;

        for (u32 i = 0; i < bytes; i++) out[i] = d->stage[i];

        out += bytes;
        lba += chunk;
        count -= chunk;
    }
    return true;
}

static bool disk_write(ahci_disk *d, u64 lba, u32 count, const void *src)
{
    if (!d || !d->ready || count == 0) return false;

    const u8 *in = (const u8 *)src;
    while (count > 0) {
        u32 chunk = count > STAGE_SECTORS ? STAGE_SECTORS : count;
        u32 bytes = chunk * BLK_SECTOR_SIZE;

        for (u32 i = 0; i < bytes; i++) d->stage[i] = in[i];

        if (!run_command(d, ATA_WRITE_DMA, lba, chunk, d->sp, bytes,
                         true))
            return false;

        in += bytes;
        lba += chunk;
        count -= chunk;
    }

    /* Ask the drive to actually commit rather than hold it in its own
     * cache. A snapshot that is only in a disk's memory is not a
     * snapshot. */
    return run_command(d, ATA_FLUSH, 0, 0, 0, 0, false);
}

/* --- whose disk is it ------------------------------------------------- */

/* The store is written from its first sector up, with no partition
 * table and no file system around it: a stretch of disk that already
 * holds something else would lose it. So a stretch is claimed as the
 * store only when it carries our mark in its first sector, or is
 * blank below the ring and taking blank stretches is allowed -- a
 * fresh image, a wiped disk -- in which case the mark is written and
 * the stretch is ours from then on. Anything else is left exactly as
 * found, read and written by nobody here, and the machine runs
 * without a memory and says so. On a real machine this is the
 * difference between a system and a disk wiper. */
#define STORE_MARK "EREBUS STORE"
#define STORE_MIN_SECTORS 40960u      /* 20 MiB: the ring, and a log worth having */

static u32 le32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static u64 le64(const u8 *p) { return (u64)le32(p) | ((u64)le32(p + 4) << 32); }

/* The partition type that means "a store of this system": a GUID of
 * our own, E2EB0500-5354-4F52-4552-454255530001, here in the order
 * the table keeps it. tools/mkusb.sh makes one; the word "settle"
 * makes one; anyone's partitioning tool can. A partition of this kind
 * on any disk is the store, the boot disk included -- which is how
 * one disk carries the whole system, and how a machine with somebody
 * else's system on the rest of the disk lends this one a corner. */
static const u8 STORE_GUID[16] = { 0x00, 0x05, 0xEB, 0xE2, 0x54, 0x53, 0x52, 0x4F,
                                   0x45, 0x52, 0x45, 0x42, 0x55, 0x53, 0x00, 0x01 };

static bool find_store_partition(ahci_disk *d, u64 *first, u64 *count)
{
    static u8 hdr[BLK_SECTOR_SIZE];
    static u8 ents[32 * BLK_SECTOR_SIZE];
    if (!disk_read(d, 1, 1, hdr)) return false;
    if (memcmp(hdr, "EFI PART", 8) != 0) return false;

    u64 elba = le64(hdr + 72);
    u32 n = le32(hdr + 80), sz = le32(hdr + 84);
    if (sz < 128 || sz > 512 || n == 0 || elba < 2 || elba >= d->sectors) return false;
    u32 bytes = n * sz;
    if (bytes > sizeof(ents)) bytes = sizeof(ents);
    if (!disk_read(d, elba, (bytes + BLK_SECTOR_SIZE - 1) / BLK_SECTOR_SIZE, ents)) return false;

    for (u32 i = 0; (u64)i * sz + sz <= bytes; i++) {
        const u8 *e = ents + i * sz;
        if (memcmp(e, STORE_GUID, 16) != 0) continue;
        u64 f = le64(e + 32), l = le64(e + 40);
        if (l < f || l >= d->sectors) return false;
        *first = f;
        *count = l - f + 1;
        return true;
    }
    return false;
}

static bool claim(ahci_disk *d, u64 base, u64 span, bool take_blank)
{
    static u8 sector[BLK_SECTOR_SIZE];
    static u8 probe[64 * BLK_SECTOR_SIZE];
    u32 port = d->index;
    const char *what = base ? "the partition" : "the disk";

    if (span < STORE_MIN_SECTORS) {
        kprintf("blk:  %s on port %u is too small for a store\n", what, port);
        return false;
    }
    if (!disk_read(d, base, 1, sector)) return false;
    if (memcmp(sector, STORE_MARK, sizeof(STORE_MARK) - 1) == 0) {
        kprintf("blk:  %s on port %u carries our mark; it is the store\n", what, port);
        return true;
    }

    for (u64 lba = 0; lba < 1024; lba += 64) {
        if (!disk_read(d, base + lba, 64, probe)) return false;
        for (u32 i = 0; i < sizeof(probe); i++) {
            if (!probe[i]) continue;
            kprintf("blk:  %s on port %u carries something that is not ours; "
                    "nothing will be written to it\n", what, port);
            return false;
        }
    }
    if (!take_blank) {
        kprintf("blk:  a blank disk on port %u; 'settle on disk %u' in the terminal would make it the store\n",
                port, (u32)(d - disks) + 1);
        return false;
    }

    memset(sector, 0, BLK_SECTOR_SIZE);
    memcpy(sector, STORE_MARK, sizeof(STORE_MARK) - 1);
    if (!disk_write(d, base, 1, sector)) return false;
    kprintf("blk:  a blank %s on port %u; it is the store now and carries our mark\n",
            base ? "partition" : "disk", port);
    return true;
}

/* --- the bus --------------------------------------------------------- */

bool blk_init(void)
{
    const pci_device *dev = pci_find(0x01, 0x06, 0x01);   /* SATA, AHCI */
    if (!dev) { kprintf("blk:  no ahci controller on the bus\n"); return false; }

    /* The controller has to be allowed to reach memory on its own, and
     * that is off until somebody asks for it. */
    u32 command = pci_read32(dev, 0x04);
    pci_write32(dev, 0x04, command | (1u << 1) | (1u << 2));

    phys_addr abar = pci_bar(dev, 5);
    if (!abar) { kprintf("blk:  controller has no register window\n"); return false; }

    /* Device registers, so uncached: a cached read would answer from
     * the last value seen rather than from the controller. */
    if (!vmm_map(vmm_kernel_pml4(), (virt_addr)phys_to_virt(abar), abar,
                 8 * PAGE_SIZE, PAGE_KERNEL_MMIO))
        return false;
    hba = (hba_mem *)phys_to_virt(abar);

    hba->ghc |= (1u << 31);          /* AHCI mode rather than legacy IDE */

    /* Every plain disk on the bus, brought up and numbered in the
     * order of its port. */
    u32 implemented = hba->pi;
    for (u32 i = 0; i < 32 && ndisks < DISK_MAX; i++) {
        if (!(implemented & (1u << i))) continue;

        hba_port *p = &hba->ports[i];
        u32 det = p->ssts & 0x0F;
        u32 ipm = (p->ssts >> 8) & 0x0F;
        if (det != 3 || ipm != 1) continue;      /* nothing, or asleep */
        if (p->sig != 0x00000101) continue;      /* not a plain disk */

        if (disk_up(&disks[ndisks], p, i)) ndisks++;
        else kprintf("blk:  the disk on port %u did not answer IDENTIFY\n", i);
    }
    if (ndisks == 0) {
        kprintf("blk:  controller present but no disk attached\n");
        return false;
    }

    /* Roles. Port zero is where the machine booted from: the kernel
     * lives there, and a kernel built here is installed there. The
     * store is, first, a partition of the store's kind on the boot
     * disk or on the disk after it; failing that, the disk after the
     * boot disk, whole, when it is ours -- or blank, on a machine that
     * booted from its own disk; a machine running from a stick offers
     * a blank disk rather than taking it. The disk after that is the
     * exchange disk files cross on. */
    ahci_disk *next_p = NULL;
    for (u32 i = 0; i < ndisks; i++) {
        if (disks[i].index == 0) { boot_p = &disks[i]; continue; }
        if (!next_p)     next_p = &disks[i];
        else if (!aux_p) aux_p = &disks[i];
    }
    if (boot_p) kprintf("blk:  the boot disk on port 0, %llu sectors\n", boot_p->sectors);

    u64 first = 0, count = 0;
    if (boot_p && find_store_partition(boot_p, &first, &count)) {
        store_p = boot_p;
        store_base = first;
        store_span = count;
        kprintf("blk:  a store partition on the boot disk, %llu sectors from sector %llu\n",
                count, first);
        present = claim(store_p, store_base, store_span, true);
    } else if (next_p) {
        store_p = next_p;
        if (find_store_partition(next_p, &first, &count)) {
            store_base = first;
            store_span = count;
            kprintf("blk:  a store partition on port %u, %llu sectors from sector %llu\n",
                    next_p->index, count, first);
            present = claim(store_p, store_base, store_span, true);
        } else {
            store_base = 0;
            store_span = next_p->sectors;
            present = claim(store_p, store_base, store_span, boot_p != NULL);
        }
        if (!present) store_p = NULL;
    } else {
        kprintf("blk:  no disk to keep the graph on\n");
    }

    if (aux_p)
        kprintf("blk:  an exchange disk on port %u, %llu sectors\n", aux_p->index, aux_p->sectors);
    return true;
}

/* Makes this stretch of that disk the store, now. The stretch was
 * blanked by whoever made it, so a blank one is taken. */
bool blk_adopt(u32 which, u64 first, u64 count)
{
    if (which >= ndisks || !disks[which].ready) return false;
    if (first + count > disks[which].sectors || first + count < first) return false;
    if (!claim(&disks[which], first, count, true)) return false;
    store_p = &disks[which];
    store_base = first;
    store_span = count;
    present = true;
    return true;
}

/* --- by number --------------------------------------------------------- */

u32  blk_disk_count(void)              { return ndisks; }
u64  blk_disk_sectors(u32 which)       { return which < ndisks ? disks[which].sectors : 0; }
const char *blk_disk_model(u32 which)  { return which < ndisks && disks[which].model[0] ? disks[which].model : "a disk"; }
u32  blk_disk_port(u32 which)          { return which < ndisks ? disks[which].index : 0; }

bool blk_disk_read(u32 which, u64 lba, u32 count, void *dst)
{
    if (which >= ndisks || lba + count > disks[which].sectors || lba + count < lba) return false;
    return disk_read(&disks[which], lba, count, dst);
}

bool blk_disk_write(u32 which, u64 lba, u32 count, const void *src)
{
    if (which >= ndisks || lba + count > disks[which].sectors || lba + count < lba) return false;
    return disk_write(&disks[which], lba, count, src);
}

i32  blk_boot_disk(void)  { return boot_p ? (i32)(boot_p - disks) : -1; }
i32  blk_store_disk(void) { return present && store_p ? (i32)(store_p - disks) : -1; }
u64  blk_store_base(void) { return present ? store_base : 0; }
i32  blk_aux_disk(void)   { return aux_p ? (i32)(aux_p - disks) : -1; }

/* --- the roles ---------------------------------------------------------- */

bool blk_present(void)      { return present; }
u64  blk_sectors(void)      { return present ? store_span : 0; }
const char *blk_model(void) { return store_p && store_p->model[0] ? store_p->model : (ndisks ? disks[0].model : "none"); }
u32  blk_port(void)         { return store_p ? store_p->index : (ndisks ? disks[0].index : 0); }

/* The store's sectors count from where the store begins, and end
 * where it ends: a partition's neighbours are never reached. */
bool blk_read(u64 lba, u32 count, void *dst)
{
    if (!present || lba + count > store_span || lba + count < lba) return false;
    return disk_read(store_p, store_base + lba, count, dst);
}

bool blk_write(u64 lba, u32 count, const void *src)
{
    if (!present || lba + count > store_span || lba + count < lba) return false;
    return disk_write(store_p, store_base + lba, count, src);
}

bool blk_aux_present(void)  { return aux_p != NULL; }
u64  blk_aux_sectors(void)  { return aux_p ? aux_p->sectors : 0; }
bool blk_aux_read(u64 lba, u32 count, void *dst)        { return disk_read(aux_p, lba, count, dst); }
bool blk_aux_write(u64 lba, u32 count, const void *src) { return disk_write(aux_p, lba, count, src); }

bool blk_boot_present(void)  { return boot_p != NULL; }
u64  blk_boot_sectors(void)  { return boot_p ? boot_p->sectors : 0; }
bool blk_boot_read(u64 lba, u32 count, void *dst)        { return disk_read(boot_p, lba, count, dst); }
bool blk_boot_write(u64 lba, u32 count, const void *src) { return disk_write(boot_p, lba, count, src); }

/* --- self test -------------------------------------------------------- */

bool blk_selftest(void)
{
    if (!present || store_span < 4096) return false;

    /* Below the ring of generations, which begins at sector 1024, and
     * restored afterwards, so a test run leaves the disk exactly as it
     * found it. */
    const u64 probe = 768;
    static u8 original[BLK_SECTOR_SIZE];
    static u8 pattern[BLK_SECTOR_SIZE];
    static u8 readback[BLK_SECTOR_SIZE];

    if (!blk_read(probe, 1, original)) return false;

    for (u32 i = 0; i < BLK_SECTOR_SIZE; i++)
        pattern[i] = (u8)(i * 7 + 11);

    if (!blk_write(probe, 1, pattern)) return false;
    if (!blk_read(probe, 1, readback)) return false;

    bool same = true;
    for (u32 i = 0; i < BLK_SECTOR_SIZE; i++)
        if (readback[i] != pattern[i]) { same = false; break; }

    blk_write(probe, 1, original);

    if (!same) kprintf("blk:  what was written did not read back\n");
    return same;
}
