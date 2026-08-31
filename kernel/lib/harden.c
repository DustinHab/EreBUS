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

/* Fixed for now. Once a random source is available (RDRAND or RDSEED,
 * at the latest when processes appear) the guard is overwritten at
 * start-up: a predictable value can simply be written back by whoever
 * is doing the overwriting, which makes it worthless. */
u64 __stack_chk_guard = 0x5EC0DE00C0FFEE11ULL;

__attribute__((noreturn))
void __stack_chk_fail(void)
{
    panic("stack guard corrupted -- buffer overflow in the kernel");
}
