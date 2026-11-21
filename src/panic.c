#include "panic.h"
#include "idt.h"
#include "printk.h"
#include "stdarg.h"
#include "types.h"
#include "vga.h"

/* Attribute bytes: background in the high nibble, foreground in the low. */
#define ATTR_FATAL	((VGA_RED << 4) | VGA_WHITE)
#define ATTR_OOPS	((VGA_BROWN << 4) | VGA_WHITE)

static unsigned int	oopses;

static void print_registers(const struct registers *regs)
{
	uint32_t cr0;
	uint32_t cr2;
	uint32_t cr3;

	__asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
	__asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
	__asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
	printk(" eip=%08x cs=%04x eflags=%08x error=%08x\n",
	       regs->eip, regs->cs, regs->eflags, regs->error_code);
	printk(" eax=%08x ebx=%08x ecx=%08x edx=%08x\n",
	       regs->eax, regs->ebx, regs->ecx, regs->edx);
	printk(" esi=%08x edi=%08x ebp=%08x esp=%08x\n",
	       regs->esi, regs->edi, regs->ebp, regs->esp);
	printk(" cr0=%08x cr2=%08x cr3=%08x\n", cr0, cr2, cr3);
}

static void halt(void)
{
	for (;;)
		__asm__ volatile ("cli; hlt");
}

static void fatal(const struct registers *regs, const char *fmt, va_list args)
{
	__asm__ volatile ("cli");
	vga_set_attr(ATTR_FATAL);
	printk("\nKERNEL PANIC: ");
	vprintk(fmt, args);
	printk("\n");
	if (regs)
		print_registers(regs);
	printk("System halted.\n");
	halt();
}

void panic(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	fatal(NULL, fmt, args);
	for (;;)
		;
}

void panic_registers(const struct registers *regs, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	fatal(regs, fmt, args);
	for (;;)
		;
}

void oops(const char *fmt, ...)
{
	uint8_t attr = vga_get_attr();
	va_list args;

	oopses++;
	vga_set_attr(ATTR_OOPS);
	printk("oops (not fatal): ");
	va_start(args, fmt);
	vprintk(fmt, args);
	va_end(args);
	vga_set_attr(attr);
	printk("\n");
}

unsigned int oops_count(void)
{
	return oopses;
}
