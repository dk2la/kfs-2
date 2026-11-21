#ifndef PANIC_H
#define PANIC_H

/*
 * Kernel panics. Two kinds:
 *
 *   panic()  fatal. The kernel cannot go on safely (a CPU exception in
 *            kernel code, no memory to boot with). Prints the message,
 *            the registers when a CPU exception caused it, and stops the
 *            machine for good.
 *
 *   oops()   not fatal. Something was wrong but the damage is contained
 *            (a bad pointer given to kfree, an allocation that cannot be
 *            satisfied, a breakpoint). Prints the message and returns; the
 *            caller reports the failure to its own caller.
 *
 * Both print on the active screen and, like every printk, on port 0xE9.
 */

struct registers;

void	panic(const char *fmt, ...)
	__attribute__((noreturn, format(printf, 1, 2)));

/* A panic caused by a CPU exception: the saved registers are printed too. */
void	panic_registers(const struct registers *regs, const char *fmt, ...)
	__attribute__((noreturn, format(printf, 2, 3)));

void	oops(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* How many oops() the kernel has survived. */
unsigned int	oops_count(void);

#endif
