#include "pmm.h"
#include "memory.h"
#include "multiboot.h"
#include "panic.h"
#include "string.h"
#include "types.h"

#define MAX_FRAMES	(DIRECT_MAP_MAX / PAGE_SIZE)
#define FIRST_FRAME	(LOW_MEMORY_END / PAGE_SIZE)
#define BITS		32

/* One bit per frame: 1 = in use or not RAM, 0 = free. */
static uint32_t	bitmap[MAX_FRAMES / BITS];
static size_t	total_frames;
static size_t	free_frames;
static size_t	top_frame;	/* one past the highest usable frame */
static size_t	search_from;	/* no free frame at or above this index */

static int is_used(size_t frame)
{
	return (bitmap[frame / BITS] >> (frame % BITS)) & 1;
}

static void set_used(size_t frame)
{
	bitmap[frame / BITS] |= 1u << (frame % BITS);
}

static void set_free(size_t frame)
{
	bitmap[frame / BITS] &= ~(1u << (frame % BITS));
}

/* Mark the whole frames inside [addr, addr + len) as usable RAM. */
static void add_region(uint64_t addr, uint64_t len)
{
	uint64_t end = addr + len;
	size_t frame;
	size_t last;

	if (addr < LOW_MEMORY_END)
		addr = LOW_MEMORY_END;
	if (end > DIRECT_MAP_MAX)
		end = DIRECT_MAP_MAX;
	if (addr >= end)
		return;
	frame = PAGE_ALIGN_UP((uint32_t)addr) / PAGE_SIZE;
	last = PAGE_ALIGN_DOWN((uint32_t)end) / PAGE_SIZE;
	for (; frame < last; frame++) {
		if (!is_used(frame))
			continue;
		set_free(frame);
		total_frames++;
		free_frames++;
		if (frame >= top_frame)
			top_frame = frame + 1;
	}
}

static void read_memory_map(const struct multiboot_info *info)
{
	const struct multiboot_mmap_entry *entry;
	uintptr_t at = info->mmap_addr;
	uintptr_t end = info->mmap_addr + info->mmap_length;

	while (at < end) {
		entry = (const struct multiboot_mmap_entry *)at;
		if (entry->type == MULTIBOOT_MMAP_AVAILABLE)
			add_region(entry->addr, entry->len);
		at += entry->size + sizeof(entry->size);
	}
}

void pmm_init(const struct multiboot_info *info)
{
	uintptr_t addr;

	memset(bitmap, 0xFF, sizeof(bitmap));
	if (info->flags & MULTIBOOT_INFO_MMAP)
		read_memory_map(info);
	else if (info->flags & MULTIBOOT_INFO_MEMORY)
		add_region(LOW_MEMORY_END, (uint64_t)info->mem_upper * 1024);
	if (total_frames == 0)
		panic("the boot loader reported no usable memory");

	/* The kernel itself sits in that RAM. */
	for (addr = PAGE_ALIGN_DOWN(kernel_start);
	     addr < PAGE_ALIGN_UP(kernel_end); addr += PAGE_SIZE) {
		if (addr / PAGE_SIZE < top_frame && !is_used(addr / PAGE_SIZE)) {
			set_used(addr / PAGE_SIZE);
			free_frames--;
		}
	}
	search_from = top_frame;
}

uintptr_t pmm_alloc_frame(void)
{
	size_t frame = search_from;

	while (frame > FIRST_FRAME) {
		frame--;
		if (!is_used(frame)) {
			set_used(frame);
			free_frames--;
			search_from = frame;
			return frame * PAGE_SIZE;
		}
	}
	search_from = FIRST_FRAME;
	return 0;
}

int pmm_alloc_frame_at(uintptr_t addr)
{
	size_t frame = addr / PAGE_SIZE;

	if (addr % PAGE_SIZE || frame < FIRST_FRAME || frame >= top_frame ||
	    is_used(frame))
		return 0;
	set_used(frame);
	free_frames--;
	return 1;
}

void pmm_free_frame(uintptr_t addr)
{
	size_t frame = addr / PAGE_SIZE;

	if (addr % PAGE_SIZE || frame < FIRST_FRAME || frame >= top_frame) {
		oops("pmm_free_frame: %p is not a frame of usable RAM",
		     (void *)addr);
		return;
	}
	if (!is_used(frame)) {
		oops("pmm_free_frame: frame %p is already free", (void *)addr);
		return;
	}
	set_free(frame);
	free_frames++;
	if (frame >= search_from)
		search_from = frame + 1;
}

size_t pmm_total_frames(void)
{
	return total_frames;
}

size_t pmm_free_frames(void)
{
	return free_frames;
}

uintptr_t pmm_top(void)
{
	return top_frame * PAGE_SIZE;
}
