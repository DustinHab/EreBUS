/*
 * harden.c -- Laufzeitteile der Uebersetzerhaertung.
 *
 * -fstack-protector-strong legt vor jede Ruecksprungadresse einen
 * Waechterwert und prueft ihn beim Verlassen der Funktion. Stimmt er
 * nicht, wurde ueber einen Puffer hinausgeschrieben -- der klassische
 * Weg, den Rueckspruchzeiger zu uebernehmen. Der Uebersetzer erwartet
 * dafuer diese beiden Symbole.
 */
#include <eb/types.h>
#include <eb/panic.h>

/* Fester Wert. Sobald es einen Zufallszahlengeber gibt (RDRAND bzw.
 * RDSEED, spaetestens beim Einrichten der Prozesse), wird der Waechter
 * beim Start ueberschrieben -- ein vorhersagbarer Wert laesst sich sonst
 * beim Ueberschreiben einfach mitschreiben. */
u64 __stack_chk_guard = 0x5EC0DE00C0FFEE11ULL;

__attribute__((noreturn))
void __stack_chk_fail(void)
{
    panic("Stapelwaechter verletzt -- Pufferueberlauf im Kernel");
}
