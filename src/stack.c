#include "stack.h"
#include "printk.h"
#include "types.h"

/* The kernel stack, reserved in src/boot.s. It grows down from stack_top. */
extern char	stack_bottom[];
extern char	stack_top[];

#define BYTES_PER_LINE	16
/* Hexdump lines that still leave room for the rest of the dump on screen. */
#define DUMP_MAX_LINES	14
/* Guard against a corrupted ebp chain looping forever. */
#define BACKTRACE_MAX	16

static int on_stack(uintptr_t addr, size_t size)
{
	return addr >= (uintptr_t)stack_bottom &&
	       addr + size <= (uintptr_t)stack_top;
}

static char printable(uint8_t c)
{
	return c >= 0x20 && c < 0x7F ? (char)c : '.';
}

/* One line: address, four 32-bit words, then the same 16 bytes as ASCII. */
static void dump_line(uintptr_t addr)
{
	const uint32_t *words = (const uint32_t *)addr;
	const uint8_t *bytes = (const uint8_t *)addr;
	size_t i;

	printk("%p: %08x %08x %08x %08x  ", (void *)addr,
	       words[0], words[1], words[2], words[3]);
	for (i = 0; i < BYTES_PER_LINE; i++)
		printk("%c", printable(bytes[i]));
	printk("\n");
}

/*
 * Dump from `esp` up to stack_top. esp is 4-byte aligned, so the first
 * line may start below it, inside the dumping function's own frame: that
 * memory is still on the stack and safe to read.
 */
static void dump_stack(uintptr_t esp)
{
	uintptr_t top = (uintptr_t)stack_top;
	uintptr_t addr = esp & ~(uintptr_t)(BYTES_PER_LINE - 1);
	size_t lines = 0;

	printk("hexdump, esp to stack_top:\n");
	while (addr < top && lines < DUMP_MAX_LINES) {
		dump_line(addr);
		addr += BYTES_PER_LINE;
		lines++;
	}
	if (addr < top)
		printk("... %u more bytes up to stack_top\n", top - addr);
}

/*
 * Each frame starts with the caller's saved ebp, followed by the return
 * address into the caller. _start sets ebp to 0 before calling C, which
 * ends the chain.
 */
static void backtrace(uintptr_t ebp)
{
	const uint32_t *frame;
	size_t depth = 0;

	printk("backtrace (nm -n build/kernel.bin to name return addresses):\n");
	while (ebp && depth < BACKTRACE_MAX && on_stack(ebp, 8)) {
		frame = (const uint32_t *)ebp;
		printk("  #%u  frame %p  return %p\n", depth, (void *)ebp,
		       (void *)(uintptr_t)frame[1]);
		/* The chain goes up the stack: a lower address means garbage. */
		if (frame[0] <= ebp)
			break;
		ebp = frame[0];
		depth++;
	}
}

void print_kernel_stack(void)
{
	uintptr_t esp;
	uintptr_t ebp;

	__asm__ volatile ("mov %%esp, %0" : "=r"(esp));
	__asm__ volatile ("mov %%ebp, %0" : "=r"(ebp));

	printk("kernel stack: bottom=%p top=%p size=%u\n",
	       (void *)stack_bottom, (void *)stack_top,
	       (uintptr_t)stack_top - (uintptr_t)stack_bottom);
	printk("esp=%p ebp=%p in use=%u bytes\n", (void *)esp, (void *)ebp,
	       (uintptr_t)stack_top - esp);
	if (!on_stack(esp, 0)) {
		printk("esp is outside the kernel stack\n");
		return;
	}
	dump_stack(esp);
	backtrace(ebp);
}

static void __attribute__((noinline)) stack_demo_b(uint32_t magic,
						   const char *tag)
{
	(void)magic;
	(void)tag;
	print_kernel_stack();
}

void __attribute__((noinline)) stack_demo_a(uint32_t magic)
{
	/* A local string, to show up in the ASCII column of the hexdump. */
	char tag[8] = "kfs-2!";

	(void)magic;
	stack_demo_b(0xDEADBEEF, tag);
}
