/*
 * Kernel linker script.
 *
 * The kernel is loaded at 1 MiB, the conventional address: everything below
 * is reserved for the BIOS, VGA memory and GRUB itself.
 */

ENTRY(_start)

SECTIONS
{
	. = 1M;

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

	.data : ALIGN(4K)
	{
		*(.data .data.*)
	}

	.bss : ALIGN(4K)
	{
		*(COMMON)
		*(.bss .bss.*)
	}

	/DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
		*(.note.gnu.property)
	}
}
