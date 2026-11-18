#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Compare two gate.sh tables row by row: the rows whose ins/op moved by at
least <threshold> (default 0.5), with their ns/op.

Usage: insdiff.py <a.md> <b.md> [<threshold>]
"""
import re
import sys


def load(p):
    rows, title = {}, None
    for line in open(p):
        if line.startswith('| relative'):
            title = line.rsplit('|', 1)[1].strip()
            continue
        if re.match(r'\|\s*[\d.]+%\s*\|', line) and title:
            cells = [c.strip() for c in line.strip().strip('|').split('|')]
            try:
                ins = float(cells[4].replace(',', ''))
            except ValueError:
                ins = float('nan')
            m = re.search(r'`([^`]*)`', cells[-1])
            rows[(title, m.group(1) if m else cells[-1])] = (ins, float(cells[1].replace(',', '')))
    return rows


a, b = load(sys.argv[1]), load(sys.argv[2])
thr = float(sys.argv[3]) if len(sys.argv) > 3 else 0.5
out = [(b[k][0] - a[k][0], k, a[k], b[k]) for k in b if k in a and abs(b[k][0] - a[k][0]) >= thr]
for d, k, x, y in sorted(out):
    print(f"{d:+8.2f} ins  {x[0]:8.2f} -> {y[0]:8.2f}   ns {x[1]:7.2f} -> {y[1]:7.2f}   {k[0]} :: {k[1]}")
print(f"{len(out)} rows moved >= {thr}")
