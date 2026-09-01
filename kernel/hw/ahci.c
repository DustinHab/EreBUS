/*
 * ahci.c -- SATA through an AHCI controller.
 *
 * The controller is told what to do by writing structures into memory
 * and then setting one bit: a list of command headers, each pointing at
 * a command table holding the ATA command and a list of memory regions
 * to move the data through. Setting the matching bit in the issue
 * register hands it over; the bit clears when it is done.
 *
 * Everything here polls rather than waiting for an interrupt. Storage
 * interrupts are worth having once something else is competing for the
 * processor, but polling has one property worth keeping for now: it
 * works identically during early start-up and inside a fault handler,
 * and a snapshot written while the system is coming apart is exactly
 * when it matters most.
 */
#include <eb/blk.h>
#include <eb/pci.h>
#include <eb/vmm.h>
#include <eb/pmm.h>
#include <eb/mm.h>
#include <eb/io.h>
#include <eb/fmt.h>

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

/* One attached disk: its port and its own command machinery. Two of
 * these exist -- the store the graph lives on, and, when a second
 * data disk is attached, the exchange disk files cross on. */
typedef struct {
    hba_port   *port;
    u32         index;
    bool        ready;
    u64         sectors;
    cmd_header *cmd_list;
    void       *fis_area;
    cmd_table  *table;
    u8         *stage;
    phys_addr   clp, fp, tp, sp;
} ahci_disk;

static hba_mem  *hba;
static ahci_disk store_d, aux_d;
static bool present;
static u32  disk_count;
static char model[41];

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
static bool disk_up(ahci_disk *d, hba_port *p, u32 index, bool keep_model)
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

    if (keep_model) {
        /* The model name is twenty words with the bytes the other way
         * round, and padded with spaces rather than terminated. */
        for (u32 i = 0; i < 20; i++) {
            model[i * 2]     = (char)(id[27 + i] >> 8);
            model[i * 2 + 1] = (char)(id[27 + i] & 0xFF);
        }
        model[40] = 0;
        for (i32 i = 39; i >= 0 && model[i] == ' '; i--) model[i] = 0;
    }

    d->ready = true;
    return true;
}

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

    /* Sort the disks by role. Port zero is where the machine booted
     * from; the first port after it is the store the graph lives on;
     * the port after that, when one is there, is the exchange disk
     * files cross on. With only one disk at all, that one is the
     * store and the caller decides whether writing to it is
     * sensible. */
    u32 implemented = hba->pi;
    i32 found[32];
    u32 nfound = 0;
    for (u32 i = 0; i < 32; i++) {
        if (!(implemented & (1u << i))) continue;

        hba_port *p = &hba->ports[i];
        u32 det = p->ssts & 0x0F;
        u32 ipm = (p->ssts >> 8) & 0x0F;
        if (det != 3 || ipm != 1) continue;      /* nothing, or asleep */
        if (p->sig != 0x00000101) continue;      /* not a plain disk */

        found[nfound++] = (i32)i;
        disk_count++;
    }
    if (nfound == 0) {
        kprintf("blk:  controller present but no disk attached\n");
        return false;
    }

    i32 store_at = -1, aux_at = -1;
    for (u32 i = 0; i < nfound; i++) {
        if (found[i] == 0) continue;
        if (store_at < 0)     store_at = found[i];
        else if (aux_at < 0)  aux_at = found[i];
    }
    if (store_at < 0) store_at = found[0];

    if (!disk_up(&store_d, &hba->ports[store_at], (u32)store_at, true)) {
        kprintf("blk:  the disk did not answer IDENTIFY\n");
        return false;
    }
    present = true;

    if (aux_at >= 0) {
        if (disk_up(&aux_d, &hba->ports[aux_at], (u32)aux_at, false))
            kprintf("blk:  an exchange disk on port %d, %llu sectors\n",
                    aux_at, aux_d.sectors);
        else
            kprintf("blk:  the exchange disk did not answer\n");
    }
    return true;
}

bool blk_present(void)      { return present; }
u64  blk_sectors(void)      { return store_d.sectors; }
const char *blk_model(void) { return present ? model : "none"; }
u32  blk_port(void)         { return store_d.index; }
u32  blk_disk_count(void)   { return disk_count; }

bool blk_aux_present(void)  { return aux_d.ready; }
u64  blk_aux_sectors(void)  { return aux_d.sectors; }

/* --- reading and writing --------------------------------------------- */

static bool disk_read(ahci_disk *d, u64 lba, u32 count, void *dst)
{
    if (!d->ready || count == 0) return false;

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
    if (!d->ready || count == 0) return false;

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

bool blk_read(u64 lba, u32 count, void *dst)
{
    return present && disk_read(&store_d, lba, count, dst);
}

bool blk_write(u64 lba, u32 count, const void *src)
{
    return present && disk_write(&store_d, lba, count, src);
}

bool blk_aux_read(u64 lba, u32 count, void *dst)
{
    return disk_read(&aux_d, lba, count, dst);
}

bool blk_aux_write(u64 lba, u32 count, const void *src)
{
    return disk_write(&aux_d, lba, count, src);
}

/* --- self test -------------------------------------------------------- */

bool blk_selftest(void)
{
    if (!present || store_d.sectors < 4096) return false;

    /* Well away from anything we keep, and restored afterwards, so a
     * test run leaves the disk exactly as it found it. */
    const u64 probe = 2048;
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
