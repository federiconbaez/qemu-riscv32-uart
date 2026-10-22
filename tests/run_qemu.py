#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Integration tests: drive the firmware's serial console under QEMU.

Each scenario boots a fresh QEMU, waits for the prompt, sends lines on the
emulated UART and checks the replies and QEMU's exit status (set by the
firmware through the sifive_test finisher).
"""
import argparse
import os
import re
import select
import subprocess
import sys
import time

TIMEOUT = 10.0


class Console:
    def __init__(self, qemu, elf):
        self.p = subprocess.Popen(
            [qemu, "-machine", "virt", "-bios", "none", "-display", "none",
             "-serial", "stdio", "-monitor", "none", "-kernel", elf],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0)
        self.buf = b""

    def expect(self, pattern, timeout=TIMEOUT):
        rx = re.compile(pattern.encode() if isinstance(pattern, str) else pattern)
        deadline = time.monotonic() + timeout
        while True:
            m = rx.search(self.buf)
            if m:
                self.buf = self.buf[m.end():]
                return m
            left = deadline - time.monotonic()
            if left <= 0:
                raise AssertionError(f"timeout waiting for {pattern!r}; got {self.buf[-300:]!r}")
            r, _, _ = select.select([self.p.stdout], [], [], left)
            if r:
                chunk = os.read(self.p.stdout.fileno(), 4096)
                if not chunk:
                    raise AssertionError(f"QEMU exited waiting for {pattern!r}; got {self.buf[-300:]!r}")
                self.buf += chunk

    def send(self, data):
        self.p.stdin.write(data.encode() if isinstance(data, str) else data)
        self.p.stdin.flush()

    def cmd(self, line, reply):
        self.expect(r"> ")
        self.send(line + "\r")
        return self.expect(reply)

    def wait(self, timeout=TIMEOUT):
        try:
            return self.p.wait(timeout)
        finally:
            if self.p.poll() is None:
                self.p.kill()


def boot(args):
    c = Console(args.qemu, args.elf)
    c.expect(r"rv32 uart demo \(hart 0, misa=[0-9a-f]+\)")
    c.expect(r"selftest: 5/5 passed")
    c.expect(r"ready\r\n")
    return c


def t_commands(args):
    c = boot(args)
    c.cmd("ping", r"\r\npong\r\n")
    c.cmd("add 2 40", r"\r\n42\r\n")
    c.cmd("add -5 0x10", r"\r\n11\r\n")
    c.cmd("add 2147483647 1", r"error: overflow")
    c.cmd("add -2147483648 -1", r"error: overflow")
    c.cmd("add 1", r"error: usage")
    c.cmd("add 1 2 3", r"error: usage")
    c.cmd("echo hola mundo", r"\r\nhola mundo\r\n")
    c.cmd("frobnicate", r"unknown command: frobnicate")
    c.cmd("traps", r"ecall=2 illegal=2")
    c.cmd("exit", r"bye")
    assert c.wait() == 0


def t_line_editing(args):
    c = boot(args)
    c.expect(r"> ")
    c.send("pinx\x7fg\r")  # backspace fixes the typo
    c.expect(r"\r\npong\r\n")
    c.expect(r"> ")
    c.send("   \r")  # blank line is ignored and re-prompts
    long = "x" * 300  # longer than the 96-byte line buffer: truncated, no crash
    c.cmd(long, r"unknown command: x{95}\r\n")
    c.cmd("ping", r"pong")
    c.cmd("exit", r"bye")
    assert c.wait() == 0


def t_timer_runs(args):
    c = boot(args)
    m1 = c.cmd("ticks", r"ticks=(\d+)\r\n")
    time.sleep(0.3)
    m2 = c.cmd("ticks", r"ticks=(\d+)\r\n")
    t1, t2 = int(m1.group(1)), int(m2.group(1))
    # 100 Hz nominal; QEMU under load may lag, so only require clear progress.
    assert t2 - t1 >= 10, (t1, t2)
    u = int(c.cmd("uptime", r"uptime_ms=(\d+)\r\n").group(1))
    assert u >= 300, u
    c.cmd("exit", r"bye")
    assert c.wait() == 0


def t_fail_exit_code(args):
    c = boot(args)
    c.cmd("fail 0", r"error: fail needs 1..255")
    c.cmd("fail 7", r"failing with 7")
    rc = c.wait()
    assert rc == 7, rc


def t_burst_input(args):
    # Several commands written at once; the polled driver must not lose
    # bytes thanks to the 16550 FIFO plus QEMU's flow control.
    c = boot(args)
    c.expect(r"> ")
    c.send("ping\rping\rping\r")
    for _ in range(3):
        c.expect(r"pong\r\n")
    c.cmd("exit", r"bye")
    assert c.wait() == 0


TESTS = [t_commands, t_line_editing, t_timer_runs, t_fail_exit_code, t_burst_input]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--qemu", default="qemu-system-riscv32")
    ap.add_argument("--elf", default="build/firmware.elf")
    args = ap.parse_args()
    failed = 0
    for t in TESTS:
        start = time.monotonic()
        try:
            t(args)
            print(f"PASS {t.__name__} ({time.monotonic() - start:.2f}s)")
        except AssertionError as e:
            failed += 1
            print(f"FAIL {t.__name__}: {e}")
    print(f"{len(TESTS) - failed}/{len(TESTS)} qemu tests passed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
