#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# Benchmark gate: build bench.test.cpp from the working tree against
# <include-dir>, run it pinned to core 3, and write its nanobench table to
# build/perf-gate/out/<label>.md. Compare two labels with insdiff.py.
#
# Usage: gate.sh <label> <include-dir>
# nanobench comes from a BEMAN_INSIDE_BUILD_TOOLS=ON build; set NANOBENCH_INCLUDE
# when it is not in build/p26-bench.
set -euo pipefail
R=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
G=$R/build/perf-gate
NB=${NANOBENCH_INCLUDE:-$R/build/p26-bench/_deps/nanobench-src/src/include}
L=${1:?label}; INC=${2:?include dir}
mkdir -p "$G/bin" "$G/out"
g++-16 -I"$INC" -isystem "$NB" -O3 -DNDEBUG -std=c++26 -freflection -mfma \
  "$R/tests/beman/inside/bench.test.cpp" -o "$G/bin/bench-$L"
taskset -c 3 "$G/bin/bench-$L" "$G/out/$L.md" > /dev/null
echo "wrote $G/out/$L.md"
