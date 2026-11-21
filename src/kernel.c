#include "gdt.h"
#include "keyboard.h"
#include "printk.h"
#include "stack.h"
#include "types.h"
#include "vga.h"

_Static_assert(VGA_SCREENS <= KEY_F4 - KEY_F1 + 1,
	       "every screen needs a function key to reach it");

/* Give every other screen its own colours and a title. */
static void prepare_screens(void)
{
	static const enum vga_color backgrounds[VGA_SCREENS] = {
		VGA_BLACK, VGA_BLUE, VGA_GREEN, VGA_RED,
	};
	size_t i;

	for (i = 1; i < VGA_SCREENS; i++) {
		vga_switch_screen(i);
		vga_set_color(VGA_WHITE, backgrounds[i]);
		vga_clear();
		printk("screen %u\n", i + 1);
	}
	vga_switch_screen(0);
}

/*
 * Print a typed key on the active screen. Backspace only erases what was
 * typed on that screen, never the text the kernel printed there.
 */
static void echo_key(size_t screen, char c)
{
	static size_t typed[VGA_SCREENS];

	if (c == '\b') {
		if (typed[screen] == 0)
			return;
		typed[screen]--;
	} else {
		typed[screen]++;
	}
	vga_putchar(c);
}

/* Called from _start (src/boot.s) once the stack and the GDT are set up. */
void kmain(void)
{
	size_t screen = 0;
	int key;

	vga_init();
	prepare_screens();

	vga_write("42\n\n");
	gdt_print();
	printk("\nF1 GDT, F2 kernel stack, F3 and F4 free typing\n");

	vga_switch_screen(1);
	stack_demo();
	vga_switch_screen(0);

	for (;;) {
		key = keyboard_read();
		if (key >= KEY_F1 && key < KEY_F1 + VGA_SCREENS) {
			screen = (size_t)(key - KEY_F1);
			vga_switch_screen(screen);
		} else {
			echo_key(screen, (char)key);
		}
	}
}
