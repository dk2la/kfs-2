#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include "types.h"

/*
 * What a Multiboot 1 loader (GRUB) hands to the kernel: a magic number in
 * eax and, in ebx, the address of this structure. Only the fields the
 * kernel reads are named.
 */

#define MULTIBOOT_BOOT_MAGIC	0x2BADB002

#define MULTIBOOT_INFO_MEMORY	0x001	/* mem_lower and mem_upper are valid */
#define MULTIBOOT_INFO_MMAP	0x040	/* mmap_length and mmap_addr are valid */

#define MULTIBOOT_MMAP_AVAILABLE	1	/* RAM the kernel may use */

struct multiboot_info {
	uint32_t	flags;
	uint32_t	mem_lower;	/* KiB of RAM below 1 MiB */
	uint32_t	mem_upper;	/* KiB of RAM above 1 MiB */
	uint32_t	unused[8];
	uint32_t	mmap_length;	/* size of the memory map in bytes */
	uint32_t	mmap_addr;	/* physical address of the memory map */
} __attribute__((packed));

/*
 * One region of the memory map. `size` is the size of the rest of the
 * entry, so the next entry starts `size + 4` bytes further.
 */
struct multiboot_mmap_entry {
	uint32_t	size;
	uint64_t	addr;
	uint64_t	len;
	uint32_t	type;
} __attribute__((packed));

_Static_assert(sizeof(struct multiboot_info) == 52, "Multiboot info layout");
_Static_assert(sizeof(struct multiboot_mmap_entry) == 24, "Multiboot mmap layout");

#endif
