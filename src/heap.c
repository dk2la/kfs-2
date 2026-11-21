#include "heap.h"
#include "memory.h"
#include "paging.h"
#include "panic.h"
#include "pmm.h"
#include "types.h"

#define BLOCK_MAGIC	0x4B465333	/* "KFS3" */
#define ALIGNMENT	8
#define ALIGN_UP(x)	(((x) + ALIGNMENT - 1) & ~(uintptr_t)(ALIGNMENT - 1))

/* In front of every block, allocated or free. The payload follows it. */
struct block {
	uint32_t	magic;
	uint32_t	size;		/* payload bytes */
	struct block	*prev;		/* blocks are linked by address order */
	struct block	*next;
	uint32_t	free;
	uint32_t	padding;	/* keeps the payload aligned on 8 */
};

_Static_assert(sizeof(struct block) % ALIGNMENT == 0,
	       "the block header must keep payloads aligned");

/*
 * A heap is a range of addresses [start, brk) that grows towards `limit`.
 * The two heaps only differ in how a page is obtained.
 */
struct heap {
	const char	*name;
	int		virtual;	/* 1: map pages; 0: take the frame itself */
	uintptr_t	start;
	uintptr_t	brk;
	uintptr_t	limit;
	struct block	*first;
	struct block	*last;
};

static struct heap	kheap = { .name = "kmalloc", .virtual = 0 };
static struct heap	vheap = { .name = "vmalloc", .virtual = 1 };

static uintptr_t block_end(const struct block *block)
{
	return (uintptr_t)(block + 1) + block->size;
}

static int take_page(const struct heap *heap, uintptr_t addr)
{
	if (heap->virtual)
		return page_create(addr, PAGE_WRITE) != NULL;
	return pmm_alloc_frame_at(addr);
}

static void release_page(const struct heap *heap, uintptr_t addr)
{
	if (heap->virtual)
		page_free(addr);
	else
		pmm_free_frame(addr);
}

/* Move the break to `brk`, taking or releasing the pages in between. */
static int set_brk(struct heap *heap, uintptr_t brk)
{
	uintptr_t old_end = PAGE_ALIGN_UP(heap->brk);
	uintptr_t new_end;
	uintptr_t addr;

	if (brk < heap->start || brk > heap->limit)
		return -1;
	new_end = PAGE_ALIGN_UP(brk);
	for (addr = old_end; addr < new_end; addr += PAGE_SIZE) {
		if (take_page(heap, addr))
			continue;
		while (addr > old_end) {
			addr -= PAGE_SIZE;
			release_page(heap, addr);
		}
		return -1;
	}
	for (addr = new_end; addr < old_end; addr += PAGE_SIZE)
		release_page(heap, addr);
	heap->brk = brk;
	return 0;
}

static void *heap_brk(struct heap *heap, intptr_t increment)
{
	uintptr_t old = heap->brk;
	uintptr_t brk = old + (uintptr_t)increment;

	/* Refuse an increment that wraps around the address space. */
	if ((increment > 0 && brk < old) || (increment < 0 && brk > old))
		return BRK_FAILED;
	if (heap->last && brk < block_end(heap->last))
		return BRK_FAILED;
	if (set_brk(heap, brk) < 0)
		return BRK_FAILED;
	return (void *)old;
}

/* Cut `block` down to `size` and make a free block of the rest. */
static void split(struct heap *heap, struct block *block, size_t size)
{
	struct block *rest;

	if (block->size < size + sizeof(*rest) + ALIGNMENT)
		return;
	rest = (struct block *)((uintptr_t)(block + 1) + size);
	rest->magic = BLOCK_MAGIC;
	rest->size = block->size - size - sizeof(*rest);
	rest->free = 1;
	rest->prev = block;
	rest->next = block->next;
	if (rest->next)
		rest->next->prev = rest;
	else
		heap->last = rest;
	block->next = rest;
	block->size = size;
}

/* Merge `block` with the next one if both are free and touch each other. */
static void merge_with_next(struct heap *heap, struct block *block)
{
	struct block *next = block->next;

	if (!next || !block->free || !next->free ||
	    block_end(block) != (uintptr_t)next)
		return;
	block->size += sizeof(*next) + next->size;
	block->next = next->next;
	if (block->next)
		block->next->prev = block;
	else
		heap->last = block;
	next->magic = 0;
}

/* Grow the heap so that its last block can hold `size` bytes. */
static struct block *extend(struct heap *heap, size_t size)
{
	struct block *last = heap->last;
	struct block *block;
	uintptr_t at;
	size_t need;

