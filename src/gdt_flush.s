; void gdt_flush(const struct gdt_ptr *ptr);
;
; Load the GDT and make every segment register use it. CS cannot be written
; with mov: a far jump is the way to reload it.

KERNEL_CODE	equ 0x08
KERNEL_DATA	equ 0x10
KERNEL_STACK	equ 0x18

section .text
bits 32
global gdt_flush

gdt_flush:
	mov eax, [esp + 4]
	lgdt [eax]
	jmp KERNEL_CODE:.reload
.reload:
	mov ax, KERNEL_DATA
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	; The stack segment is flat like the old one, so esp stays valid.
	mov ax, KERNEL_STACK
	mov ss, ax
	ret

section .note.GNU-stack noalloc noexec nowrite progbits
