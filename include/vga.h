#ifndef VGA_H
#define VGA_H

#include "types.h"

/*
 * Screen interface: VGA text mode, 80 columns by 25 rows.
 *
 * There are VGA_SCREENS virtual screens. Each keeps its own content, write
 * position and colours. All output goes to the active screen, which is the
 * one shown on the display.
 */

#define VGA_WIDTH	80
#define VGA_HEIGHT	25
#define VGA_SCREENS	4

enum vga_color {
	VGA_BLACK,
	VGA_BLUE,
	VGA_GREEN,
	VGA_CYAN,
	VGA_RED,
	VGA_MAGENTA,
	VGA_BROWN,
	VGA_LIGHT_GREY,
	VGA_DARK_GREY,
	VGA_LIGHT_BLUE,
	VGA_LIGHT_GREEN,
	VGA_LIGHT_CYAN,
	VGA_LIGHT_RED,
	VGA_LIGHT_MAGENTA,
	VGA_YELLOW,
	VGA_WHITE,
};

/* Blank every screen (light grey on black) and show the first one. */
void	vga_init(void);

/*
 * Blank the active screen with its current background colour and move the
 * write position to the top left.
 */
void	vga_clear(void);

/* Set the colours used by the active screen from now on. */
void	vga_set_color(enum vga_color fg, enum vga_color bg);

/*
 * Write one character at the current position. '\n' starts a new line and
 * '\b' erases the previous character. Past the last row the screen scrolls
 * up by one line. The hardware cursor follows the write position.
 */
void	vga_putchar(char c);

/* Write a NUL-terminated string. */
void	vga_write(const char *s);

/* Show screen `index` (0 to VGA_SCREENS - 1) and direct output to it. */
void	vga_switch_screen(size_t index);

#endif
