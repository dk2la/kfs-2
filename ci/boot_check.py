#!/usr/bin/env python3
"""Boot the kernel ISO in QEMU without a display and check what it does.

Run by GitHub Actions after `make iso`:

    python3 ci/boot_check.py build/kfs.iso build/kernel.bin ci-out

QEMU runs headless. Its monitor (QMP) is reached through a Unix socket and the
kernel's port 0xE9 output (QEMU debugcon) goes to a file. The script waits
for the kernel to print `42`, then collects into the output directory:

    registers.txt   monitor `info registers`
    screen-1.png    screenshot of screen F1
    screen-1.txt    text of screen F1, read from VGA memory
    screen-2.png    screenshot of screen F2 (after `sendkey f2`)
    screen-2.txt    text of screen F2
    screen-3.png    screenshot of screen F3, the memory report
    screen-3.txt    text of screen F3
    panic.png       screenshot after the panic demos (F5, F6, then F7)
    panic.txt       text of that screen
    registers-panic.txt   monitor `info registers` after the fatal panic
    debugcon.log    everything the kernel wrote to port 0xE9

Each check prints PASS or FAIL. The exit status is 1 if any check failed.
"""

import json
import os
import re
import socket
import struct
import subprocess
import sys
import time
import zlib

VGA_ADDR = 0xB8000
VGA_WIDTH = 80
VGA_HEIGHT = 25
BOOT_TIMEOUT = 30


class Monitor:
    """QEMU's QMP monitor (JSON, one message per line) on a Unix socket."""

    def __init__(self, path):
        deadline = time.time() + 10
        while True:
            try:
                self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.sock.connect(path)
                break
            except OSError:
                self.sock.close()
                if time.time() > deadline:
                    raise
                time.sleep(0.2)
        self.sock.settimeout(30)
        self.reader = self.sock.makefile("r", encoding="utf-8")
        self._read_message()  # greeting
        self.execute("qmp_capabilities")

    def _read_message(self):
        line = self.reader.readline()
        if not line:
            raise RuntimeError("QEMU monitor closed the connection")
        return json.loads(line)

    def execute(self, command, **arguments):
        request = {"execute": command}
        if arguments:
            request["arguments"] = arguments
        self.sock.sendall(json.dumps(request).encode() + b"\n")
        while True:
            reply = self._read_message()
            if "return" in reply:
                return reply["return"]
            if "error" in reply:
                raise RuntimeError("%s failed: %s" % (command, reply["error"]))
            # Anything else is an asynchronous event: skip it.

    def human(self, line):
        """Run a human monitor command, such as `info registers`."""
        return self.execute("human-monitor-command", **{"command-line": line})

    def quit(self):
        try:
            self.execute("quit")
        except (OSError, RuntimeError):
            pass
        self.sock.close()


def read_screen(monitor, out_dir, name):
    """Return the 25 lines of text currently in VGA memory."""
    dump = os.path.join(out_dir, name + ".vga")
    monitor.execute("pmemsave", val=VGA_ADDR, size=VGA_WIDTH * VGA_HEIGHT * 2, filename=dump)
    with open(dump, "rb") as f:
        cells = f.read()
    os.remove(dump)
    chars = "".join(chr(c) if 0x20 <= c < 0x7F else " " for c in cells[0::2])
    return [chars[i : i + VGA_WIDTH].rstrip() for i in range(0, len(chars), VGA_WIDTH)]


