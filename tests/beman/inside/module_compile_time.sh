#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# Compile time of the headers against the modules (modules/): the best of
# three wall times for three TUs, each once with #include and once with
# import,
#   base     the core, nothing used
#   plain    N inside expressions (units_compile_time.sh's plain TU)
#   exprs    N quantity expressions (units_compile_time.sh's exprs TU)
# plus the one-off time to build the two modules. The modules come from a
# build directory configured with BEMAN_INSIDE_BUILD_MODULES=ON, by the same
# compiler and standard. Not a ctest gate (wall time is noisy); the results
# go to docs/performance.md.
#
# Usage: module_compile_time.sh <cxx> <include-dir> <module-build-dir> [<std> [<N>]]
set -euo pipefail
export LC_ALL=C

CXX="${1:?compiler}"; INC="${2:?include dir}"; BUILD="${3:?module build dir}"; STD="${4:-23}"; N="${5:-200}"
DIR="$(mktemp -d)"
trap 'rm -rf "$DIR"' EXIT
BUILD="$(cd "$BUILD" && pwd)"
BMI_DIR="$BUILD/modules/CMakeFiles/beman.inside.module.dir"

# How the compiler finds the two modules' BMIs.
case "$CXX" in
  *clang*)
    IMPORT_FLAGS=(-fmodule-file=beman.inside="$BMI_DIR/beman.inside.pcm"
                  -fmodule-file=beman.inside.units="$BMI_DIR/beman.inside.units.pcm") ;;
  *)
    printf 'beman.inside %s\nbeman.inside.units %s\n' "$BMI_DIR/beman.inside.gcm" "$BMI_DIR/beman.inside.units.gcm" > "$DIR/map.txt"
    IMPORT_FLAGS=(-fmodules -fmodule-mapper="$DIR/map.txt") ;;
esac

# body <kind>: the statements of a TU after its includes or imports.
body() {
  echo 'using namespace beman::inside;'
  [[ $1 == exprs ]] && echo 'using namespace beman::inside::units;'
  echo 'int sink(const auto& q) { return static_cast<int>(sizeof(q)); }'
  echo 'int main() {'
  echo '    int s = 0;'
  if [[ $1 != base ]]; then
    for ((i = 1; i <= N; ++i)); do
      if [[ $1 == plain ]]; then
        case $((i % 5)) in
          0) echo "    s += sink(inside<{0, $i}>{1} * just<1000> + just<$i>);" ;;
          1) echo "    s += sink(just<$i> * inside<{0, $i}>{1});" ;;
          2) echo "    s += sink(inside<{0, $i}>{1} * just<rational{18, 5}>);" ;;
          3) echo "    s += sink(inside<{0, $i}>{1} + inside<{0, $i}>{1});" ;;
          4) echo "    s += sink(inside<{-$i, $i}>{0} + just<rational{27315, 100}>);" ;;
        esac
      else
        case $((i % 5)) in
          0) echo "    s += sink(quantity<si::kilo(si::metre), inside<{0, $i}>>{1} + just<$i> * si::metre);" ;;
          1) echo "    s += sink((just<$i> * si::newton) * (quantity<si::metre, inside<{0, $i}>>{1}));" ;;
          2) echo "    s += sink(quantity<si::metre / si::second, inside<{0, $i}>>{1}.in(si::kilo(si::metre) / si::hour));" ;;
          3) echo "    s += sink(quantity<isq::height[si::metre], inside<{0, $i}>>{1} + quantity<isq::width[si::metre], inside<{0, $i}>>{1});" ;;
          4) echo "    s += sink(point<si::degree_Celsius>(inside<{-$i, $i}>{0}).point_for(si::absolute_zero));" ;;
        esac
      fi
    done
  fi
  echo '    return s == 0;'
  echo '}'
}
for kind in base plain exprs; do
  if [[ $kind == exprs ]]; then
    hdr='#include <beman/inside/units.hpp>'; imp='import beman.inside.units;'
  else
    hdr='#include <beman/inside/inside.hpp>'; imp='import beman.inside;'
  fi
  { echo "$hdr"; body $kind; } > "$DIR/$kind.include.cpp"
  { echo "$imp"; body $kind; } > "$DIR/$kind.import.cpp"
done

best() {
  local b=""
  for _ in 1 2 3; do
    local t0 t1
    t0=$(date +%s.%N)
    "$@" > /dev/null
    t1=$(date +%s.%N)
    b=$(awk -v t0="$t0" -v t1="$t1" -v b="$b" 'BEGIN { d = t1 - t0; print (b == "" || d < b) ? d : b }')
  done
  awk -v b="$b" 'BEGIN { printf "%.2f", b }'
}
# The flags CMake builds the modules with (a BMI only loads into a TU
# compiled the same way); both variants use them.
FLAGS=(-std="gnu++$STD")
[[ $(uname -m) == x86_64 ]] && FLAGS+=(-mfma)
compile_include() { "$CXX" "${FLAGS[@]}" -I "$INC" -c "$1" -o /dev/null; }
# (From the build directory: GCC's units BMI names the core BMI by a path
# relative to it.)
compile_import() { (cd "$BUILD" && "$CXX" "${FLAGS[@]}" "${IMPORT_FLAGS[@]}" -c "$1" -o /dev/null); }

line="$CXX -std=gnu++$STD:"
for kind in base plain exprs; do
  line+=" $kind $(best compile_include "$DIR/$kind.include.cpp")s → $(best compile_import "$DIR/$kind.import.cpp")s,"
done
# The modules themselves: rebuild both interface units once.
t0=$(date +%s.%N)
touch "$(dirname "$0")/../../../modules/beman.inside.cppm"
cmake --build "$BUILD" --target beman.inside.module > /dev/null
t1=$(date +%s.%N)
line+=" building the modules $(awk -v t0="$t0" -v t1="$t1" 'BEGIN { printf "%.2f", t1 - t0 }')s"
echo "$line"
