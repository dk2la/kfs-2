#ifndef STACK_H
#define STACK_H

#include "types.h"

/*
 * Kernel stack inspection.
 *
 * print_kernel_stack() prints, on the active screen:
 *   - the stack bounds (stack_bottom, stack_top from src/boot.s), the
 *     current esp and ebp, and how many bytes are in use;
 *   - a hexdump of the used part, from esp up to stack_top, 16 bytes per
 *     line as four 32-bit words and ASCII, capped to fit one screen;
 *   - a backtrace along the ebp chain: frame address and return address.
 *
 * Return addresses are numbers only: the kernel has no symbol table. Use
 * `nm -n build/kernel.bin` to find the function each one falls in.
 */
void	print_kernel_stack(void);

/*
 * Demo call chain, so that the dump has something to show:
 * stack_demo_a(magic) -> stack_demo_b(0xDEADBEEF, "kfs-2!") ->
 * print_kernel_stack(). The arguments and the string are visible in the
 * hexdump, every call is a frame in the backtrace.
 */
void	stack_demo_a(uint32_t magic);

#endif