def ppm_to_png(ppm_path, png_path):
    """Convert QEMU's binary PPM (P6, maxval 255) screendump to PNG."""
    with open(ppm_path, "rb") as f:
        data = f.read()
    # Header: "P6" width height maxval, separated by whitespace, then one
    # whitespace byte before the pixels. QEMU writes no comments.
    header = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", data)
    if not header:
        raise RuntimeError("unexpected screendump format in %s" % ppm_path)
    width, height = int(header.group(1)), int(header.group(2))
    pixels = data[header.end() : header.end() + width * height * 3]
    stride = width * 3
    raw = b"".join(b"\x00" + pixels[y * stride : (y + 1) * stride] for y in range(height))

    def chunk(kind, body):
        return (struct.pack(">I", len(body)) + kind + body
                + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF))

    with open(png_path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def save_screen(monitor, out_dir, name):
    ppm = os.path.join(out_dir, name + ".ppm")
    monitor.execute("screendump", filename=ppm)
    ppm_to_png(ppm, os.path.join(out_dir, name + ".png"))
    os.remove(ppm)
    lines = read_screen(monitor, out_dir, name)
    with open(os.path.join(out_dir, name + ".txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    return lines


def wait_for_boot(monitor, out_dir):
    deadline = time.time() + BOOT_TIMEOUT
    while time.time() < deadline:
        lines = read_screen(monitor, out_dir, "boot")
        if lines and lines[0].startswith("42"):
            return True
        time.sleep(0.5)
    return False


def wait_for_quiet(path, quiet=1.0, timeout=BOOT_TIMEOUT):
    """Wait until the file at `path` has stopped growing for `quiet` seconds."""
    deadline = time.time() + timeout
    size, since = -1, time.time()
    while time.time() < deadline:
        current = os.path.getsize(path) if os.path.exists(path) else 0
        if current != size:
            size, since = current, time.time()
        elif time.time() - since >= quiet:
            return
        time.sleep(0.2)


# ---- KFS_2 checks -------------------------------------------------------

# (selector, access byte, DPL, name) of every descriptor, as gdt_print()
# prints them.
EXPECTED_GDT = [
    (0x00, 0x00, 0, "null"),
    (0x08, 0x9A, 0, "kernel code"),
    (0x10, 0x92, 0, "kernel data"),
    (0x18, 0x92, 0, "kernel stack"),
    (0x23, 0xFA, 3, "user code"),
    (0x2B, 0xF2, 3, "user data"),
    (0x33, 0xF2, 3, "user stack"),
]

# Innermost first: the functions the backtrace frames return into.
EXPECTED_BACKTRACE = ["stack_demo_b", "stack_demo_a", "kmain", "_start"]


def check_registers(checks, registers):
    """The CPU, seen from QEMU, uses our table at 0x800 and our selectors."""
    checks.check("GDTR base 0x00000800 limit 0x37",
                 re.search(r"^GDT=\s*00000800 00000037\b", registers, re.M) is not None)
    for reg, sel in (("CS", 0x08), ("DS", 0x10), ("ES", 0x10), ("FS", 0x10),
                     ("GS", 0x10), ("SS", 0x18)):
        checks.check("%s = 0x%04x" % (reg, sel),
                     re.search(r"^%s =%04x " % (reg, sel), registers, re.M) is not None)


def check_gdt_dump(checks, log):
    checks.check("kernel printed GDTR at 0x00000800",
                 "GDTR: base=0x00000800 limit=0x0037 (7 descriptors)" in log)
    for sel, access, dpl, name in EXPECTED_GDT:
        limit = "00000" if sel == 0 else "fffff"
        flags = "0x0" if sel == 0 else "0xc"
        # The CPU sets bit 0 (accessed) of a descriptor when it loads it.
        row = r"0x%02x\s+00000000\s+%s\s+0x(?:%02x|%02x)\s+%s\s+%d\s+%s\s*$" % (
            sel, limit, access, access | 1, flags, dpl, re.escape(name))
        checks.check("GDT dump row: %s" % name, re.search(row, log, re.M) is not None)


def load_symbols(kernel):
    """Function symbols of the kernel, as (address, name), sorted."""
    out = subprocess.run(["nm", "-n", kernel], check=True, capture_output=True,
                         text=True).stdout
    symbols = []
    for line in out.splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[1] in "Tt":
            symbols.append((int(fields[0], 16), fields[2]))
    return symbols


def symbol_for(symbols, addr):
    found = "??"
    for start, name in symbols:
        if start > addr:
            break
        found = name
    return found


def check_stack_dump(checks, log, kernel):
    checks.check("kernel printed the stack header",
                 re.search(r"^kernel stack: bottom=0x[0-9a-f]{8} top=0x[0-9a-f]{8}", log, re.M)
                 is not None and re.search(r"^esp=0x[0-9a-f]{8} ebp=0x[0-9a-f]{8} in use=\d+ bytes",
                                           log, re.M) is not None)
    # The ASCII columns of the hexdump, joined: a string on the stack may
    # straddle two lines.
    ascii = "".join(re.findall(r"^0x[0-9a-f]{8}: (?:[0-9a-f]{8} ){4} (.{16})$", log, re.M))
    checks.check("hexdump shows the demo arguments",
                 "cafebabe" in log and "deadbeef" in log and "kfs-2!" in ascii)

    frames = re.findall(r"#(\d+)\s+frame 0x([0-9a-f]{8})\s+return 0x([0-9a-f]{8})", log)
    symbols = load_symbols(kernel)
    print("---- backtrace, resolved with nm ----")
    names = []
    for depth, frame, ret in frames:
        # ret - 1 is inside the call instruction, so inside the caller even
        # when the call is the last instruction of a function.
        name = symbol_for(symbols, int(ret, 16) - 1)
        names.append(name)
        print("#%s  frame 0x%s  return 0x%s  in %s" % (depth, frame, ret, name))
    # A local label such as _start.halt still belongs to _start.
    names = [n.split(".")[0] for n in names]
    checks.check("backtrace is %s" % " <- ".join(EXPECTED_BACKTRACE),
                 names == EXPECTED_BACKTRACE, " <- ".join(names))


# ---- KFS_3 checks -------------------------------------------------------

def check_memory(checks, log, registers):
    """Paging is on and the memory report shows every allocator working."""
    cr0 = re.search(r"^CR0=([0-9a-f]{8})", registers, re.M)
    cr0 = int(cr0.group(1), 16) if cr0 else 0
    checks.check("CR0 has paging (PG) and write protect (WP) set",
                 cr0 & 0x80010000 == 0x80010000, "CR0=%08x" % cr0)
    checks.check("kernel reports paging and write protect on",
                 re.search(r"^CR0=0x[0-9a-f]{8} paging on, write protect on", log, re.M)
                 is not None)
    checks.check("RAM found from the Multiboot memory map",
                 re.search(r"^RAM: \d+ MiB usable = \d+ frames of 4 KiB, \d+ free", log, re.M)
                 is not None)
    checks.check("kernel and user space are defined",
                 "kernel space 0x00000000-0x3fffffff" in log
                 and "user space   0x40000000-0xffffffff" in log)

    k = re.search(r"^kmalloc\(100\) = 0x([0-9a-f]{8}) ksize=(\d+) physical=0x([0-9a-f]{8})",
                  log, re.M)
    checks.check("kmalloc returns physical memory (virtual == physical)",
                 k is not None and k.group(1) == k.group(3) and int(k.group(2)) >= 100)
    v = re.search(r"^vmalloc\(10000\) = 0x(3[0-9a-f]{7}) vsize=(\d+)", log, re.M)
    checks.check("vmalloc returns memory in the vmalloc area",
                 v is not None and int(v.group(2)) >= 10000)
    pages = re.search(r"^  its pages:((?: 0x[0-9a-f]{8}>0x[0-9a-f]{8})+)$", log, re.M)
    pairs = re.findall(r"0x([0-9a-f]{8})>0x([0-9a-f]{8})", pages.group(1)) if pages else []
    checks.check("vmalloc pages are mapped to frames elsewhere in RAM",
                 len(pairs) == 3 and all(virt != phys for virt, phys in pairs))
    checks.check("a user page was created at 0x40000000",
                 re.search(r"^page_create\(user\) = 0x40000000 > frame 0x[0-9a-f]{8}", log, re.M)
                 is not None)
    checks.check("rights: kernel code read-only, heaps kernel read/write, user page user",
                 re.search(r"0x00100000 P - -, 0x[0-9a-f]{8} P W -", log) is not None
                 and re.search(r"0x3[0-9a-f]{7} P W -, 0x40000000 P W U", log) is not None
                 and "0x50000000 not mapped" in log)
    checks.check("memory can be written and read back",
                 "write and read back (kmalloc, vmalloc, user page): ok" in log)
    checks.check("every frame is given back after the frees",
                 re.search(r"^after kfree, vfree, page_free: \d+ frames free \(all given back\)",
                           log, re.M) is not None)


def check_panics(checks, log, registers, screen):
    """F5 and F6 are survived, F7 stops the kernel on a page fault."""
    oops_free = log.find("oops (not fatal): kmalloc heap: double free of 0x")
    oops_trap = log.find("oops (not fatal): breakpoint exception at 0x")
    fatal = log.find("KERNEL PANIC: page fault reading 0x50000000: page not mapped (kernel mode)")
    checks.check("F5: a double free is reported and survived", oops_free >= 0)
    checks.check("F6: a breakpoint exception is reported and survived",
                 oops_trap > oops_free >= 0)
    checks.check("F7: a page fault is a fatal panic", fatal > oops_trap >= 0)
    checks.check("the panic prints the registers and halts",
                 re.search(r"^ cr0=[0-9a-f]{8} cr2=50000000 cr3=[0-9a-f]{8}$", log, re.M)
                 is not None and log.rstrip().endswith("System halted."))
    # EFLAGS bit 9 is the interrupt flag.
    halted = re.search(r"^EIP=\S+ EFL=([0-9a-f]{8}) .* HLT=1\s*$", registers, re.M)
    checks.check("the CPU is halted with interrupts off",
                 halted is not None and int(halted.group(1), 16) & 0x200 == 0)
    checks.check("the panic is on screen",
                 any(line.startswith("KERNEL PANIC: page fault") for line in screen)
                 and any(line.startswith("System halted.") for line in screen))


class Checks:
    def __init__(self):
        self.failed = 0

    def check(self, name, ok, detail=""):
        print("%s  %s%s" % ("PASS" if ok else "FAIL", name, ": " + detail if detail else ""))
        if not ok:
            self.failed += 1


def run_qemu(iso, out_dir):
    sock_path = os.path.join(out_dir, "monitor.sock")
    if os.path.exists(sock_path):
        os.remove(sock_path)
    qemu = subprocess.Popen(
        [
            "qemu-system-i386",
            "-cdrom", iso,
            "-display", "none",
            "-qmp", "unix:%s,server,nowait" % sock_path,
            "-debugcon", "file:%s" % os.path.join(out_dir, "debugcon.log"),
            "-no-reboot",
        ]
    )
    return qemu, sock_path


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: boot_check.py ISO KERNEL OUT_DIR")
    iso, kernel, out_dir = sys.argv[1:]
    out_dir = os.path.abspath(out_dir)
    os.makedirs(out_dir, exist_ok=True)
    checks = Checks()

    qemu, sock_path = run_qemu(iso, out_dir)
    monitor = None
    try:
        monitor = Monitor(sock_path)
        booted = wait_for_boot(monitor, out_dir)
        checks.check("kernel booted and printed 42", booted)
        wait_for_quiet(os.path.join(out_dir, "debugcon.log"))

        registers = monitor.human("info registers")
        with open(os.path.join(out_dir, "registers.txt"), "w") as f:
            f.write(registers)

        screen1 = save_screen(monitor, out_dir, "screen-1")
        monitor.human("sendkey f2")
        time.sleep(1)
        screen2 = save_screen(monitor, out_dir, "screen-2")
        monitor.human("sendkey f3")
        time.sleep(1)
        screen3 = save_screen(monitor, out_dir, "screen-3")

        # Panic demos, on the free screen: two that the kernel survives,
        # then a fatal one.
        for key in ("f4", "f5", "f6", "f7"):
            monitor.human("sendkey " + key)
            time.sleep(1)
        wait_for_quiet(os.path.join(out_dir, "debugcon.log"))
        panic_screen = save_screen(monitor, out_dir, "panic")
        panic_registers = monitor.human("info registers")
        with open(os.path.join(out_dir, "registers-panic.txt"), "w") as f:
            f.write(panic_registers)
    finally:
        if monitor:
            monitor.quit()
        try:
            qemu.wait(timeout=10)
        except subprocess.TimeoutExpired:
            qemu.kill()

    print("---- registers ----\n" + registers)
    print("---- screen F1 ----\n" + "\n".join(screen1))
    print("---- screen F2 ----\n" + "\n".join(screen2))
    print("---- screen F3 ----\n" + "\n".join(screen3))
    print("---- screen after the panic demos ----\n" + "\n".join(panic_screen))

    with open(os.path.join(out_dir, "debugcon.log"), encoding="latin-1") as f:
        log = f.read()

    checks.check("F2 switched to another screen", screen1 != screen2)
    check_registers(checks, registers)
    check_gdt_dump(checks, log)
    check_stack_dump(checks, log, kernel)
    checks.check("screen F1 shows the GDT dump",
                 any(line.startswith("GDTR: base=0x00000800") for line in screen1))
    checks.check("screen F2 shows the stack dump",
                 any(line.startswith("kernel stack:") for line in screen2)
                 and any(line.startswith("backtrace") for line in screen2))

    checks.check("screen F3 shows the memory report",
                 any(line.startswith("RAM: ") for line in screen3)
                 and any(line.startswith("vmalloc(") for line in screen3))
    check_memory(checks, log, registers)
    check_panics(checks, log, panic_registers, panic_screen)

    sys.exit(1 if checks.failed else 0)


if __name__ == "__main__":
    main()
