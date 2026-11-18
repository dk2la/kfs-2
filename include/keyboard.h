#ifndef KEYBOARD_H
#define KEYBOARD_H

/*
 * PS/2 keyboard, US layout, read by polling the controller's I/O ports.
 *
 * A key is returned as an int: printable characters, '\n' (Enter) and '\b'
 * (Backspace) as their ASCII value, function keys as the KEY_F* values
 * below. Key releases and keys with no meaning here are never returned.
 */

#define KEY_NONE	0
#define KEY_F1		0x100
#define KEY_F2		0x101
#define KEY_F3		0x102
#define KEY_F4		0x103

/* Return the next key press, or KEY_NONE if nothing is waiting. */
int	keyboard_poll(void);

/* Wait for the next key press and return it. */
int	keyboard_read(void);

#endif
