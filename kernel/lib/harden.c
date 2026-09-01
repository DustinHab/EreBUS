/*
 * harden.c -- the runtime half of the compiler hardening.
 *
 * -fstack-protector-strong places a guard value in front of every
 * return address and checks it on the way out. A mismatch means
 * something wrote past the end of a buffer -- the classic route to
 * taking over the return pointer. The compiler expects these two
 * symbols to exist.
 */
#include <eb/types.h>
#include <eb/panic.h>
#include <eb/io.h>

/* Fixed for now. Once a random source is available (RDRAND or RDSEED,
 * at the latest when processes appear) the guard is overwritten at
 * start-up: a predictable value can simply be written back by whoever
 * is doing the overwriting, which makes it worthless. */
u64 __stack_chk_guard = 0x5EC0DE00C0FFEE11ULL;

/* Straight to the serial port, byte by byte: the stack this runs on
 * has just been proven rotten, so nothing here may lean on anything
 * with a buffer of its own. */
static void raw_say(const char *s)
{
    while (*s) outb(0x3F8, (u8)*s++);
}

static void raw_hex(u64 v)
{
    static const char hx[] = "0123456789abcdef";
    for (i32 i = 60; i >= 0; i -= 4) outb(0x3F8, (u8)hx[(v >> i) & 15]);
}

__attribute__((noreturn))
void __stack_chk_fail(void)
{
    raw_say("\r\nGUARD FAIL near rip ");
    raw_hex((u64)__builtin_return_address(0));
    raw_say("\r\n");
    panic("stack guard corrupted -- buffer overflow in the kernel");
}
