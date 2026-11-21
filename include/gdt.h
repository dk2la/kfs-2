#ifndef GDT_H
#define GDT_H

#include "types.h"

/*
 * Global Descriptor Table.
 *
 * Seven flat segments (base 0, limit 4 GiB): the null descriptor the CPU
 * requires, then code, data and stack for the kernel (ring 0) and for user
 * programs (ring 3). The table lives at GDT_ADDRESS.
 */

#define GDT_ADDRESS	0x00000800
#define GDT_ENTRIES	7

/* Selectors: index * 8, plus the requested privilege level for ring 3. */
#define GDT_KERNEL_CODE	0x08
#define GDT_KERNEL_DATA	0x10
#define GDT_KERNEL_STACK	0x18
#define GDT_USER_CODE	0x23
#define GDT_USER_DATA	0x2B
#define GDT_USER_STACK	0x33

/* One segment descriptor, in the layout the CPU reads. */
struct gdt_entry {
	uint16_t	limit_low;	/* limit, bits 0-15 */
	uint16_t	base_low;	/* base, bits 0-15 */
	uint8_t		base_mid;	/* base, bits 16-23 */
	uint8_t		access;		/* present, DPL, type */
	uint8_t		granularity;	/* flags (high 4 bits), limit 16-19 */
	uint8_t		base_high;	/* base, bits 24-31 */
} __attribute__((packed));

/* The operand of lgdt and sgdt. */
struct gdt_ptr {
	uint16_t	limit;		/* size of the table in bytes, minus 1 */
	uint32_t	base;		/* linear address of the table */
} __attribute__((packed));

_Static_assert(sizeof(struct gdt_entry) == 8, "a descriptor is 8 bytes");
_Static_assert(sizeof(struct gdt_ptr) == 6, "the GDTR operand is 6 bytes");

/*
 * Build the table, copy it to GDT_ADDRESS, load it and reload every segment
 * register. Called from _start before any other C code.
 */
void	gdt_init(void);

/* Print the table the CPU is using, read back through sgdt. */
void	gdt_print(void);

/* src/gdt_flush.s: lgdt, then reload CS, DS, ES, FS, GS and SS. */
void	gdt_flush(const struct gdt_ptr *ptr);

#endif
