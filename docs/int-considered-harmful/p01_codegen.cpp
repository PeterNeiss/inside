// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Performance exhibit: when both operands are integer-aligned on the same
// grid, `a + b` must lower to a plain machine add -- no branch into the
// checked/clamp/rational fallback.
//
//   g++ -std=c++26 -O2 -I ../../include -c p01_codegen.cpp -o p01.o
//   objdump -d p01.o
//
// The repo enforces exactly this in CI: tests/beman/inside/check_codegen.sh disassembles
// the kernels and fails the build if any of them contains a `call`.

#include <beman/inside/inside.hpp>
#include <beman/inside/units.hpp>

using namespace beman::inside;

using val = inside<{0, 1000}>;

// The inside version.
[[gnu::noinline]] auto inside_add(val a, val b)
{
  return a + b;   // inside<{0, 2000}>: the sum widens, nothing to check
}

// The native version, for comparison.
[[gnu::noinline]] int native_add(int a, int b)
{
  return a + b;
}

// A unit conversion rescales the grid and keeps the raw.
using km = units::quantity<units::si::kilo(units::si::metre), inside<{{0, 100}, per<1000>}>>;

[[gnu::noinline]] auto to_mm(km d)
{
  return d.in(units::si::milli(units::si::metre));
}

// A loop, to check that vectorization still happens. In-place += can leave
// [0, 1000], so a checked element is checked on every pass and the loop stays
// scalar; unsafe states that the caller keeps it in range.
using fast = inside<{0, 1000}, unsafe>;

[[gnu::noinline]] void inside_loop(fast* data, int n)
{
  for (int i = 0; i < n; ++i)
    data[i] += 1_ins;
}

[[gnu::noinline]] void native_loop(int* data, int n)
{
  for (int i = 0; i < n; ++i)
    data[i] += 1;
}
