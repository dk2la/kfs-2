# kfs-3

A minimal i386 kernel for the 42 project KFS_3 ("Memory"). It builds on KFS_1 and KFS_2: GRUB boots it, it prints `42`, and it keeps the screen, `printk`, keyboard, four virtual screens, the GDT at `0x00000800` and the kernel stack dump. KFS_3 adds a memory system:

- paging, with read/write and user/kernel rights on every page
- a kernel space and a user space
- functions to create, get and free pages
- `kmalloc`, `kfree`, `ksize`, `kbrk` for physical memory and `vmalloc`, `vfree`, `vsize`, `vbrk` for virtual memory
- kernel panics, fatal and not fatal

Only the mandatory part is done. [docs/DESIGN.md](docs/DESIGN.md) is the design of the KFS_2 part.

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

### Vagrant

On a machine without these packages, the `Vagrantfile` creates an Ubuntu VM that has them (it needs Vagrant and VirtualBox):

```sh
vagrant up
vagrant ssh
make run
```

The project directory is shared with the VM as `/vagrant`, where the shell starts.

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
4. `_start` points `esp` at the kernel's 16 KiB stack, clears `ebp` (the end of the frame chain), pushes the Multiboot magic and information pointer GRUB left in `eax` and `ebx`, and calls `gdt_init`, which installs our GDT at `0x800` and reloads every segment register.
5. `_start` calls `kmain` in `src/kernel.c`. From here on, all kernel code runs on our segments.
6. `kmain` installs the exception handlers (`idt_init`), then `memory_init` reads the memory map, turns paging on and sets up the heaps.
7. `kmain` prints `42` and the GDT on screen 1, the kernel stack on screen 2, the memory report on screen 3, then loops forever reading the keyboard.

## What the kernel shows

| Key | Screen |
|---|---|
| `F1` (shown at boot) | `42`, then the GDT dump |
| `F2` | the kernel stack dump |
| `F3` | the memory report |
| `F4` | free typing |
| `F5` | panic demo, not fatal: a double `kfree` |
| `F6` | panic demo, not fatal: a breakpoint exception |
| `F7` | panic demo, fatal: reads an address that is not mapped |
| `F8` | panic demo, fatal: writes to the kernel's read-only code |

The panic demos print on whichever screen is shown. After `F7` or `F8` the kernel is stopped: restart QEMU.

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

## Memory

### Address space

| Range | What |
|---|---|
| `0x00000000`-`0x3FFFFFFF` | kernel space: pages without the user bit |
| `0x00000000`-`0x2FFFFFFF` | direct map: all usable RAM mapped at its own address (virtual = physical). The kernel image and the `kmalloc` heap are here |
| `0x30000000`-`0x3FFFFFFF` | `vmalloc` area: pages mapped on demand to any free frame |
| `0x40000000`-`0xFFFFFFFF` | user space: pages with the user bit |

RAM above 768 MiB cannot be mapped 1:1 and is not used. Page 0 stays mapped because the GDT is at `0x800`. Nothing runs in ring 3 yet, so the user/kernel split is enforced by `page_map` (a user page is refused in kernel space and the reverse) but not yet exercised by a real user program.

### Physical memory (`src/pmm.c`)

The usable RAM is read from the memory map GRUB passes in the Multiboot information. A bitmap holds one bit per 4 KiB frame. Frames below 1 MiB and the kernel image are reserved. `pmm_alloc_frame` hands out the highest free frame, so that the `kmalloc` heap, which grows upwards from the end of the kernel, meets it as late as possible.

### Paging (`src/paging.c`)

One page directory and 4 KiB pages. `paging_init` builds the direct map, loads `CR3` and sets `CR0.PG` and `CR0.WP`. With `WP` set, a read-only page is read-only for the kernel too: the kernel's code and constants are mapped read-only, and a write there is a page fault.

| Function | Effect |
|---|---|
| `page_create(virt, flags)` | take a free frame, clear it, map it at `virt` |
| `page_get(virt, &phys, &flags)` | tell whether `virt` is mapped, to which physical address, with which rights |
| `page_map(virt, phys, flags)` | map a page to a given frame |
| `page_unmap(virt)` | remove a mapping |
| `page_free(virt)` | remove a mapping and give the frame back |

Rights are `PAGE_WRITE` and `PAGE_USER`. Page tables are created when first needed and given back when their last page is unmapped.

### Allocators (`src/heap.c`)

| Physical | Virtual | Effect |
|---|---|---|
| `kmalloc(size)` | `vmalloc(size)` | allocate, aligned on 8 bytes |
| `kfree(ptr)` | `vfree(ptr)` | free |
| `ksize(ptr)` | `vsize(ptr)` | usable size of a block |
| `kbrk(increment)` | `vbrk(increment)` | move the end of the heap, return where it was |

Both heaps use the same first-fit allocator: a list of blocks, each with a header; a free block is split when it is too large and merged with its free neighbours; free memory at the end of the heap is given back. They differ in where a page comes from:

