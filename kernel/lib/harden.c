/*
 * harden.c -- stack protector runtime: the guard value and the failure handler
 * required by -fstack-protector-strong.
 */
#include <eb/types.h>
#include <eb/panic.h>
#include <eb/io.h>
#include <eb/crypto.h>

/* The initial value is a placeholder used only until stack_guard_init
 * runs, very early in start-up, and replaces it with a random one from
 * the entropy pool. A guard that stayed at a value printed in the source
 * would be no guard at all. */
u64 __stack_chk_guard = 0x5EC0DE00C0FFEE11ULL;

/* Replaces the guard with a random value. Called once, before start-up
 * has called anything that will later check its canary against the new
 * value. It must not carry a canary of its own: it changes the guard and
 * then returns, and its own epilogue would compare the old saved canary
 * against the new global and fail. */
__attribute__((no_stack_protector))
void stack_guard_init(void)
{
    u64 g = 0;
    rand_bytes((u8 *)&g, sizeof(g));
    if (g) __stack_chk_guard = g;   /* never weaken it to zero */
}

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
