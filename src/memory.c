#include "memory.h"
#include "heap.h"
#include "multiboot.h"
#include "paging.h"
#include "panic.h"
#include "pmm.h"
#include "printk.h"
#include "string.h"
#include "types.h"

#define CR0_WP	0x00010000
#define CR0_PG	0x80000000

/* What the report allocates, to have something to show. */
#define DEMO_KMALLOC	100
#define DEMO_VMALLOC	10000			/* three pages */
#define DEMO_USER_PAGE	USER_SPACE_START
#define DEMO_UNMAPPED	0x50000000u

void memory_init(uint32_t magic, const struct multiboot_info *info)
{
	if (magic != MULTIBOOT_BOOT_MAGIC)
		panic("not started by a Multiboot loader (eax=0x%08x)", magic);
	pmm_init(info);
	paging_init();
	heap_init();
}

/* "P W U" for a mapped address, "-" where a right is missing. */
static void print_rights(uintptr_t addr)
{
	uint32_t flags;

	if (!page_get(addr, NULL, &flags)) {
		printk("%p not mapped", (void *)addr);
		return;
	}
	printk("%p P %c %c", (void *)addr, flags & PAGE_WRITE ? 'W' : '-',
	       flags & PAGE_USER ? 'U' : '-');
}

/* Fill `size` bytes with a pattern and read them back. */
static int write_read(void *ptr, size_t size, uint8_t seed)
{
	uint8_t *bytes = ptr;
	size_t i;

	for (i = 0; i < size; i++)
		bytes[i] = (uint8_t)(seed + i);
	for (i = 0; i < size; i++)
		if (bytes[i] != (uint8_t)(seed + i))
			return 0;
	return 1;
}

static void print_layout(void)
{
	uint32_t cr0 = paging_cr0();

	printk("RAM: %u MiB usable = %u frames of 4 KiB, %u free\n",
	       pmm_total_frames() / 256, pmm_total_frames(),
	       pmm_free_frames());
	printk("CR0=0x%08x paging %s, write protect %s  CR3=0x%08x\n", cr0,
	       cr0 & CR0_PG ? "on" : "off", cr0 & CR0_WP ? "on" : "off",
	       paging_cr3());
	printk("kernel image %p-%p, read-only up to %p\n",
	       (void *)kernel_start, (void *)kernel_end,
	       (void *)kernel_data_start);
	printk("kernel space %p-%p, RAM mapped 1:1 up to %p\n",
	       (void *)KERNEL_SPACE_START, (void *)(KERNEL_SPACE_END - 1),
	       (void *)pmm_top());
	printk("  kmalloc heap from %p, vmalloc area from %p\n",
	       (void *)kheap_start(), (void *)vheap_start());
	printk("user space   %p-%p\n", (void *)USER_SPACE_START,
	       (void *)USER_SPACE_LAST);
}

void memory_report(void)
{
	size_t free_before = pmm_free_frames();
	uintptr_t phys;
	uintptr_t addr;
	void *k;
	void *v;
	void *user;
	int ok;

	print_layout();

	k = kmalloc(DEMO_KMALLOC);
	page_get((uintptr_t)k, &phys, NULL);
	printk("kmalloc(%u) = %p ksize=%u physical=%p kbrk(0)=%p\n",
	       DEMO_KMALLOC, k, ksize(k), (void *)phys, kbrk(0));
	ok = k && write_read(k, DEMO_KMALLOC, 0x11);

	v = vmalloc(DEMO_VMALLOC);
	printk("vmalloc(%u) = %p vsize=%u vbrk(0)=%p\n",
	       DEMO_VMALLOC, v, vsize(v), vbrk(0));
	printk("  its pages:");
	for (addr = PAGE_ALIGN_DOWN(v); addr < (uintptr_t)vbrk(0);
	     addr += PAGE_SIZE) {
		page_get(addr, &phys, NULL);
		printk(" %p>%p", (void *)addr, (void *)phys);
	}
	printk("\n");
	ok = ok && v && write_read(v, DEMO_VMALLOC, 0x22);

	user = page_create(DEMO_USER_PAGE, PAGE_WRITE | PAGE_USER);
	page_get(DEMO_USER_PAGE, &phys, NULL);
	printk("page_create(user) = %p > frame %p\n", user, (void *)phys);
	ok = ok && user && write_read(user, PAGE_SIZE, 0x33);

	printk("rights: ");
	print_rights((uintptr_t)kernel_start);
	printk(", ");
	print_rights((uintptr_t)k);
	printk("\n        ");
	print_rights((uintptr_t)v);
	printk(", ");
	print_rights(DEMO_USER_PAGE);
	printk("\n        ");
	print_rights(DEMO_UNMAPPED);
	printk("\n");
	printk("write and read back (kmalloc, vmalloc, user page): %s\n",
	       ok ? "ok" : "FAILED");

	kfree(k);
	vfree(v);
	page_free(DEMO_USER_PAGE);
	printk("after kfree, vfree, page_free: %u frames free (%s)\n",
	       pmm_free_frames(), pmm_free_frames() == free_before ?
	       "all given back" : "LEAK");
}
