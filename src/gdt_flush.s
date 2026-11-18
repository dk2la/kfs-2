; Load a GDT and switch every segment register to it (NASM, 32-bit).
;
; void gdt_flush(const struct gdt_ptr *gdtr);
;
; lgdt alone changes nothing visible: the CPU keeps using the descriptors
; cached in the segment registers until each one is reloaded. CS can only be
; reloaded by a far jump; the others by a plain mov.

KERNEL_CODE	equ 0x08
KERNEL_DATA	equ 0x10
KERNEL_STACK	equ 0x18

section .text
bits 32
global gdt_flush

gdt_flush:
	mov eax, [esp + 4]
	lgdt [eax]
	jmp KERNEL_CODE:.reload_cs
.reload_cs:
	mov ax, KERNEL_DATA
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	; The stack segment is flat too, so esp keeps pointing at the same
	; linear address and the return address below is still there.
	mov ax, KERNEL_STACK
	mov ss, ax
	ret

section .note.GNU-stack noalloc noexec nowrite progbits
