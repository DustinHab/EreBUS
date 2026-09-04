/*
 * vmm.c -- kernel page tables and the protections they carry.
 * - replaces the loader's flat writable+executable mapping with per-region tables
 * - NX, CR0.WP, SMEP, SMAP switched on once the tables are precise
 * - PAT slot 1 = write-combining (PTE_PWT), used for the framebuffer
 */
#include <eb/vmm.h>
#include <eb/pmm.h>
#include <eb/mm.h>
#include <eb/cpu.h>
#include <eb/fmt.h>
#include <eb/io.h>
#include <eb/panic.h>

#define SIZE_2M 0x200000ULL
#define ADDR_MASK 0x000FFFFFFFFFF000ULL

static phys_addr kernel_pml4;
static vmm_protections active;

/* ------------------------------------------------------------------ */
/* Walking and building                                                */
/* ------------------------------------------------------------------ */

static u64 *table_at(phys_addr p)
{
    return (u64 *)phys_to_virt(p & ADDR_MASK);
}

/* Returns the next table down, creating it when asked to.
 *
 * Intermediate entries stay permissive on write and execute -- the
 * processor takes the most restrictive of all four levels, so those
 * belong on the leaf where they can be read off directly.
 *
 * The user bit is the exception, and it is not optional. The processor
 * grants user access only if every level down to the leaf allows it, so
 * a perfectly correct leaf under a kernel-only directory is unreachable
 * from ring 3. That failure looks like a protection violation on a page
 * that is plainly present, which reads as a nonsense error until one
 * remembers the levels are combined. Hence the flag travelling down. */
static u64 *step(u64 *table, u64 index, bool create, u64 flags)
{
    u64 pass_down = PTE_PRESENT | PTE_WRITE | (flags & PTE_USER);

    if (!(table[index] & PTE_PRESENT)) {
        if (!create) return NULL;
        phys_addr frame = pmm_alloc();
        if (frame == PMM_NO_FRAME) return NULL;
        table[index] = frame | pass_down;
        return table_at(frame);
    }
    if (table[index] & PTE_HUGE) return NULL;   /* already a 2 MiB leaf */

    /* An existing table on the way to a user mapping has to allow user
     * access too, even if it was first created for something else. */
    table[index] |= (flags & PTE_USER);
    return table_at(table[index]);
}

static bool map_4k(u64 *pml4v, virt_addr va, phys_addr pa, u64 flags)
{
    u64 *pdpt = step(pml4v, (va >> 39) & 0x1FF, true, flags);
    if (!pdpt) return false;
    u64 *pd = step(pdpt, (va >> 30) & 0x1FF, true, flags);
    if (!pd) return false;
    u64 *pt = step(pd, (va >> 21) & 0x1FF, true, flags);
    if (!pt) return false;

    pt[(va >> 12) & 0x1FF] = (pa & ADDR_MASK) | flags;
    return true;
}

static bool map_2m(u64 *pml4v, virt_addr va, phys_addr pa, u64 flags)
{
    u64 *pdpt = step(pml4v, (va >> 39) & 0x1FF, true, flags);
    if (!pdpt) return false;
    u64 *pd = step(pdpt, (va >> 30) & 0x1FF, true, flags);
    if (!pd) return false;

    pd[(va >> 21) & 0x1FF] = (pa & ~(SIZE_2M - 1)) | flags | PTE_HUGE;
    return true;
}

bool vmm_map(phys_addr pml4, virt_addr va, phys_addr pa, u64 size, u64 flags)
{
    u64 *pml4v = table_at(pml4);
    u64 done = 0;

    while (done < size) {
        u64 left = size - done;
        bool huge = ((va + done) % SIZE_2M) == 0 &&
                    ((pa + done) % SIZE_2M) == 0 &&
                    left >= SIZE_2M;

        if (huge) {
            if (!map_2m(pml4v, va + done, pa + done, flags)) return false;
            done += SIZE_2M;
        } else {
            if (!map_4k(pml4v, va + done, pa + done, flags)) return false;
            done += PAGE_SIZE;
        }
    }
    return true;
}

