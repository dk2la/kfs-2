; Entry points of the 32 CPU exceptions.
;
; Each one leaves the same thing on the stack - the exception number above
; an error code - then joins isr_common, which saves the registers and calls
; exception_handler(struct registers *) in src/idt.c.

section .text
bits 32
extern exception_handler

; Exceptions for which the CPU pushes no error code: push 0 in its place.
%macro ISR_NO_ERROR 1
isr%1:
	push dword 0
	push dword %1
	jmp isr_common
%endmacro

%macro ISR_ERROR 1
isr%1:
	push dword %1
	jmp isr_common
%endmacro

%assign n 0
%rep 32
  %if n = 8 || (n >= 10 && n <= 14) || n = 17 || n = 21 || n = 29 || n = 30
	ISR_ERROR n
  %else
	ISR_NO_ERROR n
  %endif
  %assign n n + 1
%endrep

isr_common:
	pusha
	cld				; C code expects the direction flag clear
	push esp			; argument: the saved registers
	call exception_handler
	add esp, 4
	popa
	add esp, 8			; drop the number and the error code
	iret

; Addresses of the entry points, for idt_init().
section .rodata
global isr_table
isr_table:
%assign n 0
%rep 32
	dd isr %+ n
  %assign n n + 1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits
