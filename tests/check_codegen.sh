#!/usr/bin/env bash
# Codegen guard: compile tests/perf_codegen.cpp at -O2 and assert the arithmetic
# fast paths did not regress — see that file's header for the rationale.
#
# Usage: check_codegen.sh <cxx> <source> <include-dir> <std>
# Exits non-zero (with the offending disassembly) if either invariant fails.
set -euo pipefail

CXX="${1:?compiler}"; SRC="${2:?source}"; INC="${3:?include dir}"; STD="${4:-23}"

OBJ="$(mktemp --suffix=.o)"
trap 'rm -f "$OBJ"' EXIT

# Force -O2 regardless of the project's build type: this guards *optimized*
# codegen, which is what ships and what a refactor can pessimize.
"$CXX" -std="c++${STD}" -O2 -I "$INC" -c "$SRC" -o "$OBJ"

DIS="$(objdump -dC --no-show-raw-insn "$OBJ")"

fail() { echo "CODEGEN GUARD FAILED: $1"; echo "----- disassembly -----"; echo "$2"; exit 1; }

# The gate: both fast paths must be call-free. A `call` means the add fell back
# to the checked / error-handler / rational path instead of inlining to a bare
# machine add — the regression this guard exists to catch (cf. the cross-grid
# addition-dispatch bug, where an operand silently dropped to the value path).
# Checked on both the scalar and the loop body, so the fallback is caught
# whether or not the loop happens to vectorize.
for fn in bnd_perf_add_fast bnd_perf_add_loop; do
  body="$(printf '%s\n' "$DIS" | sed -n "/<$fn>:/,/^\$/p")"
  [ -n "$body" ] || fail "could not find $fn in the disassembly" "$DIS"
  if printf '%s\n' "$body" | grep -qE '\bcall[a-z]*\b'; then
    fail "$fn contains a call — the integer fast path is no longer fully inlined" "$body"
  fi
done

# Vectorization is reported but NOT gated: whether the loop autovectorizes at -O2
# varies by compiler version (e.g. GCC 14 vs 15 on the same loop), so it is too
# brittle to fail on. A packed-integer add (SSE2 paddd/paddq, AVX vpaddd/vpaddq)
# means it still vectorizes; its absence is a soft heads-up, not a failure.
loop="$(printf '%s\n' "$DIS" | sed -n '/<bnd_perf_add_loop>:/,/^$/p')"
if printf '%s\n' "$loop" | grep -qE '\b(paddd|paddq|vpaddd|vpaddq)\b'; then
  echo "codegen guard OK: fast paths are call-free; loop vectorizes (packed add)"
else
  echo "codegen guard OK: fast paths are call-free; NOTE: loop did not vectorize at -O2 (informational)"
fi
