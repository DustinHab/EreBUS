/*
 * panic.c -- kontrollierter Abbruch.
 */
#include <eb/panic.h>
#include <eb/fmt.h>
#include <eb/fb.h>
#include <eb/io.h>

/* Verhindert eine Endlosschleife, falls die Ausgabe selbst abstuerzt. */
static bool in_panic;

void panic(const char *fmt, ...)
{
    if (in_panic) cpu_stop();
    in_panic = true;

    cpu_cli();

    /* Ein deutlich sichtbarer Balken am oberen Rand -- man soll auf
     * einen Blick erkennen, dass das System steht und nicht haengt. */
    if (fb_width() > 0) {
        fb_rect(0, 0, (i32)fb_width(), 4, RGB(220, 60, 60));
        fb_rect(0, (i32)fb_height() - 4, (i32)fb_width(), 4, RGB(220, 60, 60));
    }

    kprintf("\n");
    kprintf("=== ABBRUCH ===\n");

    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);

    kprintf("\nDas System steht. Neustart erforderlich.\n");
    cpu_stop();
}
