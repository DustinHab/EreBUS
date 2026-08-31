#ifndef EB_PANIC_H
#define EB_PANIC_H

#include <eb/types.h>

/* Bricht das System kontrolliert ab: Meldung auf die serielle
 * Schnittstelle und auf den Bildschirm, danach Stillstand.
 *
 * Grundsatz von Erebus: lieber sichtbar stehenbleiben als mit einem
 * unklaren Zustand weiterlaufen. Ein weiterlaufendes System mit
 * verletzten Annahmen ist genau das, was Angriffe brauchbar macht. */
__attribute__((noreturn, format(printf, 1, 2)))
void panic(const char *fmt, ...);

#endif /* EB_PANIC_H */
