#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

/*
 * Memory layout of the kernel.
 *
 * Virtual address space (4 GiB):
 *
 *   0x00000000 - 0x3FFFFFFF  kernel space (supervisor only)
 *       0x00000000 - 0x2FFFFFFF  direct map: every byte of RAM is mapped at
 *                                its own physical address (virtual ==
 *                                physical). The kernel image and the
 *                                kmalloc heap live here.
 *       0x30000000 - 0x3FFFFFFF  vmalloc area: pages mapped on demand to
 *                                whatever frames are free.
 *   0x40000000 - 0xFFFFFFFF  user space (pages carry the user bit)
 *
 * RAM above DIRECT_MAP_MAX cannot be mapped 1:1 and is not used.
 */

#define PAGE_SIZE		4096u
#define PAGE_MASK		(~(uintptr_t)(PAGE_SIZE - 1))
#define PAGE_ALIGN_DOWN(x)	((uintptr_t)(x) & PAGE_MASK)
#define PAGE_ALIGN_UP(x)	(((uintptr_t)(x) + PAGE_SIZE - 1) & PAGE_MASK)

#define KERNEL_SPACE_START	0x00000000u
#define KERNEL_SPACE_END	0x40000000u	/* first address after it */
#define DIRECT_MAP_MAX		0x30000000u
#define VMALLOC_START		0x30000000u
#define VMALLOC_END		0x40000000u
#define USER_SPACE_START	0x40000000u
#define USER_SPACE_LAST		0xFFFFFFFFu

/* RAM below this is left to the BIOS, the VGA card and our GDT. */
#define LOW_MEMORY_END		0x00100000u

/* Set by linker.ld. Only their addresses mean something. */
extern char	kernel_start[];		/* first byte of the image */
extern char	kernel_data_start[];	/* end of code and constants */
extern char	kernel_end[];		/* first byte after .bss */

struct multiboot_info;

/*
 * Bring the memory system up: find the RAM from the Multiboot information,
 * turn paging on, set up both heaps. Panics if the kernel was not started
 * by a Multiboot loader.
 */
void	memory_init(uint32_t magic, const struct multiboot_info *info);

/*
 * Print the state of the memory system on the active screen and exercise
 * every allocator once, so that the result can be read on screen.
 */
void	memory_report(void);

#endif
