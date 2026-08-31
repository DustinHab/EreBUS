#ifndef EB_SERIAL_H
#define EB_SERIAL_H

#include <eb/types.h>

/* Serial port COM1, 115200 8N1.
 *
 * This is the lifeline during development: it works before anything
 * else in the system does, it still works when the framebuffer has long
 * been scribbled over, and QEMU pipes it straight into the terminal. */
bool serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);
bool serial_present(void);

#endif /* EB_SERIAL_H */