	if (last && last->free && block_end(last) == heap->brk) {
		need = PAGE_ALIGN_UP(size - last->size);
		if (heap_brk(heap, (intptr_t)need) == BRK_FAILED)
			return NULL;
		last->size += need;
		return last;
	}
	/* Someone may have moved the break with kbrk/vbrk: realign. */
	at = ALIGN_UP(heap->brk);
	need = PAGE_ALIGN_UP(at + sizeof(*block) + size) - heap->brk;
	if (heap_brk(heap, (intptr_t)need) == BRK_FAILED)
		return NULL;
	block = (struct block *)at;
	block->magic = BLOCK_MAGIC;
	block->size = heap->brk - (uintptr_t)(block + 1);
	block->free = 1;
	block->prev = last;
	block->next = NULL;
	if (last)
		last->next = block;
	else
		heap->first = block;
	heap->last = block;
	return block;
}

/*
 * If the last block is free and ends at the break, drop it and move the
 * break down to where it started: the pages above go back to the system.
 */
static void trim(struct heap *heap)
{
	struct block *last = heap->last;

	if (!last || !last->free || block_end(last) != heap->brk)
		return;
	heap->last = last->prev;
	if (heap->last)
		heap->last->next = NULL;
	else
		heap->first = NULL;
	last->magic = 0;
	set_brk(heap, (uintptr_t)last);
}

static void *heap_alloc(struct heap *heap, size_t size)
{
	struct block *block;

	if (size == 0)
		return NULL;
	if (size > heap->limit - heap->start) {
		oops("%s heap: %u bytes is more than it can ever hold",
		     heap->name, size);
		return NULL;
	}
	size = ALIGN_UP(size);
	for (block = heap->first; block; block = block->next)
		if (block->free && block->size >= size)
			break;
	if (!block)
		block = extend(heap, size);
	if (!block) {
		oops("%s heap: out of memory, %u bytes refused",
		     heap->name, size);
		return NULL;
	}
	split(heap, block, size);
	block->free = 0;
	return block + 1;
}

/* The header of the block `ptr` belongs to, or NULL if it is not one. */
static struct block *block_of(const struct heap *heap, const void *ptr)
{
	uintptr_t addr = (uintptr_t)ptr;
	struct block *block;

	if (addr % ALIGNMENT || addr < heap->start + sizeof(*block) ||
	    addr >= heap->brk)
		return NULL;
	block = (struct block *)addr - 1;
	return block->magic == BLOCK_MAGIC ? block : NULL;
}

static void heap_free(struct heap *heap, void *ptr)
{
	struct block *block;

	if (!ptr)
		return;
	block = block_of(heap, ptr);
	if (!block) {
		oops("%s heap: free of %p, not an allocated block",
		     heap->name, ptr);
		return;
	}
	if (block->free) {
		oops("%s heap: double free of %p", heap->name, ptr);
		return;
	}
	block->free = 1;
	merge_with_next(heap, block);
	if (block->prev) {
		block = block->prev;
		merge_with_next(heap, block);
	}
	trim(heap);
}

static size_t heap_size(const struct heap *heap, const void *ptr)
{
	const struct block *block = block_of(heap, ptr);

	if (!block || block->free) {
		oops("%s heap: size of %p, not an allocated block",
		     heap->name, ptr);
		return 0;
	}
	return block->size;
}

void heap_init(void)
{
	kheap.start = PAGE_ALIGN_UP(kernel_end);
	kheap.brk = kheap.start;
	kheap.limit = pmm_top();
	vheap.start = VMALLOC_START;
	vheap.brk = vheap.start;
	vheap.limit = VMALLOC_END;
}

void *kmalloc(size_t size)
{
	return heap_alloc(&kheap, size);
}

void kfree(void *ptr)
{
	heap_free(&kheap, ptr);
}

size_t ksize(const void *ptr)
{
	return heap_size(&kheap, ptr);
}

void *kbrk(intptr_t increment)
{
	return heap_brk(&kheap, increment);
}

uintptr_t kheap_start(void)
{
	return kheap.start;
}

void *vmalloc(size_t size)
{
	return heap_alloc(&vheap, size);
}

void vfree(void *ptr)
{
	heap_free(&vheap, ptr);
}

size_t vsize(const void *ptr)
{
	return heap_size(&vheap, ptr);
}

void *vbrk(intptr_t increment)
{
	return heap_brk(&vheap, increment);
}

uintptr_t vheap_start(void)
{
	return vheap.start;
}
