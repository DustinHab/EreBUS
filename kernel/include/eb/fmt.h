#ifndef EB_FMT_H
#define EB_FMT_H

#include <eb/types.h>
#include <eb/varargs.h>

/* Eine Senke nimmt einzelne Zeichen entgegen. Der Kernel schreibt an
 * alle angemeldeten Senken gleichzeitig -- anfangs nur die serielle
 * Schnittstelle, später zusätzlich der Textbereich auf dem Bildschirm. */
typedef void (*kout_sink)(char c);

void kout_add_sink(kout_sink sink);

/* Blendet alle Senken ausser der seriellen aus -- fuer lange Diagnosen,
 * die den Bildschirm sonst vollschreiben wuerden. */
void kout_mute_screen(bool mute);

void kputc(char c);
void kputs(const char *s);

/* Unterstützt: %s %c %d %i %u %x %X %p %%
 * Längenangaben l, ll, z werden beachtet (64 Bit statt 32).
 * Feldbreite mit optionaler Null-Auffüllung: %8x, %08x, %4u.
 * Bewusst kein Fließkomma -- im Kernel gibt es keins. */
void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void kvprintf(const char *fmt, va_list ap);

#endif /* EB_FMT_H */
