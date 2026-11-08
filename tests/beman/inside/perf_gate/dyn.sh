#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# Instructions per iteration of a kernel, measured with perf stat on core 3:
# build <driver.cpp> with -DITERS=<N>, run it with argv[1] "0" (setup only)
# and "1" (setup plus N iterations of the kernel), and divide the difference
# by N. The cpu_core PMU is the performance cores' counter on a hybrid CPU.
#
# Usage: dyn.sh <include-dir> <driver.cpp> [N]      (N defaults to 10^7)
R=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
G=$R/build/perf-gate
mkdir -p "$G/bin"
INC=$1; SRC=$2; N=${3:-10000000}
B=$(mktemp -p "$G/bin" dynXXXX)
g++-16 -I"$INC" -O3 -DNDEBUG -std=c++26 -freflection -mfma "$SRC" -o $B -DITERS=$N || exit 1
base=$(taskset -c 3 perf stat -x, -e cpu_core/instructions/u $B 0 2>&1 >/dev/null | cut -d, -f1)
full=$(taskset -c 3 perf stat -x, -e cpu_core/instructions/u $B 1 2>&1 >/dev/null | cut -d, -f1)
python3 -c "print(f'{($full-$base)/$N:.2f} ins/iter')"
rm -f $B
