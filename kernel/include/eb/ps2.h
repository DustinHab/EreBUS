#ifndef EB_PS2_H
#define EB_PS2_H

#include <eb/types.h>

/* The PS/2 controller: keyboard on line 1, mouse on line 12.
 *
 * The queues here are the system's one door for keys and movements.
 * The controller fills them where a machine has one; the usb driver
 * fills the same queues from the xHCI controller's devices, speaking
 * scancode set 1 into ps2_feed_scancode so that the layout tables and
 * the modifier rules live in one place.
 */

/* Keys with no character get a code point of their own, above anything
 * a character could occupy. */
#define KEY_UP     0x110000u
#define KEY_DOWN   0x110001u
#define KEY_LEFT   0x110002u
#define KEY_RIGHT  0x110003u
#define KEY_HOME   0x110004u
#define KEY_END    0x110005u
#define KEY_PGUP   0x110006u
#define KEY_PGDN   0x110007u
#define KEY_DELETE 0x110008u
#define KEY_ESCAPE 27u
#define KEY_TAB     9u
#define KEY_ENTER  10u

typedef struct {
    u8   scancode;
    u32  codepoint;   /* zero for keys with no character */
    bool down;
    bool shift, ctrl, alt;
} key_event;

typedef struct {
    i32 dx, dy;       /* movement since the last packet, y already flipped */
    i32 dz;           /* wheel steps; positive rolls the page down */
    u8  buttons;      /* bit 0 left, bit 1 right, bit 2 middle */
} mouse_event;

void ps2_init(void);
bool ps2_keyboard_present(void);
bool ps2_mouse_present(void);

/* Take the next event, or false if there is none waiting. */
bool ps2_poll_key(key_event *out);
bool ps2_poll_mouse(mouse_event *out);

u64 ps2_key_count(void);
u64 ps2_mouse_count(void);

/* The same queues fed from elsewhere: a byte of scancode set 1, or a
 * movement with dy the screen's way. The usb driver speaks through
 * these, so the layout tables and the modifier rules live once. */
void ps2_feed_scancode(u8 code);
void ps2_feed_mouse(i32 dx, i32 dy, i32 dz, u8 buttons);

#endif /* EB_PS2_H */
