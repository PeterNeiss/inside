#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Differential oracle: runs the dumper and recomputes every case exactly.

  check.py [--seed S] [--n N] -- <dumper command...>

The dumper (beman.inside.differential) prints one case per line; this
script recomputes each result with Python's Fraction (and π to 200 digits)
and reports every mismatch. In a scheduled CI run (GITHUB_EVENT_NAME is
"schedule") it takes a random seed and 20 times the cases, and prints the
seed so a failure can be reproduced.
"""
import argparse
import math
import os
import random
import struct
import subprocess
import sys
from fractions import Fraction as F

PI = F(
    "3.14159265358979323846264338327950288419716939937510582097494459230781"
    "640628620899862803482534211706798214808651328230664709384460955058223"
    "17253594081284811174502841027019385211055596446229489549303820"
)


def num(s):
    """A printed value: a decimal, N/D, or a hex double."""
    if "x" in s:
        return F(float.fromhex(s))
    return F(s)


def round_to(x, step, mode, base=F(0)):
    """x onto base + k·step by mode (ties: nearest away from zero, even to an even k)."""
    t = (x - base) / step
    fl = math.floor(t)
    if t == fl:
        k = fl
    elif mode == "floor":
        k = fl
    elif mode == "ceil":
        k = fl + 1
    elif mode == "trunc":
        k = fl if x >= 0 else fl + 1
    else:
        d = t - fl
        if d != F(1, 2):
            k = fl + 1 if d > F(1, 2) else fl
        elif mode == "nearest":
            k = fl + 1 if x > 0 else fl
        else:  # even
            k = fl + 1 if (base / step + fl) % 2 else fl
    return base + k * step


def quotient(a, b, mode):
    x = a / b
    fl = math.floor(x)
    if mode == "trunc":
        return math.trunc(x)
    if mode == "floor":
        return fl
    if mode == "ceil":
        return math.ceil(x)
    d = x - fl
    if d != F(1, 2):
        return fl + 1 if d > F(1, 2) else fl
    if mode == "nearest":
        return fl + 1 if x > 0 else fl
    return fl + 1 if fl % 2 else fl


def farey(x, n):
    """The neighbours of x among the fractions with denominators up to n."""
    if x.denominator <= n:
        return x, x
    fl = math.floor(x)
    f = x - fl
    a, b, c, d = 0, 1, 1, 1
    while b + d <= n:
        m = F(a + c, b + d)
        if m < f:
            a, b = a + c, b + d
        else:
            c, d = a + c, b + d
    return fl + F(a, b), fl + F(c, d)


def bounded(x, n, mode):
    lo, hi = farey(x, n)
    if lo == hi:
        return lo
    if mode == "floor":
        return lo
    if mode == "ceil":
        return hi
    if mode == "trunc":
        return lo if x > 0 else hi
    if x - lo != hi - x:
        return lo if x - lo < hi - x else hi
    return hi if x > 0 else lo


def float32_bits(x):
    """x rounded to the nearest float32, ties to even, as its bit pattern."""
    if x == 0:
        return 0
    sign = -1 if x < 0 else 1
    a = abs(x)
    e = a.numerator.bit_length() - a.denominator.bit_length()
    if F(2) ** e > a:
        e -= 1
    if F(2) ** (e + 1) <= a:
        e += 1
    ulp = F(2) ** (max(e, -126) - 23)
    q = a / ulp
    k = math.floor(q)
    r = q - k
    if r > F(1, 2) or (r == F(1, 2) and k % 2):
        k += 1
    v = sign * k * ulp
    if abs(v) >= F(2) ** 128:
        return struct.unpack("<I", struct.pack("<f", sign * float("inf")))[0]
    return struct.unpack("<I", struct.pack("<f", float(v)))[0]


class Checker:
    def __init__(self):
        self.cases = 0
        self.failures = 0

    def expect(self, ok, line, what):
        self.cases += 1
        if not ok:
            self.failures += 1
            if self.failures <= 20:
                print(f"MISMATCH ({what}): {line}")

    def value(self, got, want, line, what):
        """got: a printed result; want: a Fraction, or an error code string."""
        if isinstance(want, str):
            self.expect(got == want, line, f"{what}: want {want}")
        else:
            self.expect(not got.startswith(("E", "N")) and num(got) == want, line, f"{what}: want {want}")

    def arith(self, f, line):
        a, b = num(f[2]), num(f[3])
        want = [a + b, a - b, a * b, a / b if b else "E2"]
        for got, w, what in zip(f[4:8], want, ["+", "-", "*", "/"]):
            if got != "N":
                self.value(got, w, line, what)
        self.expect(f[8] == str(int(a < b)) and f[9] == str(int(a == b)), line, "compare")

    def mod(self, f, line):
        mode, a, b = f[1], num(f[2]), num(f[3])
        if b == 0:
            self.expect(f[4:7] == ["E2", "E2", "E"], line, "% by 0")
            return
        q = quotient(a, b, mode)
        r = a - q * b
        self.value(f[4], r, line, "%")
        self.value(f[5], q, line, "divmod quotient")
        self.value(f[6], r, line, "divmod remainder")

    def store(self, f, line):
        mode, clamped = f[1], f[2] == "1"
        lo, hi, notch, v = num(f[3]), num(f[4]), num(f[5]), num(f[6])
        r = round_to(v, notch, mode, lo)
        if r < lo or r > hi:
            want = (lo if r < lo else hi) if clamped else "E3"
        else:
            want = r
        self.value(f[7], want, line, f"store {mode}")

    def bounded(self, f, line):
        a, d = num(f[1]), num(f[2])
        want = [a + d, a - d, a * d, a / d, d / a if a else "E2"]
        for got, w, what in zip(f[3:8], want, ["+", "-", "*", "/", "reversed /"]):
            self.value(got, w, line, what)
        v = (a * d) / (d + d * d)
        for got, mode in zip(f[8:12], ["nearest", "floor", "ceil", "trunc"]):
            self.value(got, bounded(v, 1000, mode), line, f"bounded {mode}")
        self.value(f[12], v if v.denominator <= 1000 else "E4", line, "bounded exact")
        x = num(f[13])
        for got, mode in zip(f[14:17], ["nearest", "floor", "ceil"]):
            self.value(got, bounded(x, 1000, mode), line, f"bounded double {mode}")

    def floats(self, f, line):
        x = num(f[1])
        self.expect(int(f[2]) == float32_bits(x), line, f"to<float>: want bits {float32_bits(x)}")
        self.expect(float.fromhex(f[3]) == float(x), line, f"to<double>: want {float(x).hex()}")

    def pi(self, f, line):
        mode, factor, notch, x = f[1], f[2], num(f[3]), num(f[4])
        exact = x * PI / 180 if factor == "deg2rad" else x * 180 / PI
        self.value(f[5], round_to(exact, notch, mode), line, f"{factor} {mode}")

    def run(self, lines):
        families = {"A": self.arith, "M": self.mod, "S": self.store, "B": self.bounded, "F": self.floats, "P": self.pi}
        for line in lines:
            f = line.split()
            if not f:
                continue
            fn = families.get(f[0])
            if fn is None:
                self.expect(False, line, "unknown family")
                continue
            try:
                fn(f, line)
            except (ValueError, ZeroDivisionError, IndexError) as e:
                self.expect(False, line, f"unreadable: {e}")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--n", type=int, default=100)
    p.add_argument("command", nargs=argparse.REMAINDER)
    args = p.parse_args()
    command = [c for c in args.command if c != "--"]
    seed, n = args.seed, args.n
    if os.environ.get("GITHUB_EVENT_NAME") == "schedule":
        seed, n = random.SystemRandom().randrange(1, 2**63), n * 20
    print(f"differential: seed={seed} n={n} (reproduce: check.py --seed {seed} --n {n} -- <dumper>)")
    out = subprocess.run(command + [str(seed), str(n)], capture_output=True, text=True)
    if out.returncode != 0:
        print(out.stdout[-2000:], out.stderr[-2000:])
        print(f"dumper failed with exit code {out.returncode}")
        return 1
    c = Checker()
    c.run(out.stdout.splitlines())
    print(f"differential: {c.cases} checks, {c.failures} mismatches")
    return 1 if c.failures or c.cases == 0 else 0


if __name__ == "__main__":
    sys.exit(main())
