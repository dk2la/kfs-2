# KFS_2 design

This document records what we are building for the 42 project KFS_2 ("GDT & Stack"), how it is built and verified, and how the work is carried out. It was agreed on 2026-11-18 and is the reference for implementation and review.

## Goal

Give the KFS_1 kernel its own Global Descriptor Table, loaded at a fixed address, and a tool that prints the kernel stack in a human-friendly way.

The subject requires:

- a Global Descriptor Table containing kernel code, kernel data, kernel stack, user code, user data and user stack segments
- the GDT declared to the CPU ("to the BIOS" in the subject's words) and set at address `0x00000800`
- a tool that prints the kernel stack in a human-friendly way
- the same build rules as KFS_1: our own linker script, no host library, a Makefile, a small bootable image, the whole work under 10 MB

## Scope

**Mandatory part only.** The bonus (a debugging shell) is not built.

The KFS_1 kernel, bonuses included, is the starting point: VGA output with scrolling, cursor and colours, `printk`, the polled PS/2 keyboard and the four virtual screens on F1 to F4 all stay. Only the boot demo in `kmain` changes.

## Constraint: no local testing

The kernel is never built, run or tested on a local machine. Every build and every check runs on GitHub Actions, and results are read from the CI logs and artifacts. A change counts as working only when CI says so.

## Repository and workflow

| Topic | Decision |
|---|---|
| Repository | `dk2la/kfs-2`, public, separate from `dk2la/kfs-1` |
| First commit | Import of `kfs-1` `master` as it stands, without its history |
| Branches | `feature/*` → `develop` → `master`, as in KFS_1 |
| Merging | A PR is merged only with green CI. Claude opens PRs; the owner reviews and merges. Claude never merges. |
| Order of work | 1. this document; 2. PR `feature/ci` (CI on the imported KFS_1 code); 3. PR `feature/mandatory` (GDT, stack tool, CI checks for them); 4. `develop` → `master` |

## Technical decisions

Everything from KFS_1 carries over unchanged unless listed here (C with NASM, i386, Multiboot 1, system `gcc -m32` and `ld -m elf_i386`, own `linker.ld`, ISO from `grub-mkrescue`).

| Topic | Decision | Reason |
|---|---|---|
| Segment model | Flat: every segment has base `0`, limit 4 GiB | Paging comes later; segmentation only has to separate privilege levels |
| Stack segments | Plain read/write data segments, not expand-down | An expand-down segment with base 0 and a 4 GiB limit describes an empty range, so the first `push` would fault |
| Where the table lives | Built in C, copied to `0x800` at run time with `memcpy`; GDTR points at `0x800` | Placing a linker section at `0x800` would ask GRUB to load an ELF segment into low memory, which it may refuse |
| When it is loaded | From `_start`, right after the stack is set, before `kmain` | All C code of the kernel then runs on our segments, never on GRUB's |
| Frame pointers | `-fno-omit-frame-pointer` added to `CFLAGS` | The backtrace walks the `ebp` chain |
| Debug output | `printk` also writes every character to I/O port `0xE9` | QEMU's debugcon captures it to a file, so CI can read what the kernel printed. On real hardware the write is ignored. |

### Why `0x800` is safe

Low memory on a PC: `0x000`–`0x3FF` is the real-mode interrupt vector table, `0x400`–`0x4FF` the BIOS data area, and `0x500`–`0x7BFF` is free conventional memory. The table takes 7 × 8 = 56 bytes, `0x800`–`0x837`, inside the free area. The kernel does not read the Multiboot information structure, so nothing it needs can be overwritten.

## GDT

### Layout

| Index | Selector | Segment | Access byte | DPL | Type |
|---|---|---|---|---|---|
| 0 | `0x00` | null | `0x00` | – | required by the CPU |
| 1 | `0x08` | kernel code | `0x9A` | 0 | code, execute/read |
| 2 | `0x10` | kernel data | `0x92` | 0 | data, read/write |
| 3 | `0x18` | kernel stack | `0x92` | 0 | data, read/write |
| 4 | `0x23` | user code | `0xFA` | 3 | code, execute/read |
| 5 | `0x2B` | user data | `0xF2` | 3 | data, read/write |
| 6 | `0x33` | user stack | `0xF2` | 3 | data, read/write |

User selectors include RPL 3 (`0x20 | 3`, ...). Every descriptor has limit `0xFFFFF` and flags `0xC` (4 KiB granularity, 32-bit), so it covers the full 4 GiB. The GDTR holds base `0x00000800` and limit `0x37` (56 − 1).

### Loading sequence

1. `_start` sets `esp` to `stack_top` and calls `gdt_init()`.
2. `gdt_init()` (C) fills the seven descriptors and copies them to `0x800`.
3. `gdt_flush` (assembly) runs `lgdt`, far-jumps to `0x08:.reload` to reload `CS`, then sets `DS`, `ES`, `FS` and `GS` to `0x10` and `SS` to `0x18`.
4. `_start` calls `kmain`.

`SS` is reloaded without moving `esp`: the stack segment is flat, so the same linear addresses stay valid.

### GDT dump

`gdt_print()` reads the GDTR with `sgdt` and prints its base and limit, then, for each descriptor, its selector, base, limit, access byte, flags, DPL and a name. It proves on screen that the table really is at `0x800` and is the one the CPU uses.

## Kernel stack tool

`print_kernel_stack()` prints:

1. **Header:** `stack_bottom`, `stack_top`, current `esp` and `ebp`, and how many bytes are in use. `stack_bottom` and `stack_top` become `global` in `boot.s`.
2. **Hexdump** of the used part only, from `esp` up to `stack_top`, one line per 16 bytes: address, four 32-bit words in hex, the same bytes as ASCII (`.` for non-printable). The output is capped to the height of a screen.
3. **Backtrace** along the `ebp` chain: frame number, frame address, return address. The walk stops at a null `ebp` or when `ebp` leaves the kernel stack.

Return addresses are printed as numbers only; the kernel carries no symbol table. CI resolves them to function names (see below).

To give the dump something to show, `kmain` calls a short demo chain, `kmain → stack_demo_a(0xCAFEBABE, …) → stack_demo_b(0xDEADBEEF, …) → print_kernel_stack()`. The recognisable arguments and several frames appear in the hexdump and the backtrace.

## `printk` changes

Added: a field width and the `0` and `-` flags, for example `%08x`, `%4u`, `%-10s`, so that the dumps line up. Length modifiers (`%ld`) are still not supported.

## What the kernel shows

| Screen | Content |
|---|---|
| F1 (shown at boot) | `42`, then the GDT dump |
| F2 | The kernel stack dump |
| F3, F4 | Free typing, as in KFS_1 |

The KFS_1 boot demo (colour sample, `printk` examples) is removed. After boot `kmain` stays in the keyboard loop, as before.

## Layout

New and changed files compared with KFS_1:

```
.github/workflows/ci.yml   build, boot and check in QEMU
ci/boot_check.py           drives QEMU through QMP and runs the checks
docs/DESIGN.md             this document
src/
  boot.s                   calls gdt_init before kmain; stack symbols global
  gdt.c                    builds the GDT, copies it to 0x800, prints it
  gdt_flush.s              lgdt, far jump, segment register reload
  stack.c                  print_kernel_stack and the demo call chain
  kernel.c                 new boot demo: screens F1 and F2
include/
  gdt.h
  stack.h
lib/
  printk.c                 width and flags; mirror to port 0xE9
Makefile                   new sources, -fno-omit-frame-pointer
README.md                  GDT, stack dump, CI
```

## Continuous integration

A GitHub Actions workflow runs on every push and every pull request, on `ubuntu-latest`.

1. Install the packages listed in the README.
2. `make` and `make iso`.
3. Fail if the ISO is 10 MB or larger.
4. Boot the ISO in `qemu-system-i386` with no display, debugcon to a log file and the QEMU monitor reachable from the script. The kernel is the normal one: there is no test mode. CI waits a few seconds, collects the results below, then kills QEMU.
5. Upload the ISO, the screenshots, the debugcon log and the monitor output as artifacts.

### Checks

| Check | How | What it proves |
|---|---|---|
| GDTR, from outside the kernel | Monitor `info registers` must show `GDT=00000800 00000037`, `CS=0008`, `DS=0010`, `SS=0018` | The CPU uses our table at `0x800`, with our selectors loaded |
| Kernel output | grep the debugcon log for the GDT dump, the stack dump header and the backtrace | The kernel printed both dumps |
| Screen | `screendump` of screen F1, then `sendkey f2` and `screendump` of screen F2, saved as PNG | What the evaluator will see |
| Backtrace symbols | Return addresses from the log resolved with `addr2line`/`nm` on `kernel.bin`, written to the CI log | The frames are the expected functions |

PR `feature/ci` only adds steps 1–5 with a boot check of the imported KFS_1 kernel. PR `feature/mandatory` adds the GDT, kernel output and backtrace checks.

## Defence checklist

- The GDT has the six required segments plus the null descriptor (F1 screen, `include/gdt.h`).
- It is at `0x00000800` (F1 screen and CI `info registers`).
- It is loaded with `lgdt` and the segment registers point into it (CI `info registers`).
- The kernel stack is printed in a human-friendly way (F2 screen).
- The ISO is under 10 MB (CI step 3).
- No host library or host linker script (Makefile flags, own `linker.ld`).
