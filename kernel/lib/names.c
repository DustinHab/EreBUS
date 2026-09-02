/*
 * names.c -- an address, read back to a name.
 *
 * The table's shape is shared with tools/mknames.py and with the
 * linker's kernel layout: u32 count, u32 letters (offset from the
 * table's start), count entries of { u64 addr; u32 name; u32 pad; }
 * sorted by address, then the letters, each name ended by zero.
 */
#include <eb/names.h>

extern const u8 __names_start[];
extern const u8 __names_end[];

static u32 rd32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static u64 rd64(const u8 *p)
{
    return (u64)rd32(p) | ((u64)rd32(p + 4) << 32);
}

u32 names_count(void)
{
    u64 n = (u64)(__names_end - __names_start);
    if (n < 8) return 0;
    u32 count = rd32(__names_start);
    if (8 + (u64)count * 16 > n) return 0;
    return count;
}

const char *names_of(u64 addr, u64 *off)
{
    const u8 *t = __names_start;
    u64 n = (u64)(__names_end - __names_start);
    u32 count = names_count();
    if (!count) return NULL;
    u32 letters = rd32(t + 4);
    if (letters >= n) return NULL;

    /* The last entry at or below the address. */
    u32 lo = 0, hi = count;
    while (lo < hi) {
        u32 mid = (lo + hi) / 2;
        if (rd64(t + 8 + (u64)mid * 16) <= addr) lo = mid + 1;
        else hi = mid;
    }
    if (lo == 0) return NULL;
    const u8 *e = t + 8 + (u64)(lo - 1) * 16;
    u64 base = rd64(e);
    u32 name = rd32(e + 8);
    if ((u64)letters + name >= n) return NULL;
    if (off) *off = addr - base;
    return (const char *)(t + letters + name);
}
