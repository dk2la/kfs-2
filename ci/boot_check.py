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
    iso, _kernel, out_dir = sys.argv[1:]
    out_dir = os.path.abspath(out_dir)
    os.makedirs(out_dir, exist_ok=True)
    checks = Checks()

    qemu, sock_path = run_qemu(iso, out_dir)
    monitor = Monitor(sock_path)
    try:
        booted = wait_for_boot(monitor, out_dir)
        checks.check("kernel booted and printed 42", booted)

        registers = monitor.human("info registers")
        with open(os.path.join(out_dir, "registers.txt"), "w") as f:
            f.write(registers)

        screen1 = save_screen(monitor, out_dir, "screen-1")
        monitor.human("sendkey f2")
        time.sleep(1)
        screen2 = save_screen(monitor, out_dir, "screen-2")
    finally:
        monitor.quit()
        try:
            qemu.wait(timeout=10)
        except subprocess.TimeoutExpired:
            qemu.kill()

    print("---- registers ----\n" + registers)
    print("---- screen F1 ----\n" + "\n".join(screen1))
    print("---- screen F2 ----\n" + "\n".join(screen2))

    checks.check("F2 switched to another screen", screen1 != screen2)

    sys.exit(1 if checks.failed else 0)


if __name__ == "__main__":
    main()
