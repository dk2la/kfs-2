#ifndef STACK_H
#define STACK_H

#include "types.h"

/*
 * Print the kernel stack on the active screen: its bounds and how much is
 * in use, a hexdump from esp upwards, and the chain of calls that led here.
 */
void	print_kernel_stack(void);

/*
 * Call print_kernel_stack through two more functions with recognisable
 * arguments, so that the dump has several frames and known values to show.
 */
void	stack_demo(void);

#endif
