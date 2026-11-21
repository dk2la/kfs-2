#ifndef PAGING_H
#define PAGING_H

#include "types.h"

/*
 * Paging: 4 KiB pages, one page directory of 1024 entries, each pointing
 * to a page table of 1024 entries.
 *
 * A linear address is split 10 / 10 / 12 bits: index in the directory,
 * index in the table, offset in the page. CR3 holds the physical address
 * of the directory.
 *
 * Rights are the low bits of each entry. CR0.WP is set, so read-only pages
 * are read-only for the kernel too.
 */

#define PAGE_PRESENT	0x001	/* the page is mapped */
#define PAGE_WRITE	0x002	/* writable; without it the page is read-only */
#define PAGE_USER	0x004	/* reachable from ring 3; without it kernel only */
#define PAGE_ACCESSED	0x020	/* set by the CPU on any access */
#define PAGE_DIRTY	0x040	/* set by the CPU on a write */
#define PAGE_FLAGS_MASK	0xFFF

/*
 * Map all usable RAM at its own address (the kernel's code and constants
 * read-only), then load CR3 and switch paging and write protection on.
 */
void	paging_init(void);

/*
 * Map the page at `virt` to the frame at `phys`. `flags` is PAGE_WRITE
 * and/or PAGE_USER. A page in user space must have PAGE_USER, a page in
 * kernel space must not: anything else is refused. Returns 0, or -1 when
 * refused, already mapped, or out of memory for a page table.
 */
int	page_map(uintptr_t virt, uintptr_t phys, uint32_t flags);

/* Remove the mapping of the page at `virt`. The frame is left alone. */
int	page_unmap(uintptr_t virt);

/*
 * Create a page: take a free frame, clear it and map it at `virt`.
 * Returns the page's address, or NULL.
 */
void	*page_create(uintptr_t virt, uint32_t flags);

/*
 * Get a page: return 1 if the address is mapped and store the physical
 * address it translates to and the entry's flags (either may be NULL).
 */
int	page_get(uintptr_t virt, uintptr_t *phys, uint32_t *flags);

/* Unmap the page at `virt` and give its frame back. */
void	page_free(uintptr_t virt);

uint32_t	paging_cr0(void);
uint32_t	paging_cr3(void);

#endif