bool vmm_unmap_page(phys_addr pml4, virt_addr va, phys_addr *out_frame)
{
    /* Takes one 4 KiB leaf out of a mapping and hands back the frame it
     * pointed at. Deliberately nothing more: whether that frame should
     * be freed is the caller's knowledge -- a stack page is owned, a
     * shared code page emphatically is not -- and the table pages
     * themselves stay for whoever tears down the whole space. */
    u64 *t = table_at(pml4);

    u64 e = t[(va >> 39) & 0x1FF];
    if (!(e & PTE_PRESENT)) return false;
    t = table_at(e);

    e = t[(va >> 30) & 0x1FF];
    if (!(e & PTE_PRESENT) || (e & PTE_HUGE)) return false;
    t = table_at(e);

    e = t[(va >> 21) & 0x1FF];
    if (!(e & PTE_PRESENT) || (e & PTE_HUGE)) return false;
    t = table_at(e);

    u64 *leaf = &t[(va >> 12) & 0x1FF];
    if (!(*leaf & PTE_PRESENT)) return false;

    if (out_frame) *out_frame = *leaf & ADDR_MASK;
    *leaf = 0;

    /* The processor may still hold the old translation. Flushing it
     * here rather than trusting the next CR3 write means the moment
     * this returns, the page really is gone. */
    __asm__ volatile ("invlpg (%0)" :: "r"(va) : "memory");
    return true;
}

bool vmm_resolve(virt_addr va, phys_addr *out_pa, u64 *out_flags)
{
    u64 *t = table_at(read_cr3());

    u64 e = t[(va >> 39) & 0x1FF];
    if (!(e & PTE_PRESENT)) return false;
    t = table_at(e);

    e = t[(va >> 30) & 0x1FF];
    if (!(e & PTE_PRESENT)) return false;
    if (e & PTE_HUGE) {                       /* 1 GiB leaf */
        if (out_pa) *out_pa = (e & ADDR_MASK) + (va & 0x3FFFFFFF);
        if (out_flags) *out_flags = e & ~ADDR_MASK;
        return true;
    }
    t = table_at(e);

    e = t[(va >> 21) & 0x1FF];
    if (!(e & PTE_PRESENT)) return false;
    if (e & PTE_HUGE) {                       /* 2 MiB leaf */
        if (out_pa) *out_pa = (e & ADDR_MASK) + (va & (SIZE_2M - 1));
        if (out_flags) *out_flags = e & ~ADDR_MASK;
        return true;
    }
    t = table_at(e);

    e = t[(va >> 12) & 0x1FF];
    if (!(e & PTE_PRESENT)) return false;
    if (out_pa) *out_pa = (e & ADDR_MASK) + (va & 0xFFF);
    if (out_flags) *out_flags = e & ~ADDR_MASK;
    return true;
}

phys_addr vmm_kernel_pml4(void) { return kernel_pml4; }
vmm_protections vmm_active_protections(void) { return active; }

/* ------------------------------------------------------------------ */
/* Turning the protections on                                          */
/* ------------------------------------------------------------------ */

#define EFER_MSR 0xC0000080u
#define EFER_NXE (1ULL << 11)

#define CR4_PGE  (1ULL << 7)
#define CR4_UMIP (1ULL << 11)
#define CR4_SMEP (1ULL << 20)
#define CR4_SMAP (1ULL << 21)

#define CR0_WP   (1ULL << 16)
#define CR0_MP   (1ULL << 1)
#define CR0_EM   (1ULL << 2)
#define CR0_TS   (1ULL << 3)
#define CR0_NW   (1ULL << 29)
#define CR0_CD   (1ULL << 30)

#define PAT_MSR  0x277u

#define CR4_OSFXSR     (1ULL << 9)
#define CR4_OSXMMEXCPT (1ULL << 10)

