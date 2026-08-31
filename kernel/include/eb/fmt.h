#ifndef EB_FMT_H
#define EB_FMT_H

#include <eb/types.h>
#include <eb/varargs.h>

/* A sink takes single characters. The kernel writes to every registered
 * sink at once -- at first only the serial port, later the text area on
 * screen as well. */
typedef void (*kout_sink)(char c);

void kout_add_sink(kout_sink sink);

/* Hides every sink except the serial port -- for long diagnostics that
 * would otherwise bury the screen. */
void kout_mute_screen(bool mute);

/* Removes the screen sink permanently, for when the desktop takes over. */
void kout_detach_screen(void);

/* Once a time source is available, every log line is prefixed with the
 * uptime, the way dmesg does it. Pass NULL to turn it off again. */
void kout_set_clock(u64 (*nanoseconds)(void));

void kputc(char c);
void kputs(const char *s);

/* Supports: %s %c %d %i %u %x %X %p %%
 * Length modifiers l, ll and z are honoured (64 bit instead of 32).
 * Field width with optional zero padding and left alignment: %8x, %08x,
 * %-11s. Deliberately no floating point -- there is none in the kernel. */
void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void kvprintf(const char *fmt, va_list ap);

#endif /* EB_FMT_H */