- the `kmalloc` heap is in the direct map. Its addresses are physical addresses and a block is one contiguous piece of RAM. `kbrk` takes the next frame of RAM, and fails if that frame is in use.
- the `vmalloc` heap is in the `vmalloc` area. `vbrk` maps each new page to any free frame, so a block is contiguous in virtual addresses only.

### Panics (`src/panic.c`, `src/idt.c`)

| Kind | When | Effect |
|---|---|---|
| `panic()`, fatal | a CPU exception in kernel code (page fault, general protection fault, ...), no usable memory at boot, not started by a Multiboot loader | prints `KERNEL PANIC`, the message and, for an exception, the registers including `CR2`; disables interrupts and halts |
| `oops()`, not fatal | a bad or double free, an allocation that cannot be satisfied, a page mapping that is refused, a breakpoint exception | prints `oops (not fatal)` and the message, then returns; the failed call returns an error (`NULL`, `-1`) |

The 32 CPU exceptions have handlers in an IDT (`src/idt.c`, entry points in `src/isr.s`). Hardware interrupts stay disabled; the keyboard is still polled.

### The memory report

Screen `F3` prints the amount of RAM, `CR0` and `CR3`, the layout, then runs each allocator once: a `kmalloc` (its address equals its physical address), a `vmalloc` of three pages (with the frame behind each page), a user page, the rights of five addresses, a write and read back of all three, and the number of free frames after everything is freed.

## Layout

```
Makefile
linker.ld        places the Multiboot header first, kernel at 1 MiB,
                 exports the bounds of the image
grub.cfg         one menu entry: multiboot /boot/kernel.bin
src/
  boot.s         Multiboot header, stack, _start: gdt_init, then kmain
  isr.s          entry points of the CPU exceptions
  idt.c          the IDT and the exception handler
  panic.c        panic (fatal) and oops (not fatal)
  pmm.c          physical frames: bitmap from the Multiboot memory map
  paging.c       page directory and tables, page_create/get/map/free
  heap.c         kmalloc and vmalloc heaps
  memory.c       memory_init and the report of screen F3
  gdt.c          builds the GDT, copies it to 0x800, prints it
  gdt_flush.s    lgdt, far jump, segment register reload
  stack.c        print_kernel_stack and the demo call chain
  kernel.c       kmain: screens F1 to F3, panic demos, the keyboard loop
  vga.c          screen: write, colours, scrolling, cursor, virtual screens
  keyboard.c     PS/2 keyboard, read by polling
lib/
  string.c       strlen, strcmp, memset, memcpy
  printk.c       formatted print, mirrored to port 0xE9
include/
  gdt.h          selectors and GDT address
  stack.h
  memory.h       address space layout, memory_init
  multiboot.h    what GRUB passes to the kernel
  pmm.h
  paging.h       page rights, page functions
  heap.h         kmalloc, vmalloc and friends
  panic.h
  idt.h          saved registers of an exception
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

Every push and pull request runs `.github/workflows/ci.yml`: it installs the packages above, runs `make` and `make iso`, fails if the ISO is 10 MB or larger, then runs `ci/boot_check.py`.

`ci/boot_check.py` boots the ISO in `qemu-system-i386` without a display and drives it through QMP, QEMU's monitor protocol, on a Unix socket. It waits for `42` on screen, saves `info registers`, a screenshot and the text of screen F1, presses F2 and F3 and does the same for those screens, then presses F4, F5, F6 and F7 for the panic demos. Everything the kernel prints is captured from port `0xE9`. It then checks:

| Check | What it proves |
|---|---|
| `info registers` shows `GDT=00000800 00000037`, `CS=0008`, `DS`/`ES`/`FS`/`GS=0010`, `SS=0018` | The CPU uses our table at `0x800`, with our selectors loaded |
| The printed GDT dump has `GDTR: base=0x00000800 limit=0x0037` and the seven expected rows | The kernel reads back the table it installed |
| The stack header, `cafebabe`, `deadbeef` and `kfs-2!` are in the output | The hexdump shows the demo frames |
| The backtrace return addresses, resolved with `nm`, fall in `stack_demo_b`, `stack_demo_a`, `kmain`, `_start` | The `ebp` walk follows the real call chain |
| Screen F1 shows the GDT dump, screen F2 the stack dump, screen F3 the memory report | What the evaluator sees |
| `info registers` shows `CR0` with `PG` and `WP` set | Paging and write protection are on |
| The memory report has the RAM size, both spaces, a `kmalloc` whose address equals its physical address, a `vmalloc` whose three pages map to other frames, a user page, the expected rights, `ok` for the write and read back, and `all given back` | Every allocator works and nothing leaks |
| After `F5` and `F6` the kernel prints two `oops` and still answers | Panics that are not fatal are survived |
| After `F7` the kernel prints `KERNEL PANIC: page fault reading 0x50000000`, the registers and `System halted.`, and the CPU is halted with interrupts off | A fatal panic prints and stops the kernel |

The ISO, the kernel, the screenshots, the screen text and the logs are uploaded as the `kfs-ci` artifact of the run.
