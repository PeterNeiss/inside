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
  'inside {0, 100}|inside<{0, 100}>'
  'inside 1/100, half away|inside<{{0, 1}, per<100>}, round_half_away>'
  'inside continuous|inside<{{0, 1}, max_denominator{1000}}>'
  'm|quantity<si::metre, inside<{0, 100}>>'
  'km|quantity<si::kilo(si::metre), inside<{0, 100}>>'
  'km/h|quantity<si::kilo(si::metre) / si::hour, inside<{0, 100}>>'
  'J|quantity<si::joule, inside<{0, 100}>>'
  'N|quantity<si::newton, inside<{0, 100}>>'
  'L|quantity<si::litre, inside<{0, 100}>>'
  'ft|quantity<international::foot, inside<{0, 100}>>'
  'S|quantity<si::siemens, inside<{0, 100}>>'
  'velocity|quantity<isq::velocity[si::metre / si::second], inside<{0, 100}>>'
  'point in C|decltype(point<si::degree_Celsius>(inside<{-50, 50}>{20}))'
  'ftm, unit<>|quantity<ftm_line, inside<{0, 100}>>'
  'ftm, defined_by|quantity<ftm, inside<{0, 100}>>'
  'cts, base_unit<>|quantity<cts_line, inside<{0, 100}>>'
  'cts, defined_by|quantity<cts, inside<{0, 100}>>'
)

# Units of your own: one-line (content-keyed) and named (defined_by).
prologue() {
  printf '#include <inside/units.hpp>\n'
  printf 'using namespace ins;\n'
  printf 'inline constexpr auto     ftm_line = unit<"ftm", just<18288, 10000> * si::metre>;\n'
  printf 'inline constexpr unit_def ftm_def{"ftm", just<18288, 10000> * si::metre};\n'
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
