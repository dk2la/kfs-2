#include "gdt.h"
#include "heap.h"
#include "idt.h"
#include "keyboard.h"
#include "memory.h"
#include "panic.h"
#include "printk.h"
#include "stack.h"
#include "types.h"
#include "vga.h"

/* What each screen shows at boot. */
#define SCREEN_GDT	0	/* F1: 42 and the GDT dump */
#define SCREEN_STACK	1	/* F2: the kernel stack dump */
#define SCREEN_MEMORY	2	/* F3: the memory report */

/* An address in user space that nothing maps, for the page fault demo. */
#define UNMAPPED_ADDRESS	0x50000000u

/* Recognisable argument for the stack demo, easy to find in the hexdump. */
#define STACK_DEMO_MAGIC	0xCAFEBABE

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
		if (i == SCREEN_STACK)
			printk("screen %u - kernel stack\n", i + 1);
		else if (i == SCREEN_MEMORY)
			printk("screen %u - memory\n", i + 1);
		else
			printk("screen %u - type something\n", i + 1);
	}
	vga_switch_screen(SCREEN_GDT);
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

/*
 * Panic demos, on the active screen. F5 and F6 are not fatal: the kernel
 * reports and goes on. F7 and F8 are page faults in kernel code: fatal.
 */
static void panic_demo(int key)
{
	void *block;
	void *other;

	switch (key) {
	case KEY_F5:
		/* `other` keeps the heap alive while `block` is freed twice. */
		block = kmalloc(32);
		other = kmalloc(32);
		kfree(block);
		kfree(block);
		kfree(other);
		break;
	case KEY_F6:
		__asm__ volatile ("int3");
		break;
	case KEY_F7:
		printk("reading %p, which is not mapped...\n",
		       (void *)UNMAPPED_ADDRESS);
		(void)*(volatile uint32_t *)UNMAPPED_ADDRESS;
		break;
	case KEY_F8:
		printk("writing to %p, read-only kernel code...\n",
		       (void *)kernel_start);
		*(volatile uint8_t *)kernel_start = 0;
		break;
	}
}

/*
 * Called from _start (src/boot.s), once the stack is set up and our GDT is
 * loaded, with what the Multiboot loader left in eax and ebx.
 */
void kmain(uint32_t magic, const struct multiboot_info *info)
{
	size_t screen = 0;
	int key;

	vga_init();
	idt_init();
	memory_init(magic, info);
	printk("42\n");
	gdt_print();
	printk("F1 GDT, F2 kernel stack, F3 memory, F4 free typing\n");
	printk("F5 oops, F6 breakpoint (both go on), "
	       "F7 page fault, F8 write to read-only (both halt)\n");
	prepare_screens();

	/* Called from here, so that kmain is one of the frames shown. */
	vga_switch_screen(SCREEN_STACK);
	stack_demo_a(STACK_DEMO_MAGIC);
	vga_switch_screen(SCREEN_MEMORY);
	memory_report();
	vga_switch_screen(SCREEN_GDT);

	for (;;) {
		key = keyboard_read();
		if (key >= KEY_F1 && key < KEY_F1 + VGA_SCREENS) {
			screen = (size_t)(key - KEY_F1);
			vga_switch_screen(screen);
		} else if (key >= KEY_F5 && key <= KEY_F8) {
			panic_demo(key);
		} else {
			echo_key(screen, (char)key);
		}
	}
}
