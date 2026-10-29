#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# Compile time of units.hpp: the best of three wall times for four TUs,
#   base     inside.hpp + io.hpp (what units.hpp builds on)
#   include  units.hpp, nothing used
#   plain    the same N expressions' inside arithmetic without units (the
#            grids each one instantiates): the part units.hpp does not add
#   exprs    units.hpp and N distinct quantity expressions (mixed-unit sums,
#            products, conversions, kinds), each its own instantiations
# Not a ctest gate (wall time is noisy); docs/units.md records a run.
#
# Usage: units_compile_time.sh <cxx> <include-dir> [<std> [<N> [<flags>]]]
set -euo pipefail
export LC_ALL=C

CXX="${1:?compiler}"; INC="${2:?include dir}"; STD="${3:-26}"; N="${4:-200}"
read -r -a FLAGS <<< "${5:-}"
DIR="$(mktemp -d)"
trap 'rm -rf "$DIR"' EXIT

printf '#include <beman/inside/inside.hpp>\n#include <beman/inside/io.hpp>\nint main() {}\n' > "$DIR/base.cpp"
printf '#include <beman/inside/units.hpp>\nint main() {}\n' > "$DIR/include.cpp"
{
  echo '#include <beman/inside/units.hpp>'
  echo 'using namespace beman::inside;'
  echo 'using namespace beman::inside::units;'
  echo 'int sink(const auto& q) { return static_cast<int>(sizeof(q)); }'
  echo 'int main() {'
  echo '    int s = 0;'
  for ((i = 1; i <= N; ++i)); do
    case $((i % 5)) in
      0) echo "    s += sink(quantity<si::kilo(si::metre), inside<{0, $i}>>{1} + just<$i> * si::metre);" ;;
      1) echo "    s += sink((just<$i> * si::newton) * (quantity<si::metre, inside<{0, $i}>>{1}));" ;;
      2) echo "    s += sink(quantity<si::metre / si::second, inside<{0, $i}>>{1}.in(si::kilo(si::metre) / si::hour));" ;;
      3) echo "    s += sink(quantity<isq::height[si::metre], inside<{0, $i}>>{1} + quantity<isq::width[si::metre], inside<{0, $i}>>{1});" ;;
      4) echo "    s += sink(point<si::degree_Celsius>(inside<{-$i, $i}>{0}).point_for(si::absolute_zero));" ;;
    esac
  done
  echo '    return s == 0;'
  echo '}'
} > "$DIR/exprs.cpp"

{
  echo '#include <beman/inside/inside.hpp>'
  echo 'using namespace beman::inside;'
  echo 'int sink(const auto& q) { return static_cast<int>(sizeof(q)); }'
  echo 'int main() {'
  echo '    int s = 0;'
  for ((i = 1; i <= N; ++i)); do
    case $((i % 5)) in
      0) echo "    s += sink(inside<{0, $i}>{1} * just<1000> + just<$i>);" ;;
      1) echo "    s += sink(just<$i> * inside<{0, $i}>{1});" ;;
      2) echo "    s += sink(inside<{0, $i}>{1} * just<rational{18, 5}>);" ;;
      3) echo "    s += sink(inside<{0, $i}>{1} + inside<{0, $i}>{1});" ;;
      4) echo "    s += sink(inside<{-$i, $i}>{0} + just<rational{27315, 100}>);" ;;
    esac
  done
  echo '    return s == 0;'
  echo '}'
} > "$DIR/plain.cpp"

best() {
  local b=""
  for _ in 1 2 3; do
    local t0 t1
    t0=$(date +%s.%N)
    "$CXX" ${FLAGS[@]+"${FLAGS[@]}"} -std="c++${STD}" -freflection -I "$INC" -c "$1" -o /dev/null
    t1=$(date +%s.%N)
    b=$(awk -v t0="$t0" -v t1="$t1" -v b="$b" 'BEGIN { d = t1 - t0; print (b == "" || d < b) ? d : b }')
  done
  awk -v b="$b" 'BEGIN { printf "%.2f", b }'
}

base=$(best "$DIR/base.cpp")
incl=$(best "$DIR/include.cpp")
plain=$(best "$DIR/plain.cpp")
expr=$(best "$DIR/exprs.cpp")
echo "$CXX -std=c++$STD: base ${base}s, units.hpp ${incl}s, $N inside expressions ${plain}s, as quantities ${expr}s"
