NAME		= kernel.bin
ISO_NAME	= kfs.iso

BUILD		= build
KERNEL		= $(BUILD)/$(NAME)
ISO		= $(BUILD)/$(ISO_NAME)
ISO_ROOT	= $(BUILD)/iso

# Toolchain. Override on the command line to use a cross-compiler, e.g.
#   make CC=i686-elf-gcc LD=i686-elf-ld
CC		= gcc
LD		= ld
AS		= nasm
GRUB_MKRESCUE	= grub-mkrescue
QEMU		= qemu-system-i386

CFLAGS		= -m32 -std=gnu99 -ffreestanding -fno-builtin -fno-stack-protector \
		  -fno-pie -nostdlib -nodefaultlibs -Wall -Wextra -Werror -Iinclude
ASFLAGS		= -f elf32
LDFLAGS		= -m elf_i386 -T linker.ld -nostdlib

# Only the GRUB modules needed to find and boot the kernel from a CD, so the
# ISO stays far below the 10 MB limit.
GRUB_MODULES	= multiboot normal iso9660 biosdisk
GRUB_FLAGS	= --install-modules="$(GRUB_MODULES)" --locales="" --themes="" \
		  --fonts=""

C_SRCS		= src/kernel.c src/vga.c src/keyboard.c lib/string.c lib/printk.c
ASM_SRCS	= src/boot.s
HEADERS		= include/types.h include/vga.h include/string.h include/io.h \
		  include/stdarg.h include/printk.h include/keyboard.h

OBJS		= $(ASM_SRCS:%.s=$(BUILD)/%.o) $(C_SRCS:%.c=$(BUILD)/%.o)

all: $(KERNEL)

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

iso: $(ISO)

$(ISO): $(KERNEL) grub.cfg
	@mkdir -p $(ISO_ROOT)/boot/grub
	cp $(KERNEL) $(ISO_ROOT)/boot/$(NAME)
	cp grub.cfg $(ISO_ROOT)/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) $(GRUB_FLAGS) -o $@ $(ISO_ROOT)

# The emulated screen is drawn as text in the terminal. Quit with Esc, 2,
# then type "quit" in the QEMU monitor.
run: $(ISO)
	$(QEMU) -cdrom $(ISO) -display curses

clean:
	rm -rf $(BUILD)/src $(BUILD)/lib

fclean:
	rm -rf $(BUILD)

re: fclean all

.PHONY: all iso run clean fclean re
