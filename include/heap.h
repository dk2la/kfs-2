#ifndef HEAP_H
#define HEAP_H

#include "types.h"

/*
 * Dynamic memory, in two flavours with the same four operations.
 *
 * Physical memory: kmalloc, kfree, ksize, kbrk.
 *   The heap starts right after the kernel image, in the direct map: an
 *   address returned by kmalloc is also the physical address of the memory,
 *   and a block is one contiguous piece of RAM. kbrk grows it by taking the
 *   next frame of RAM, so it stops at the first frame that is not free.
 *
 * Virtual memory: vmalloc, vfree, vsize, vbrk.
 *   The heap lives in the vmalloc area. vbrk grows it by mapping new pages
 *   to whatever frames are free: a block is contiguous in virtual addresses
 *   only, its pages may be anywhere in RAM.
 *
 * The allocators never panic fatally: a request that cannot be satisfied
 * or a bad pointer is reported with oops() and the call fails.
 */

/* Returned by kbrk and vbrk when the break cannot move. */
#define BRK_FAILED	((void *)-1)

void	heap_init(void);

/* Allocate `size` bytes, aligned on 8. NULL if size is 0 or memory is short. */
void	*kmalloc(size_t size);
void	*vmalloc(size_t size);

/* Free a block. NULL is ignored; an invalid or already freed pointer is
 * reported and nothing is done. */
void	kfree(void *ptr);
void	vfree(void *ptr);

/* Usable size of a block, at least what was asked for. 0 if invalid. */
size_t	ksize(const void *ptr);
size_t	vsize(const void *ptr);

/*
 * Move the end of the heap (the "break") by `increment` bytes and return
 * where it was before; an increment of 0 just reads it. Memory is taken or
 * given back by whole pages. Fails with BRK_FAILED when there is no memory
 * left or when shrinking would cut into allocated blocks.
 */
void	*kbrk(intptr_t increment);
void	*vbrk(intptr_t increment);

/* First address of each heap. */
uintptr_t	kheap_start(void);
uintptr_t	vheap_start(void);

#endif
