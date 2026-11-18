; Kernel entry point (NASM, 32-bit).
;
; GRUB loads the kernel at 1 MiB, switches the CPU to 32-bit protected mode
; and jumps to _start. Nothing else is set up for us: in particular there is
; no usable stack, so we provide our own before calling any C code.

; Multiboot 1 header. GRUB scans the first 8 KiB of the file for it.
MB_MAGIC	equ 0x1BADB002
MB_FLAGS	equ 0x00000003		; align modules on 4 KiB, provide memory info
MB_CHECKSUM	equ -(MB_MAGIC + MB_FLAGS)

section .multiboot align=4
	dd MB_MAGIC
	dd MB_FLAGS
	dd MB_CHECKSUM

; 16 KiB kernel stack. It grows downwards, so esp starts at stack_top.
STACK_SIZE	equ 16384

section .bss
alignb 16
stack_bottom:
	resb STACK_SIZE
stack_top:

section .text
bits 32
global _start
extern kmain

_start:
	mov esp, stack_top
	cld				; C code expects the direction flag clear
	call kmain

	; kmain returned: there is nothing left to run. Stop here for good.
	cli
.halt:
	hlt
	jmp .halt

; Tell the linker this object does not need an executable stack.
section .note.GNU-stack noalloc noexec nowrite progbits
