#include "gdt.h"
#include "printk.h"
#include "string.h"
#include "types.h"

/* One segment descriptor, in the layout the CPU expects. */
struct gdt_entry {
	uint16_t	limit_low;	/* limit, bits 0-15 */
	uint16_t	base_low;	/* base, bits 0-15 */
	uint8_t		base_mid;	/* base, bits 16-23 */
	uint8_t		access;		/* present, DPL, type */
	uint8_t		flags_limit;	/* flags (high nibble), limit bits 16-19 */
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
 * Access byte: present (0x80), DPL in bits 5-6, code/data descriptor
 * (0x10), then the type: 0xA = code, execute/read; 0x2 = data, read/write.
 * The CPU sets bit 0 (accessed) the first time it loads a segment, so the
 * dump shows 0x93 for the kernel data and stack segments.
 * Stack segments are plain read/write data: an expand-down segment with
 * base 0 and a 4 GiB limit would describe an empty range.
 */
#define ACCESS_KERNEL_CODE	0x9A
#define ACCESS_KERNEL_DATA	0x92
#define ACCESS_USER_CODE	0xFA
#define ACCESS_USER_DATA	0xF2

/* 4 KiB granularity (0x8) and 32-bit segment (0x4). */
#define FLAGS_FLAT		0xC
/* With 4 KiB granularity, 0xFFFFF pages cover the full 4 GiB. */
#define LIMIT_FLAT		0xFFFFF

/* A selector is the descriptor index times 8, plus the RPL in bits 0-1. */
#define INDEX(selector)		((selector) >> 3)

/* src/gdt_flush.s: lgdt, far jump to reload CS, reload the data registers. */
void	gdt_flush(const struct gdt_ptr *gdtr);

static const char *const segment_names[GDT_ENTRIES] = {
	"null",
	"kernel code",
	"kernel data",
	"kernel stack",
	"user code",
	"user data",
	"user stack",
};

static struct gdt_entry make_entry(uint32_t base, uint32_t limit,
				   uint8_t access, uint8_t flags)
{
	struct gdt_entry entry;

	entry.limit_low = (uint16_t)(limit & 0xFFFF);
	entry.base_low = (uint16_t)(base & 0xFFFF);
	entry.base_mid = (uint8_t)((base >> 16) & 0xFF);
	entry.access = access;
	entry.flags_limit = (uint8_t)((flags << 4) | ((limit >> 16) & 0xF));
	entry.base_high = (uint8_t)((base >> 24) & 0xFF);
	return entry;
}

void gdt_init(void)
{
	struct gdt_entry table[GDT_ENTRIES];
	struct gdt_ptr gdtr;

	table[0] = make_entry(0, 0, 0, 0);
	table[INDEX(GDT_KERNEL_CODE)] =
		make_entry(0, LIMIT_FLAT, ACCESS_KERNEL_CODE, FLAGS_FLAT);
	table[INDEX(GDT_KERNEL_DATA)] =
		make_entry(0, LIMIT_FLAT, ACCESS_KERNEL_DATA, FLAGS_FLAT);
	table[INDEX(GDT_KERNEL_STACK)] =
		make_entry(0, LIMIT_FLAT, ACCESS_KERNEL_DATA, FLAGS_FLAT);
	table[INDEX(GDT_USER_CODE)] =
		make_entry(0, LIMIT_FLAT, ACCESS_USER_CODE, FLAGS_FLAT);
	table[INDEX(GDT_USER_DATA)] =
		make_entry(0, LIMIT_FLAT, ACCESS_USER_DATA, FLAGS_FLAT);
	table[INDEX(GDT_USER_STACK)] =
		make_entry(0, LIMIT_FLAT, ACCESS_USER_DATA, FLAGS_FLAT);

	memcpy((void *)GDT_ADDRESS, table, sizeof(table));
	gdtr.limit = sizeof(table) - 1;
	gdtr.base = GDT_ADDRESS;
	gdt_flush(&gdtr);
}

void gdt_print(void)
{
	struct gdt_ptr gdtr;
	const struct gdt_entry *table;
	const struct gdt_entry *e;
	uint32_t count;
	uint32_t i;
	uint32_t base;
	uint32_t limit;
	uint32_t dpl;

	/* Ask the CPU where its table is, rather than trusting our constant. */
	__asm__ volatile ("sgdt %0" : "=m"(gdtr));
	table = (const struct gdt_entry *)(uintptr_t)gdtr.base;
	count = ((uint32_t)gdtr.limit + 1) / sizeof(struct gdt_entry);

	printk("GDTR: base=%p limit=0x%04x (%u descriptors)\n",
	       (void *)(uintptr_t)gdtr.base, gdtr.limit, count);
	printk("  sel   base      limit  access  flags  DPL  segment\n");
	for (i = 0; i < count; i++) {
		e = &table[i];
		base = (uint32_t)e->base_low | (uint32_t)e->base_mid << 16 |
		       (uint32_t)e->base_high << 24;
		limit = (uint32_t)e->limit_low |
			(uint32_t)(e->flags_limit & 0xF) << 16;
		dpl = (e->access >> 5) & 3;
		printk("  0x%02x  %08x  %05x  0x%02x    0x%x    %u    %s\n",
		       i * 8 | dpl, base, limit, e->access,
		       e->flags_limit >> 4, dpl,
		       i < GDT_ENTRIES ? segment_names[i] : "?");
	}
}
