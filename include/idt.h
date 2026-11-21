#ifndef IDT_H
#define IDT_H

#include "types.h"

/*
 * Interrupt Descriptor Table, for the 32 CPU exceptions only. Hardware
 * interrupts stay disabled: the keyboard is polled.
 */

/* What src/isr.s pushes before calling exception_handler(). */
struct registers {
	uint32_t	edi;
	uint32_t	esi;
	uint32_t	ebp;
	uint32_t	esp;		/* as pushed by pusha: before the push */
	uint32_t	ebx;
	uint32_t	edx;
	uint32_t	ecx;
	uint32_t	eax;
	uint32_t	number;		/* exception number, 0 to 31 */
	uint32_t	error_code;	/* 0 when the CPU pushes none */
	uint32_t	eip;		/* pushed by the CPU from here on */
	uint32_t	cs;
	uint32_t	eflags;
};

/* Build the table and load it with lidt. */
void	idt_init(void);

/* Called from src/isr.s for every exception. */
void	exception_handler(struct registers *regs);

#endif
