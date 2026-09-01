#ifndef EB_TIME_H
#define EB_TIME_H

#include <eb/types.h>

/* Measures how fast the time stamp counter runs and starts the clock.
 * Takes about 50 ms, because that is how long the measurement runs.
 * Returns false if the result was implausible, in which case there are
 * no timestamps rather than wrong ones. */
bool time_init(void);

u64 time_ns(void);      /* nanoseconds since time_init */
u64 time_tsc_hz(void);

/* The clock on the wall, from the CMOS RTC read once at start-up and
 * carried forward by the counter. */
void time_read_rtc(void);
void time_wall(u32 *h, u32 *m, u32 *s);

/* Sets the wall clock -- how a time server's answer lands. */
void time_set_wall(u32 h, u32 m, u32 s);

/* Tells this boot apart from any other, to the second. */
u64  time_boot_stamp(void);  /* 0 if the measurement failed */

/* Periodic tick on interrupt line 0. Not needed for timestamps -- the
 * counter covers those -- but it is what will drive preemption, and it
 * doubles as a cross-check that interrupts really are arriving. */
void pit_init(u32 hz);
u64  pit_ticks(void);
u32  pit_hz(void);

#endif /* EB_TIME_H */
