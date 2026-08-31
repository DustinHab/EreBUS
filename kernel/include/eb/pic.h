#ifndef EB_PIC_H
#define EB_PIC_H

#include <eb/types.h>

/* The 8259 pair from 1976. Every x86 machine still has it, or emulates
 * it, which is why it comes first: the local APIC is the better answer
 * and will replace this, but the PIC works everywhere without needing
 * ACPI tables parsed first.
 *
 * By default the chips deliver vectors 0..15, which collide with the
 * processor's own exception vectors -- a timer interrupt would look
 * like a double fault. So the first thing to do is move them. */
void pic_init(u8 offset_master, u8 offset_slave);

void pic_set_mask(u8 irq, bool masked);
void pic_mask_all(void);
void pic_eoi(u8 irq);

/* Lines 7 and 15 can fire without a device behind them, caused by
 * electrical noise on the request line. Acknowledging such an interrupt
 * as though it were real confuses the chip, so it has to be told
 * apart. */
bool pic_spurious(u8 irq);

#endif /* EB_PIC_H */