static u64 read_msr(u32 msr)
{
    u32 lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | lo;
}

static void write_msr(u32 msr, u64 value)
{
    __asm__ volatile ("wrmsr" :: "c"(msr), "a"((u32)value),
                      "d"((u32)(value >> 32)));
}

static u64 read_cr0(void)
{
    u64 v; __asm__ volatile ("movq %%cr0, %0" : "=r"(v)); return v;
}

static void write_cr0(u64 v)
{
    __asm__ volatile ("movq %0, %%cr0" :: "r"(v) : "memory");
}

static u64 read_cr4(void)
{
    u64 v; __asm__ volatile ("movq %%cr4, %0" : "=r"(v)); return v;
}

static void write_cr4(u64 v)
{
    __asm__ volatile ("movq %0, %%cr4" :: "r"(v) : "memory");
}

/* Teaches the processor what PTE_PWT means.
 *
 * The page attribute table is eight slots, and a page's three cache
 * bits pick one of them. Out of reset slot 1 -- the one PWT alone
 * selects -- says write-through, which nothing here wants. Rewriting it
 * to write-combining gives every mapping a way to ask for that with a
 * single bit that works the same on 4 KiB and 2 MiB pages, because the
 * slot number stays below four either way and the bit that differs
 * between the two page sizes stays clear.
 *
 * The other seven slots keep their reset meanings, so every mapping
 * that does not set PWT is unaffected, and there are none that do until
 * this returns.
 *
 * Changing the table while caches hold lines described by the old
 * meanings is the one hazard, so the processor is taken through the
 * sequence the manual asks for: caches off, written back and emptied,
 * table changed, then caches on again. This runs once, before the
 * kernel's own tables are live. */
static void enable_pat(const cpu_info *cpu)
{
    if (!cpu->pat) return;

    /* WB, WC, UC-, UC, and the same four again. */
    const u64 table = 0x0007010600070106ULL;

    u64 cr0 = read_cr0();
    write_cr0((cr0 | CR0_CD) & ~CR0_NW);
    __asm__ volatile ("wbinvd" ::: "memory");
    write_msr(PAT_MSR, table);
    __asm__ volatile ("wbinvd" ::: "memory");
    write_cr0(cr0);

    active.wc = true;
}

/* NX has to be enabled before any table entry carries bit 63, or the
 * processor treats that bit as reserved and every such page faults. */
static void enable_nx(const cpu_info *cpu)
{
    if (!cpu->nx) return;
    write_msr(EFER_MSR, read_msr(EFER_MSR) | EFER_NXE);
    active.nx = true;
}

static void enable_after_switch(const cpu_info *cpu)
{
    /* The vector unit is switched on for programs -- the compiler's
     * floating point lives in xmm registers -- and never used by the
     * kernel itself, which is built without it. Each program's
     * registers are saved and restored around it by the scheduler,
     * so nothing leaks between programs and nothing is lost. */
    write_cr0((read_cr0() | CR0_WP | CR0_MP) & ~(CR0_EM | CR0_TS));
    active.wp = true;

    u64 cr4 = read_cr4() | CR4_OSFXSR | CR4_OSXMMEXCPT;
    if (cpu->smep) { cr4 |= CR4_SMEP; active.smep = true; }
    if (cpu->smap) { cr4 |= CR4_SMAP; active.smap = true; }
    if (cpu->umip) { cr4 |= CR4_UMIP; active.umip = true; }
    write_cr4(cr4);
}

/* ------------------------------------------------------------------ */
/* Building the kernel address space                                   */
/* ------------------------------------------------------------------ */

static bool is_real_memory(u32 type)
{
    return type == EB_MEM_FREE || type == EB_MEM_LOADER ||
           type == EB_MEM_KERNEL || type == EB_MEM_ACPI;
}

/* Maps one section of the kernel image with its own permissions. The
 * boundaries come from the linker script and are already page aligned. */
