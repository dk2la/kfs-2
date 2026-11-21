/*
 * Kernel linker script.
 *
 * The kernel is loaded at 1 MiB, the conventional address: everything below
 * is reserved for the BIOS, VGA memory and GRUB itself.
 *
 * kernel_start, kernel_data_start and kernel_end tell the memory code where
 * the image lies (include/memory.h).
 */

ENTRY(_start)

SECTIONS
{
	. = 1M;
	kernel_start = .;

	/*
	 * The Multiboot header goes first so that it ends up within the
	 * first 8 KiB of the file, where GRUB looks for it.
	 */
	.text : ALIGN(4K)
	{
		KEEP(*(.multiboot))
		*(.text .text.*)
	}

	.rodata : ALIGN(4K)
	{
		*(.rodata .rodata.*)
	}

	/*
	 * Everything before this point (code and constants) is mapped
	 * read-only once paging is on; .data starts on a new page.
	 */
	.data : ALIGN(4K)
	{
		kernel_data_start = .;
		*(.data .data.*)
	}

	.bss : ALIGN(4K)
	{
		*(COMMON)
		*(.bss .bss.*)
	}
	kernel_end = .;

	/DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
		*(.note.gnu.property)
	}
}
