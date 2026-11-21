#include "paging.h"
#include "memory.h"
#include "panic.h"
#include "pmm.h"
#include "string.h"
#include "types.h"

#define ENTRIES		1024
#define DIR_INDEX(virt)	((virt) >> 22)
#define TABLE_INDEX(virt)	(((virt) >> 12) & 0x3FF)

#define CR0_WP		0x00010000	/* read-only pages bind the kernel too */
#define CR0_PG		0x80000000	/* paging */

/*
 * The directory is part of the kernel image. Page tables are frames taken
 * from the physical allocator; they are reached through the direct map,
 * where a frame's virtual address is its physical address.
 */
static uint32_t	page_directory[ENTRIES] __attribute__((aligned(PAGE_SIZE)));

static int in_user_space(uintptr_t virt)
{
	return virt >= USER_SPACE_START;
}

static void flush_page(uintptr_t virt)
{
	__asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

/* The page table covering `virt`, created on request. */
static uint32_t *table_for(uintptr_t virt, int create)
{
	uint32_t *entry = &page_directory[DIR_INDEX(virt)];
	uintptr_t frame;

	if (!(*entry & PAGE_PRESENT)) {
		if (!create)
			return NULL;
		frame = pmm_alloc_frame();
		if (!frame)
			return NULL;
		memset((void *)frame, 0, PAGE_SIZE);
		/*
		 * The directory entry allows everything its region may
		 * need; each page then narrows the rights down.
		 */
		*entry = frame | PAGE_PRESENT | PAGE_WRITE |
			 (in_user_space(virt) ? PAGE_USER : 0);
	}
	return (uint32_t *)(*entry & PAGE_MASK);
}

/* Give an empty page table back. The direct map's tables always stay. */
static void drop_table_if_empty(uintptr_t virt)
{
	uint32_t *entry = &page_directory[DIR_INDEX(virt)];
	uint32_t *table = (uint32_t *)(*entry & PAGE_MASK);
	size_t i;

	if (virt < VMALLOC_START)
		return;
	for (i = 0; i < ENTRIES; i++)
		if (table[i] & PAGE_PRESENT)
			return;
	*entry = 0;
	pmm_free_frame((uintptr_t)table);
}

static int map(uintptr_t virt, uintptr_t phys, uint32_t flags)
{
	uint32_t *table = table_for(virt, 1);

	if (!table)
		return -1;
	if (table[TABLE_INDEX(virt)] & PAGE_PRESENT)
		return -1;
	table[TABLE_INDEX(virt)] = phys | PAGE_PRESENT | flags;
	flush_page(virt);
	return 0;
}

void paging_init(void)
{
	uintptr_t addr;
	uint32_t cr0;
	uint32_t flags;

	for (addr = 0; addr < pmm_top(); addr += PAGE_SIZE) {
		flags = PAGE_WRITE;
		if (addr >= (uintptr_t)kernel_start &&
		    addr < (uintptr_t)kernel_data_start)
			flags = 0;
		if (map(addr, addr, flags) < 0)
			panic("out of memory while building the direct map");
	}

	__asm__ volatile ("mov %0, %%cr3" : : "r"(page_directory));
	__asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
	cr0 |= CR0_PG | CR0_WP;
	__asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
}

int page_map(uintptr_t virt, uintptr_t phys, uint32_t flags)
{
	if ((virt | phys) & ~PAGE_MASK ||
	    flags & ~(uint32_t)(PAGE_WRITE | PAGE_USER)) {
		oops("page_map: bad arguments %p -> %p flags 0x%x",
		     (void *)virt, (void *)phys, flags);
		return -1;
	}
	if (!!(flags & PAGE_USER) != in_user_space(virt)) {
		oops("page_map: %p is in %s space, %s page refused",
		     (void *)virt, in_user_space(virt) ? "user" : "kernel",
		     flags & PAGE_USER ? "user" : "kernel-only");
		return -1;
	}
	return map(virt, phys, flags);
}

int page_unmap(uintptr_t virt)
{
	uint32_t *table = table_for(virt, 0);

	if (!table || !(table[TABLE_INDEX(virt)] & PAGE_PRESENT))
		return -1;
	table[TABLE_INDEX(virt)] = 0;
	flush_page(virt);
	drop_table_if_empty(virt);
	return 0;
}

void *page_create(uintptr_t virt, uint32_t flags)
{
	uintptr_t frame = pmm_alloc_frame();

	if (!frame)
		return NULL;
	/* Cleared through the direct map, whatever rights `virt` gets. */
	memset((void *)frame, 0, PAGE_SIZE);
	if (page_map(virt, frame, flags) < 0) {
		pmm_free_frame(frame);
		return NULL;
	}
	return (void *)virt;
}

int page_get(uintptr_t virt, uintptr_t *phys, uint32_t *flags)
{
	uint32_t *table = table_for(virt, 0);
	uint32_t entry;

	if (!table)
		return 0;
	entry = table[TABLE_INDEX(virt)];
	if (!(entry & PAGE_PRESENT))
		return 0;
	if (phys)
		*phys = (entry & PAGE_MASK) | (virt & ~PAGE_MASK);
	if (flags)
		*flags = entry & PAGE_FLAGS_MASK;
	return 1;
}

void page_free(uintptr_t virt)
{
	uintptr_t phys;

	if (!page_get(virt, &phys, NULL)) {
		oops("page_free: %p is not mapped", (void *)virt);
		return;
	}
	page_unmap(virt);
	pmm_free_frame(PAGE_ALIGN_DOWN(phys));
}

uint32_t paging_cr0(void)
{
	uint32_t cr0;

	__asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
	return cr0;
}

uint32_t paging_cr3(void)
{
	uint32_t cr3;

	__asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
	return cr3;
}
