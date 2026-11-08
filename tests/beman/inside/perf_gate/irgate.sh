#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# Ir gate: the cachegrind instruction count (Ir) of each of perf_workload's 13
# keys, built from the working tree against two include dirs (a, then b),
# and the change per key. Both builds use the same binary path: the length
# of argv[0] shifts Ir by up to ±14. Ir does not depend on the machine's
# load; ±14..47 on the non-math keys is code-layout noise.
#
# Usage: irgate.sh <include-a> <include-b>      (needs valgrind)
# A baseline include dir: git archive HEAD include | tar -x -C <dir>
R=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
G=$R/build/perf-gate
mkdir -p "$G/bin" "$G/out"
keys="integer_qformat integer_mul qformat_div cross_grid_assign checked_add rational_store rational_compare math_table math_double math_double_fine math_dd math_integer math_decimal"
for side in a b; do
  inc=$1; [ $side = b ] && inc=$2
  g++-16 -I"$inc" -O3 -DNDEBUG -std=gnu++26 -freflection -mfma "$R/tests/beman/inside/perf_workload.test.cpp" -o "$G/bin/pw" || exit 1
  for k in $keys; do
    valgrind --tool=cachegrind --cache-sim=no --cachegrind-out-file=/dev/null "$G/bin/pw" $k 2>&1 >/dev/null | awk -v k=$k -v s=$side '/I *refs/{gsub(",","",$NF); print s, k, $NF}'
  done
done > "$G/out/ir.txt"
python3 -I - "$G/out/ir.txt" <<'PY'
import sys
v = {}
for line in open(sys.argv[1]):
    s, k, n = line.split(); v.setdefault(k, {})[s] = int(n)
for k in sorted(v):
    a, b = v[k]['a'], v[k]['b']
    print(f"{k:18s} {a:12d} {b:12d} {100*(b-a)/a:+8.2f}%")
PY