static bool map_section(phys_addr pml4, const char *from, const char *to,
                        u64 flags, const char *what)
{
    virt_addr start = (virt_addr)from;
    virt_addr end   = (virt_addr)to;
    if (end <= start) return true;

    if (!vmm_map(pml4, start, kernel_virt_to_phys(from), end - start, flags)) {
        kprintf("vmm:  could not map the kernel's %s\n", what);
        return false;
    }
    return true;
}

void vmm_init(const eb_boot_info *bi)
{
    cpu_info cpu;
    cpu_detect(&cpu);
    enable_nx(&cpu);
    enable_pat(&cpu);

    phys_addr root = pmm_alloc();
    if (root == PMM_NO_FRAME)
        panic("no memory for the top level page table");
    kernel_pml4 = root;

    const eb_mem_range *r = (const eb_mem_range *)phys_to_virt(bi->mem_ranges);
    bool ok = true;

    /* The direct map: every byte of real memory, readable and writable,
     * and never executable. Nothing the kernel reaches through here is
     * code, so there is no reason for the processor to accept it as
     * such -- and plenty of reason not to. */
    u64 top = 0;
    for (u64 i = 0; i < bi->mem_count && ok; i++) {
        if (!is_real_memory(r[i].type)) continue;
        phys_addr base = r[i].base & ~(SIZE_2M - 1);
        u64 end = r[i].base + r[i].pages * PAGE_SIZE;
        u64 size = (end - base + SIZE_2M - 1) & ~(SIZE_2M - 1);
        ok = vmm_map(root, EB_PHYSMAP_BASE + base, base, size,
                     PAGE_KERNEL_DATA);
        if (end > top) top = end;
    }
    if (!ok) panic("could not build the direct map");

    /* The kernel image, one section at a time. This is where W^X stops
     * being a slogan: after this, no page of the kernel is both
     * writable and executable, and the processor enforces it. */
    ok = map_section(root, __kernel_start, __text_end,
                     PAGE_KERNEL_CODE, "code")
      && map_section(root, __rodata_start, __rodata_end,
                     PAGE_KERNEL_RO, "constants")
      && map_section(root, __data_start, __bss_end,
                     PAGE_KERNEL_DATA, "data");
    if (!ok) panic("could not map the kernel image");

    /* The framebuffer is a device window, usually far above installed
     * memory, so the direct map above does not reach it.
     *
     * Write-combining, not ordinary memory: see PAGE_KERNEL_WC. On a
     * machine that only ever ran under emulation this looks like a
     * detail, because there the framebuffer is plain memory and every
     * mapping is as fast as any other. On real hardware it is the
     * difference between a console that scrolls and one that crawls. */
    if (bi->fb_base && bi->fb_size) {
        phys_addr fb = bi->fb_base & ~(SIZE_2M - 1);
        u64 fb_size = (bi->fb_base + bi->fb_size - fb + SIZE_2M - 1)
                      & ~(SIZE_2M - 1);
        if (!vmm_map(root, EB_PHYSMAP_BASE + fb, fb, fb_size,
                     active.wc ? PAGE_KERNEL_WC : PAGE_KERNEL_DATA))
            panic("could not map the framebuffer");
    }

    /* Fill in every top level entry of the upper half, even the empty
     * ones.
     *
     * This looks like waste -- 256 page tables, a megabyte, most of
     * which will never hold a mapping. It buys something specific.
     * Every process gets its own top level table whose upper half is
     * copied from this one, and a copy only stays correct if the
     * original never changes afterwards. Anything the kernel maps later
     * -- heap growth, a new thread stack, a device -- then lands in a
     * table that already exists and is already shared, instead of
     * creating a top level entry that existing processes would never
     * see. That failure would appear as one process faulting on memory
     * another can read perfectly well, which is not a bug anybody wants
     * to chase. */
    u64 *root_v = table_at(root);
    for (u32 i = 256; i < 512; i++) {
        if (root_v[i] & PTE_PRESENT) continue;
        phys_addr t = pmm_alloc();
        if (t == PMM_NO_FRAME) panic("no memory to reserve the kernel half");
        root_v[i] = t | PTE_PRESENT | PTE_WRITE;
    }

    /* Everything the kernel needs now exists in the upper half. The
     * lower half of these tables was never filled in, so switching
     * removes the identity mapping in the same instruction. */
    __asm__ volatile ("movq %0, %%cr3" :: "r"(root) : "memory");

    enable_after_switch(&cpu);

    /* The span, not the sum of the pieces: ranges get rounded out to
     * 2 MiB boundaries and neighbours then share a page, so adding the
     * pieces up would count the same memory more than once. */
    kprintf("vmm:  own tables active, direct map covers %p-%p (%llu MiB), "
            "identity mapping dropped\n",
            (void *)EB_PHYSMAP_BASE, (void *)(EB_PHYSMAP_BASE + top),
            top >> 20);
}

