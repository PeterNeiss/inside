#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# How long the names of inside and quantity types are: for a fixed set of
# types, the mangled name of one function instantiated on the type, and the
# type as a diagnostic prints it. Compiles through stdin; writes no files.
# Not a ctest (the numbers are compiler output, not pass/fail);
# the CHANGELOG records a run.
#
# Usage: symbol_lengths.sh <cxx> <include-dir>
set -euo pipefail
export LC_ALL=C

CXX="${1:?compiler}"; INC="${2:?include dir}"
FLAGS=(-std=c++26 -freflection -O0 -I "$INC")

# name|type
TYPES=(
  'inside {0, 100}|id<"[0..100]">'
  'inside 1/100, half away|id<"[0..1] 1/100 round_half_away">'
  'inside continuous|id<"[0..1] <1000>">'
  'm|id<"[0..100] [m]">'
  'km|id<"[0..100] [km]">'
  'km/h|id<"[0..100] [km/h]">'
  'J|id<"[0..100] [J]">'
  'N|id<"[0..100] [N]">'
  'L|id<"[0..100] [L]">'
  'ft|id<"[0..100] [ft]">'
  'S|id<"[0..100] [S]">'
  'velocity|id<"[0..100] [m/s]{velocity}">'
  'point in C|decltype(point<si::degree_Celsius>(id<"[-50..50]">{20}))'
  'ftm, unit<>|id<"[0..100] [ftm]">'
  'ftm, defined_by|id<"[0..100] [ftm]">'
  'cts, base_unit<>|id<"[0..100] [cts]">'
  'cts, defined_by|id<"[0..100] [cts]">'
)

# Units of your own: one-line (content-keyed) and named (defined_by).
prologue() {
  printf '#include <inside/units.hpp>\n'
  printf 'using namespace ins;\n'
  printf 'inline constexpr auto     ftm_line = unit<"ftm", 1.8288_q * si::metre>;\n'
  printf 'inline constexpr unit_def ftm_def{"ftm", 1.8288_q * si::metre};\n'
  printf 'inline constexpr auto     ftm = defined_by<ftm_def>;\n'
  printf 'inline constexpr auto     cts_line = base_unit<"cts", "count">;\n'
  printf 'inline constexpr kind_def count_def{"count", base_dimension<"count">};\n'
  printf 'inline constexpr unit_def cts_def{"cts", defined_by<count_def>};\n'
  printf 'inline constexpr auto     cts = defined_by<cts_def>;\n'
}

printf '%-24s %8s %8s\n' "type" "mangled" "printed"
for entry in "${TYPES[@]}"; do
  name="${entry%%|*}"; type="${entry#*|}"
  mangled=$({ prologue; printf 'using T = %s;\ntemplate <class X> int f(const X&) { return 0; }\ntemplate int f(const T&);\n' "$type"; } |
    "$CXX" "${FLAGS[@]}" -x c++ - -S -o - 2>/dev/null | grep -m1 -o '^_Z1f[A-Za-z0-9_]*' | awk '{ print length($0) }' || true)
  printed=$({ prologue; printf 'using T = %s;\nint main() { T::no_such_member x; }\n' "$type"; } |
    "$CXX" "${FLAGS[@]}" -x c++ - -fsyntax-only 2>&1 | grep -m1 -o "{aka .*}" | awk '{ print length($0) - 8 }' || true)
  printf '%-24s %8s %8s\n' "$name" "${mangled:-?}" "${printed:-?}"
done
