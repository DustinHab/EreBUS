#ifndef EB_XHCI_H
#define EB_XHCI_H

#include <eb/types.h>

/* USB, through the xHCI host controller every machine since about
 * 2012 has. Keyboards and mice are spoken to in the boot protocol and
 * feed the same queues the PS/2 driver fills, so the shell never
 * learns which wire a key came down; disks (sticks, card readers)
 * are registered with the block layer, where a stick with a store
 * partition can be the store. Everything else is seen and named but
 * not driven. */

/* Finds the controller, brings it up, walks its ports, and starts the
 * thread that listens. False when there is no controller. */
bool xhci_init(void);

bool xhci_present(void);
u32  xhci_keyboards(void);
u32  xhci_mice(void);
u32  xhci_disks(void);

#endif /* EB_XHCI_H */
