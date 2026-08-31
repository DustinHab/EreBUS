#ifndef EB_PANIC_H
#define EB_PANIC_H

#include <eb/types.h>

/* Brings the system down in a controlled way: message to the serial
 * port and to the screen, then a halt.
 *
 * Erebus principle: stop visibly rather than continue in an unclear
 * state. A system still running on broken assumptions is exactly what
 * makes an attack useful. */
__attribute__((noreturn, format(printf, 1, 2)))
void panic(const char *fmt, ...);

#endif /* EB_PANIC_H */
