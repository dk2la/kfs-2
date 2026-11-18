#ifndef GDT_H
#define GDT_H

/*
 * Global Descriptor Table.
 *
 * Flat segments: every descriptor has base 0 and covers the full 4 GiB, so
 * segmentation only separates privilege levels (DPL 0 for the kernel, 3 for
 * user space). The table is built in C and copied to GDT_ADDRESS, where the
 * GDTR points.
 *
 * Index  Selector  Segment
 *   0     0x00     null (required by the CPU)
 *   1     0x08     kernel code
 *   2     0x10     kernel data
 *   3     0x18     kernel stack
 *   4     0x23     user code   (RPL 3)
 *   5     0x2B     user data   (RPL 3)
 *   6     0x33     user stack  (RPL 3)
 */

#define GDT_ADDRESS		0x00000800
#define GDT_ENTRIES		7

#define GDT_KERNEL_CODE		0x08
#define GDT_KERNEL_DATA		0x10
#define GDT_KERNEL_STACK	0x18
#define GDT_USER_CODE		(0x20 | 3)
#define GDT_USER_DATA		(0x28 | 3)
#define GDT_USER_STACK		(0x30 | 3)

/*
 * Build the table, copy it to GDT_ADDRESS, load it with lgdt and reload
 * every segment register. Called from _start (src/boot.s) before kmain.
 */
void	gdt_init(void);

/*
 * Print the GDTR as the CPU reports it (sgdt), then every descriptor of
 * the table it points to.
 */
void	gdt_print(void);

#endif
