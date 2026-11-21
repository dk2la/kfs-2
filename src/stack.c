#include "stack.h"
#include "printk.h"
#include "types.h"

/* The most lines the hexdump and the backtrace may take on a screen. */
#define DUMP_LINES_MAX	10
#define FRAMES_MAX	6
#define WORDS_PER_LINE	4

/* Defined in src/boot.s. The stack grows down from stack_top. */
extern uint8_t	stack_bottom[];
extern uint8_t	stack_top[];

/* What every function leaves at ebp: the caller's ebp, then where to return. */
struct frame {
	const struct frame	*caller;
	uint32_t		return_address;
};

static int on_stack(uintptr_t address)
{
	return address >= (uintptr_t)stack_bottom &&
	       address < (uintptr_t)stack_top;
}

static void dump_line(const uint8_t *line, const uint8_t *end)
{
	const uint8_t *p;
	size_t i;

	printk("%p:", (const void *)line);
	for (i = 0; i < WORDS_PER_LINE; i++) {
		p = line + i * sizeof(uint32_t);
		if (p < end)
			printk(" %08x", *(const uint32_t *)p);
		else
			printk("         ");
	}
	printk("  ");
	for (p = line; p < line + WORDS_PER_LINE * sizeof(uint32_t) &&
	     p < end; p++)
		printk("%c", *p >= ' ' && *p <= '~' ? *p : '.');
	printk("\n");
}

static void hexdump(const uint8_t *from, const uint8_t *end)
{
	size_t lines = 0;

	while (from < end && lines < DUMP_LINES_MAX) {
		dump_line(from, end);
		from += WORDS_PER_LINE * sizeof(uint32_t);
		lines++;
	}
	if (from < end)
		printk("... %u more bytes up to the top\n",
		       (uint32_t)(end - from));
}

static void backtrace(const struct frame *frame)
{
	size_t depth = 0;

	printk("call chain (frame, return address):\n");
	while (frame && on_stack((uintptr_t)frame) && depth < FRAMES_MAX) {
		printk(" #%u  ebp=%p  ret=%p\n", depth,
		       (const void *)frame, (void *)frame->return_address);
		frame = frame->caller;
		depth++;
	}
}

void print_kernel_stack(void)
{
	const struct frame *frame = __builtin_frame_address(0);
	uintptr_t esp;

	__asm__ volatile ("mov %%esp, %0" : "=r"(esp));
	printk("kernel stack: bottom=%p top=%p size=%u\n",
	       (void *)stack_bottom, (void *)stack_top,
	       (uint32_t)(stack_top - stack_bottom));
	printk("esp=%p ebp=%p used=%u bytes\n", (void *)esp,
	       (const void *)frame, (uint32_t)((uintptr_t)stack_top - esp));
	hexdump((const uint8_t *)esp, stack_top);
	backtrace(frame);
}

static __attribute__((noinline)) void stack_demo_b(uint32_t marker)
{
	volatile uint32_t local = marker;

	(void)local;
	print_kernel_stack();
}

static __attribute__((noinline)) void stack_demo_a(uint32_t marker)
{
	volatile uint32_t local = marker;

	(void)local;
	stack_demo_b(0xDEADBEEF);
}

void stack_demo(void)
{
	stack_demo_a(0xCAFEBABE);
}
