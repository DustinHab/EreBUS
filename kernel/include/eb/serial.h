#ifndef EB_SERIAL_H
#define EB_SERIAL_H

#include <eb/types.h>

/* Serielle Schnittstelle COM1, 115200 8N1.
 *
 * Das ist die Lebensader beim Entwickeln: sie funktioniert, bevor
 * irgendetwas anderes im System steht, sie funktioniert noch, wenn der
 * Bildschirm längst kaputtgeschrieben ist, und QEMU leitet sie direkt
 * ins Terminal um. */
bool serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);
bool serial_present(void);

#endif /* EB_SERIAL_H */
