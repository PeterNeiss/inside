#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# Codegen guard: compile tests/beman/inside/perf_codegen.test.cpp at -O2 and assert the arithmetic
# fast paths did not regress — see that file's header for the rationale.
#
# Usage: check_codegen.sh <cxx> <source> <include-dir> <std> [<flags>]
# <flags> is the build's CMAKE_CXX_FLAGS as one string (e.g. -stdlib=libc++);
# flags that change inlining or add instrumentation (coverage, profiling,
# sanitizers, -fno-inline, optimization and debug levels) are dropped.
# Exits non-zero (with the offending disassembly) if either invariant fails.
set -euo pipefail

CXX="${1:?compiler}"; SRC="${2:?source}"; INC="${3:?include dir}"; STD="${4:-23}"
read -r -a ALL <<< "${5:-}"
FLAGS=()
for f in "${ALL[@]}"; do
  case "$f" in
    --coverage|-fprofile*|-ftest-coverage|-fsanitize*|-fno-sanitize*|-fno-inline*|-fno-default-inline|-O*|-g*) ;;
    *) FLAGS+=("$f") ;;
  esac
done

OBJ="$(mktemp --suffix=.o)"
trap 'rm -f "$OBJ"' EXIT

# Force -O2 regardless of the project's build type: this guards *optimized*
# codegen, which is what ships and what a refactor can pessimize.
"$CXX" ${FLAGS[@]+"${FLAGS[@]}"} -std="c++${STD}" -O2 -I "$INC" -c "$SRC" -o "$OBJ"

# -r prints each call's relocation (its target symbol) on the next line.
DIS="$(objdump -drC --no-show-raw-insn "$OBJ")"

fail() { echo "CODEGEN GUARD FAILED: $1"; echo "----- disassembly -----"; echo "$2"; exit 1; }

# The gate: the fast paths must be call-free. A `call` means the operation fell
# back to the checked / rational path instead of inlining to bare machine
# arithmetic — the regression this guard exists to catch (cf. the cross-grid
# addition-dispatch bug, where an operand silently dropped to the value path).
# Checked on both the scalar and the loop body, so the fallback is caught
# whether or not the loop happens to vectorize. The one call allowed is the
# range check's error report, detail::raise: checked policies (the default)
# need it on their failure branch, which GCC moves to .text.unlikely and
# Clang keeps at the end of the function.
# Calls in a function body other than to beman::inside::detail::raise.
calls_besides_raise() {
  printf '%s\n' "$1" | awk '
    pending { if ($0 !~ /beman::inside::detail::raise\(/) print call; pending = 0 }
    /[[:space:]]call[a-z]*[[:space:]]/ { call = $0; pending = 1 }
    END { if (pending) print call }'
}
for fn in ins_perf_add_fast ins_perf_add_loop ins_perf_mul_fast ins_perf_sub_compound ins_perf_to_double ins_perf_range_sum; do
  body="$(printf '%s\n' "$DIS" | sed -n "/<$fn>:/,/^\$/p")"
  [ -n "$body" ] || fail "could not find $fn in the disassembly" "$DIS"
  if [ -n "$(calls_besides_raise "$body")" ]; then
    fail "$fn contains a call — the integer fast path is no longer fully inlined" "$body"
  fi
done

# Vectorization is reported but NOT gated: whether the loop autovectorizes at -O2
# varies by compiler version (e.g. GCC 14 vs 15 on the same loop), so it is too
# brittle to fail on. A packed-integer add (SSE2 paddd/paddq, AVX vpaddd/vpaddq)
# means it still vectorizes; its absence is a soft heads-up, not a failure.
loop="$(printf '%s\n' "$DIS" | sed -n '/<ins_perf_add_loop>:/,/^$/p')"
if printf '%s\n' "$loop" | grep -qE '\b(paddd|paddq|vpaddd|vpaddq)\b'; then
  echo "codegen guard OK: fast paths are call-free (bar error reports); loop vectorizes (packed add)"
else
  echo "codegen guard OK: fast paths are call-free (bar error reports); NOTE: loop did not vectorize at -O2 (informational)"
fi
