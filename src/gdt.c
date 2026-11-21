#include "gdt.h"
#include "printk.h"
#include "string.h"
#include "types.h"

/* Access byte: present, descriptor type "code or data", then the type. */
#define ACCESS_KERNEL_CODE	0x9A	/* ring 0, execute/read */
#define ACCESS_KERNEL_DATA	0x92	/* ring 0, read/write */
#define ACCESS_USER_CODE	0xFA	/* ring 3, execute/read */
#define ACCESS_USER_DATA	0xF2	/* ring 3, read/write */

/* Flags: limit counted in 4 KiB pages, 32-bit segment. */
#define FLAGS_FLAT		0xC
#define LIMIT_FLAT		0xFFFFF

static void set_entry(struct gdt_entry *entry, uint32_t base, uint32_t limit,
		      uint8_t access, uint8_t flags)
{
	entry->limit_low = limit & 0xFFFF;
	entry->base_low = base & 0xFFFF;
	entry->base_mid = (base >> 16) & 0xFF;
	entry->access = access;
	entry->granularity = (uint8_t)((flags << 4) | ((limit >> 16) & 0x0F));
	entry->base_high = (base >> 24) & 0xFF;
}

void gdt_init(void)
{
	struct gdt_entry table[GDT_ENTRIES];
	struct gdt_ptr ptr;

	set_entry(&table[0], 0, 0, 0, 0);
	set_entry(&table[1], 0, LIMIT_FLAT, ACCESS_KERNEL_CODE, FLAGS_FLAT);
	set_entry(&table[2], 0, LIMIT_FLAT, ACCESS_KERNEL_DATA, FLAGS_FLAT);
	set_entry(&table[3], 0, LIMIT_FLAT, ACCESS_KERNEL_DATA, FLAGS_FLAT);
	set_entry(&table[4], 0, LIMIT_FLAT, ACCESS_USER_CODE, FLAGS_FLAT);
	set_entry(&table[5], 0, LIMIT_FLAT, ACCESS_USER_DATA, FLAGS_FLAT);
	set_entry(&table[6], 0, LIMIT_FLAT, ACCESS_USER_DATA, FLAGS_FLAT);

	memcpy((void *)GDT_ADDRESS, table, sizeof(table));
	ptr.limit = sizeof(table) - 1;
	ptr.base = GDT_ADDRESS;
	gdt_flush(&ptr);
}

void gdt_print(void)
{
	static const char *const names[GDT_ENTRIES] = {
		"null", "kernel code", "kernel data", "kernel stack",
		"user code", "user data", "user stack",
	};
	const struct gdt_entry *entry;
	struct gdt_ptr ptr;
	uint16_t cs;
	uint16_t ds;
	uint16_t ss;
	uint32_t count;
	uint32_t dpl;
	uint32_t i;

	__asm__ volatile ("sgdt %0" : "=m"(ptr));
	__asm__ volatile ("mov %%cs, %0" : "=r"(cs));
	__asm__ volatile ("mov %%ds, %0" : "=r"(ds));
	__asm__ volatile ("mov %%ss, %0" : "=r"(ss));
	count = ((uint32_t)ptr.limit + 1) / sizeof(*entry);

	printk("GDT: base=0x%08x limit=0x%04x (%u entries)\n",
	       ptr.base, ptr.limit, count);
	printk("registers: CS=0x%02x DS=0x%02x SS=0x%02x\n\n", cs, ds, ss);
	printk("sel   base        limit    access  flags  DPL  segment\n");
	entry = (const struct gdt_entry *)ptr.base;
	for (i = 0; i < count; i++, entry++) {
		dpl = (entry->access >> 5) & 3;
		printk("0x%02x  0x%02x%02x%04x  0x%x%04x  0x%02x    0x%x    %u    %s\n",
		       i * 8 + dpl,
		       entry->base_high, entry->base_mid, entry->base_low,
		       entry->granularity & 0x0F, entry->limit_low,
		       entry->access, entry->granularity >> 4, dpl,
		       i < GDT_ENTRIES ? names[i] : "?");
	}
}