/* The loader's page tables are dead once we are on our own. Nothing
 * else would ever reclaim them, and they are the better part of a
 * megabyte. */
void vmm_reclaim_loader_tables(const eb_boot_info *bi)
{
    if (!bi->pagetab_base || !bi->pagetab_size) return;
    pmm_free_contig(bi->pagetab_base, bi->pagetab_size / PAGE_SIZE);
}

/* ------------------------------------------------------------------ */
/* Checking that the tables say what we think they say                 */
/* ------------------------------------------------------------------ */

bool vmm_selftest(void)
{
    phys_addr pa;
    u64 flags;
    bool ok = true;

    /* Kernel code: present, executable, not writable. */
    if (!vmm_resolve((virt_addr)__kernel_start, &pa, &flags)) {
        kprintf("vmm:  kernel code is not mapped at all\n");
        ok = false;
    } else {
        if (flags & PTE_WRITE) {
            kprintf("vmm:  kernel code is writable\n");
            ok = false;
        }
        if (active.nx && (flags & PTE_NX)) {
            kprintf("vmm:  kernel code is marked non-executable\n");
            ok = false;
        }
        if (pa != kernel_virt_to_phys(__kernel_start)) {
            kprintf("vmm:  kernel code maps to %p, expected %p\n",
                    (void *)pa, (void *)kernel_virt_to_phys(__kernel_start));
            ok = false;
        }
    }

    /* Constants: neither writable nor executable. */
    if ((virt_addr)__rodata_end > (virt_addr)__rodata_start &&
        vmm_resolve((virt_addr)__rodata_start, NULL, &flags)) {
        if (flags & PTE_WRITE) {
            kprintf("vmm:  kernel constants are writable\n");
            ok = false;
        }
        if (active.nx && !(flags & PTE_NX)) {
            kprintf("vmm:  kernel constants are executable\n");
            ok = false;
        }
    }

    /* Data: writable, and above all not executable. */
    if (vmm_resolve((virt_addr)__bss_start, NULL, &flags)) {
        if (!(flags & PTE_WRITE)) {
            kprintf("vmm:  kernel data is read-only\n");
            ok = false;
        }
        if (active.nx && !(flags & PTE_NX)) {
            kprintf("vmm:  kernel data is executable -- W^X is not holding\n");
            ok = false;
        }
    }

    /* The direct map must not be executable either. */
    if (vmm_resolve((virt_addr)phys_to_virt(0x100000), NULL, &flags)) {
        if (active.nx && !(flags & PTE_NX)) {
            kprintf("vmm:  the direct map is executable\n");
            ok = false;
        }
    }

    /* And the identity mapping has to be gone: the lower half belongs
     * to user processes now, and nothing of the kernel's should be
     * reachable from an address a program could hold. */
    if (vmm_resolve(0x100000, NULL, NULL)) {
        kprintf("vmm:  the identity mapping is still in place\n");
        ok = false;
    }

    return ok;
}
