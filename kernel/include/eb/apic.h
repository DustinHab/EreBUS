#ifndef EB_APIC_H
#define EB_APIC_H

#include <eb/types.h>

/* The processor's local interrupt controller. The 8259 pair stays in
 * charge of the legacy lines (timer, keyboard, mouse, and a card's
 * interrupt pin when it has no better way); the local controller is
 * what a message-signalled interrupt from a pci device lands on, and it
 * is told when such an interrupt has been handled. Nothing here needs
 * an acpi table: the controller's address is in a model register. */

/* Switches it on in whatever mode the firmware left it (xapic through
 * its memory window, x2apic through registers), with the legacy lines
 * still passing through. False when the processor has none. */
bool lapic_init(void);

bool lapic_present(void);
u32  lapic_id(void);
void lapic_eoi(void);

/* What a pci device writes to raise vector v here: the address and the
 * data word of the message. */
u32  lapic_msi_address(void);
u32  lapic_msi_data(u8 vector);

#endif /* EB_APIC_H */
