# kfs-2

A minimal i386 kernel for the 42 project KFS_2 ("GDT & Stack"). It builds on [kfs-1](https://github.com/dk2la/kfs-1): GRUB boots it, it prints `42`, and it keeps the KFS_1 screen, `printk`, keyboard and four virtual screens. KFS_2 adds:

- its own Global Descriptor Table, at address `0x00000800`, with kernel code, data and stack segments and user code, data and stack segments
- a tool that prints the kernel stack: bounds, a hexdump and a backtrace

Only the mandatory part is done; the bonus shell is not. The design and the reasons behind each choice are in [docs/DESIGN.md](docs/DESIGN.md).

## Dependencies

The kernel is built on Debian or Ubuntu, x86_64:

```sh
sudo apt install make nasm gcc gcc-multilib grub-pc-bin grub-common xorriso mtools qemu-system-x86
```

| Package | Used for |
|---|---|
| `nasm` | assembling the boot code |
| `gcc`, `gcc-multilib` | compiling 32-bit C |
| `grub-pc-bin`, `grub-common`, `xorriso`, `mtools` | building the bootable ISO with `grub-mkrescue` |
| `qemu-system-x86` | running the kernel |

## Build and run

| Command | Effect |
|---|---|
| `make` | Compile and link `build/kernel.bin` |
| `make iso` | Build the bootable image `build/kfs.iso` (BIOS only) |
| `make run` | Boot the ISO in QEMU, with the emulated screen drawn as text in the terminal |
| `make clean` | Remove object files |
| `make fclean` | Remove all build output |
| `make re` | Rebuild from scratch |

To leave `make run`, press `Esc` then `2` to reach the QEMU monitor, type `quit` and press Enter.

To open a graphical window instead: `qemu-system-i386 -cdrom build/kfs.iso`.

`CC`, `LD` and `AS` can be overridden, for example `make CC=i686-elf-gcc LD=i686-elf-ld`.

## How it boots

1. The firmware starts GRUB from the ISO.
2. GRUB reads `grub.cfg` and loads `/boot/kernel.bin`. It recognises the kernel by the Multiboot 1 header (magic `0x1BADB002`) that `linker.ld` places at the start of the code.
3. GRUB puts the kernel at 1 MiB, switches the CPU to 32-bit protected mode, with a GDT of its own, and jumps to `_start` in `src/boot.s`.
4. `_start` points `esp` at the kernel's 16 KiB stack, clears `ebp` (the end of the frame chain) and calls `gdt_init`, which installs our GDT at `0x800` and reloads every segment register.
5. `_start` calls `kmain` in `src/kernel.c`. From here on, all kernel code runs on our segments.
6. `kmain` prints `42` and the GDT on screen 1, the kernel stack on screen 2, then loops forever reading the keyboard.

## What the kernel shows

| Key | Screen |
|---|---|
| `F1` (shown at boot) | `42`, then the GDT dump |
| `F2` | the kernel stack dump |
| `F3`, `F4` | free typing |

Typing works on every screen, as in KFS_1.

## The GDT

`src/gdt.c` builds the table, `src/gdt_flush.s` loads it.

| Index | Selector | Segment | Access byte | DPL |
|---|---|---|---|---|
| 0 | `0x00` | null | `0x00` | – |
| 1 | `0x08` | kernel code | `0x9A` | 0 |
| 2 | `0x10` | kernel data | `0x92` | 0 |
| 3 | `0x18` | kernel stack | `0x92` | 0 |
| 4 | `0x23` | user code | `0xFA` | 3 |
| 5 | `0x2B` | user data | `0xF2` | 3 |
| 6 | `0x33` | user stack | `0xF2` | 3 |

Every segment is flat: base `0`, limit `0xFFFFF` with 4 KiB granularity, so it covers the full 4 GiB. Segmentation only separates privilege levels; paging will come later. The user selectors carry RPL 3. The stack segments are ordinary read/write data segments: an expand-down segment with base 0 and a 4 GiB limit would describe an empty range.

`gdt_init()` fills the seven descriptors on its own stack and copies them with `memcpy` to `0x00000800`, in the free conventional memory above the BIOS data area. `gdt_flush` then:

1. runs `lgdt` with base `0x800` and limit `0x37` (7 × 8 − 1)
2. far-jumps to `0x08:.reload_cs`, the only way to reload `CS`
3. sets `DS`, `ES`, `FS` and `GS` to `0x10` and `SS` to `0x18`

`lgdt` alone would change nothing visible: the CPU keeps using the descriptors cached in the segment registers until they are reloaded.

`gdt_print()` asks the CPU where its table is (`sgdt`) and prints the GDTR, then every descriptor it finds there: selector, base, limit, access byte, flags, DPL and name. This shows that the table really is at `0x800` and is the one in use. The kernel data and stack segments show access `0x93`, not `0x92`: the CPU sets bit 0 (accessed) of a descriptor when it loads it into a segment register.

## The kernel stack tool

`print_kernel_stack()` in `src/stack.c` prints:

1. the bounds of the kernel stack (`stack_bottom`, `stack_top` from `src/boot.s`), the current `esp` and `ebp`, and the bytes in use
2. a hexdump from `esp` up to `stack_top`, one line per 16 bytes: address, four 32-bit words, then the same bytes as ASCII. It is capped to 14 lines so that the whole dump fits on one screen.
3. a backtrace along the `ebp` chain: each frame's address and the return address into its caller. It stops at the null `ebp` set by `_start`, or as soon as `ebp` leaves the stack or stops going up.

The kernel is compiled with `-fno-omit-frame-pointer`, so every function keeps the `ebp` chain the backtrace walks.

To give the dump something to show, `kmain` calls `stack_demo_a(0xCAFEBABE)`, which calls `stack_demo_b(0xDEADBEEF, "kfs-2!")`, which calls `print_kernel_stack()`. Both arguments and the string are visible in the hexdump, and the backtrace has four frames: into `stack_demo_b`, `stack_demo_a`, `kmain` and `_start`.

The kernel carries no symbol table, so return addresses are printed as numbers. `nm -n build/kernel.bin` lists the functions by address; CI resolves the backtrace this way (see below).

## printk

`printk(fmt, ...)` in `lib/printk.c` supports `%c`, `%s`, `%d`, `%u`, `%x`, `%p` and `%%`, with an optional field width and the flags `-` (pad on the right) and `0` (pad numbers with zeros): `%08x`, `%4u`, `%-10s`. A NULL string prints as `(null)`.

Every character is also written to I/O port `0xE9`. QEMU's debugcon device (`-debugcon file:log`) records it, which is how CI reads what the kernel printed. On real hardware nothing listens on that port.

## Layout

```
Makefile
linker.ld        places the Multiboot header first, kernel at 1 MiB
grub.cfg         one menu entry: multiboot /boot/kernel.bin
src/
  boot.s         Multiboot header, stack, _start: gdt_init, then kmain
  gdt.c          builds the GDT, copies it to 0x800, prints it
  gdt_flush.s    lgdt, far jump, segment register reload
  stack.c        print_kernel_stack and the demo call chain
  kernel.c       kmain: screens F1 and F2, then the keyboard loop
  vga.c          screen: write, colours, scrolling, cursor, virtual screens
  keyboard.c     PS/2 keyboard, read by polling
lib/
  string.c       strlen, strcmp, memset, memcpy
  printk.c       formatted print, mirrored to port 0xE9
include/
  gdt.h          selectors and GDT address
  stack.h
  types.h        fixed-width integer types, size_t
  io.h           inb, outb
  stdarg.h       variadic arguments, from compiler builtins
  vga.h
  keyboard.h
  printk.h
  string.h
ci/
  boot_check.py  boots the ISO in QEMU for CI and checks it
.github/workflows/
  ci.yml         GitHub Actions: build, size check, boot check
docs/
  DESIGN.md
```

## Compiler flags

```
-m32 -std=gnu99 -ffreestanding -fno-builtin -fno-stack-protector -fno-pie -nostdlib -nodefaultlibs -fno-omit-frame-pointer -Wall -Wextra -Werror
```

The kernel is linked with `ld -m elf_i386 -T linker.ld -nostdlib`, so no host library and no host linker script is involved.

## Continuous integration

The kernel is built and checked only on GitHub Actions. Every push and pull request runs `.github/workflows/ci.yml`: it installs the packages above, runs `make` and `make iso`, fails if the ISO is 10 MB or larger, then runs `ci/boot_check.py`.

`ci/boot_check.py` boots the ISO in `qemu-system-i386` without a display and drives it through QMP, QEMU's monitor protocol, on a Unix socket. It waits for `42` on screen, saves `info registers`, a screenshot and the text of screen F1, presses F2 and does the same for screen F2. Everything the kernel prints is captured from port `0xE9`. It then checks:

| Check | What it proves |
|---|---|
| `info registers` shows `GDT=00000800 00000037`, `CS=0008`, `DS`/`ES`/`FS`/`GS=0010`, `SS=0018` | The CPU uses our table at `0x800`, with our selectors loaded |
| The printed GDT dump has `GDTR: base=0x00000800 limit=0x0037` and the seven expected rows | The kernel reads back the table it installed |
| The stack header, `cafebabe`, `deadbeef` and `kfs-2!` are in the output | The hexdump shows the demo frames |
| The backtrace return addresses, resolved with `nm`, fall in `stack_demo_b`, `stack_demo_a`, `kmain`, `_start` | The `ebp` walk follows the real call chain |
| Screen F1 shows the GDT dump, screen F2 the stack dump | What the evaluator sees |

The ISO, the kernel, the screenshots, the screen text and the logs are uploaded as the `kfs-ci` artifact of the run.
