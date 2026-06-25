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

# (1) The scalar fast path must be call-free: a `call` here means the add fell
#     back to the checked / error-handler / rational path instead of inlining.
fast="$(printf '%s\n' "$DIS" | sed -n '/<bnd_perf_add_fast>:/,/\bret\b/p')"
[ -n "$fast" ] || fail "could not find bnd_perf_add_fast in the disassembly" "$DIS"
if printf '%s\n' "$fast" | grep -qE '\bcall[a-z]*\b'; then
  fail "bnd_perf_add_fast contains a call — the integer fast path is no longer fully inlined" "$fast"
fi

# (2) The loop must still vectorize: a packed-integer add (SSE2 paddd/paddq or
#     AVX vpaddd/vpaddq) proves the fast path stayed autovectorizable.
loop="$(printf '%s\n' "$DIS" | sed -n '/<bnd_perf_add_loop>:/,/^$/p')"
[ -n "$loop" ] || fail "could not find bnd_perf_add_loop in the disassembly" "$DIS"
if ! printf '%s\n' "$loop" | grep -qE '\b(paddd|paddq|vpaddd|vpaddq)\b'; then
  fail "bnd_perf_add_loop did not vectorize (no packed integer add)" "$loop"
fi

echo "codegen guard OK: scalar fast path is call-free and the loop vectorizes"
