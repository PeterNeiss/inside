#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Recompute certify's result hash from its --dump output with hashlib.

    check_hash.py -- <certify> --pair atanh --dump

Each pair's dump is a "# function input -> output" line, one record per
input ("n/d", "-n/d" or "errc k"), and a "# sha256 <hex>" line. The script
rebuilds each 17-byte record (certify.hpp has the layout), hashes them and
compares the digest with the program's: the hash is defined by the records,
not by one implementation of SHA-256.
"""

import hashlib
import subprocess
import sys


def record(text):
    if text.startswith("errc "):
        return bytes([2]) + int(text[5:]).to_bytes(8, "little") + (0).to_bytes(8, "little")
    kind = 1 if text.startswith("-") else 0
    n, d = text.lstrip("-").split("/")
    return bytes([kind]) + int(n).to_bytes(8, "little") + int(d).to_bytes(8, "little")


def main():
    command = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    out = subprocess.run(command, capture_output=True, text=True)
    pairs, h, name = 0, None, None
    for line in out.stdout.splitlines():
        if line.startswith("# sha256 "):
            expected = line.split()[2]
            if h.hexdigest() != expected:
                print(f"check_hash: {name}: hashlib {h.hexdigest()}, certify {expected}")
                return 1
            pairs += 1
        elif line.startswith("# "):
            name, h = line[2:], hashlib.sha256()
        elif h is not None and line:
            h.update(record(line))
    if pairs == 0:
        print("check_hash: no pair dumped\n" + out.stderr)
        return 1
    print(f"check_hash: {pairs} pair hashes recomputed with hashlib, all equal")
    return 0


if __name__ == "__main__":
    sys.exit(main())
