#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# Kernel asm counts: compile <kernels.cpp> at -O3 against <include-dir> and
# print, for each function named k_*, the instructions in its hot section,
# its divisions, whether it has a stack canary, and its calls (plus the
# instructions of its cold clone). With a third argument, the asm too.
#
# Usage: kern.sh <include-dir> <kernels.cpp> [dump]
# <kernels.cpp> holds one [[gnu::noinline]] non-template function per
# operation, named k_*, taking its operands by value:
#   [[gnu::noinline]] auto k_add(inside<{0, 100}> a, inside<{0, 100}> b) { return a + b; }
INC=$1; SRC=$2
g++-16 -I"$INC" -O3 -DNDEBUG -std=c++26 -freflection -mfma -fno-asynchronous-unwind-tables -masm=intel -S -o - "$SRC" 2>&1 | c++filt |
awk -v dump="$3" '
/^"?k_[a-z0-9_]*\(/ { if (index($0, "[clone .cold]")) { cold=1 } else { cold=0; name=$0; sub(/\(.*/, "", name); gsub(/"/, "", name); order[++n]=name } if (dump) print "## " name (cold ? " [cold]" : ""); p=1; next }
p && /^\t\.size/ { p=0 }
p && /^\t[a-z]/ { if (dump) print (cold ? "  [cold] " : "  ") $0
  if (cold) { cins[name]++; next }
  ins[name]++; if ($1 ~ /^v?(i)?div/ || $1 ~ /^vdiv|^vsqrt/) divs[name]++
  if ($0 ~ /fs:40/) can[name]++; if ($1 == "call") calls[name]++ }
END { for (i=1;i<=n;i++){k=order[i]; printf "%-22s ins %4d  div %d  canary %d  call %d  (cold %d)\n", k, ins[k], divs[k], (can[k]>0), calls[k], cins[k]} }'
