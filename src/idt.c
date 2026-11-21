#include "idt.h"
#include "gdt.h"
#include "panic.h"
#include "types.h"

#define IDT_ENTRIES	32
/* Present, ring 0, 32-bit interrupt gate. */
#define GATE_INTERRUPT	0x8E

#define EXCEPTION_BREAKPOINT	3
#define EXCEPTION_PAGE_FAULT	14

/* Page fault error code bits. */
#define FAULT_PROTECTION	0x1	/* the page was present */
#define FAULT_WRITE		0x2
#define FAULT_USER		0x4

struct idt_entry {
	uint16_t	offset_low;
	uint16_t	selector;
	uint8_t		zero;
	uint8_t		type;
	uint16_t	offset_high;
} __attribute__((packed));

struct idt_pointer {
	uint16_t	limit;
	uint32_t	base;
} __attribute__((packed));

_Static_assert(sizeof(struct idt_entry) == 8, "an IDT gate is 8 bytes");

/* src/isr.s */
extern const uint32_t	isr_table[IDT_ENTRIES];

static struct idt_entry	idt[IDT_ENTRIES];

static const char *const names[IDT_ENTRIES] = {
	"divide error", "debug", "non-maskable interrupt", "breakpoint",
	"overflow", "bound range exceeded", "invalid opcode",
	"device not available", "double fault", "coprocessor segment overrun",
	"invalid TSS", "segment not present", "stack-segment fault",
	"general protection fault", "page fault", "reserved",
	"x87 floating-point error", "alignment check", "machine check",
	"SIMD floating-point error", "virtualization exception",
	"control protection exception", "reserved", "reserved", "reserved",
	"reserved", "reserved", "reserved", "reserved", "reserved",
	"security exception", "reserved",
};

void idt_init(void)
{
	struct idt_pointer pointer;
	size_t i;

	for (i = 0; i < IDT_ENTRIES; i++) {
		idt[i].offset_low = isr_table[i] & 0xFFFF;
		idt[i].selector = GDT_KERNEL_CODE;
		idt[i].zero = 0;
		idt[i].type = GATE_INTERRUPT;
		idt[i].offset_high = isr_table[i] >> 16;
	}
	pointer.limit = sizeof(idt) - 1;
	pointer.base = (uint32_t)idt;
	__asm__ volatile ("lidt %0" : : "m"(pointer));
}

void exception_handler(struct registers *regs)
{
	uint32_t address;

	switch (regs->number) {
	case EXCEPTION_BREAKPOINT:
		/* A trap: eip already points after the int3. */
		oops("breakpoint exception at %p, execution goes on",
		     (void *)(regs->eip - 1));
		return;
	case EXCEPTION_PAGE_FAULT:
		/* CR2 holds the address whose access faulted. */
		__asm__ volatile ("mov %%cr2, %0" : "=r"(address));
		panic_registers(regs, "page fault %s %p: %s (%s mode)",
				regs->error_code & FAULT_WRITE ?
				"writing" : "reading", (void *)address,
				regs->error_code & FAULT_PROTECTION ?
				"page rights forbid it" : "page not mapped",
				regs->error_code & FAULT_USER ?
				"user" : "kernel");
	default:
		panic_registers(regs, "exception %u: %s",
				regs->number, names[regs->number % IDT_ENTRIES]);
	}
}
