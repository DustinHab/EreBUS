/*
 * panic.c -- controlled shutdown on an unrecoverable error.
 */
#include <eb/panic.h>
#include <eb/fmt.h>
#include <eb/io.h>

/* Guards against an endless loop if the output path itself is broken. */
static bool in_panic;

void panic(const char *fmt, ...)
{
    if (in_panic) cpu_stop();
    in_panic = true;

    cpu_cli();

    kprintf("\nkern: panic: ");

    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);

    kprintf("\nkern: system halted\n");
    cpu_stop();
}
