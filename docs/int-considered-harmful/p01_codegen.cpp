// Performance exhibit: when both operands are integer-aligned on the same
// grid, `a + b` must lower to a plain machine add -- no branch into the
// checked/clamp/rational fallback.
//
//   g++ -std=c++23 -O2 -I ../../include -c p01_codegen.cpp -o p01.o
//   objdump -d p01.o
//
// The repo enforces exactly this in CI: tests/check_codegen.sh disassembles
// the kernels and fails the build if any of them contains a `call`.

#include <beman/inside/inside.hpp>

using namespace beman::inside;

using val = inside<{0, 1000}, unsafe>;

// The inside version.
[[gnu::noinline]] val inside_add(val a, val b)
{
  return val{a.raw() + b.raw()};
}

// The native version, for comparison.
[[gnu::noinline]] int native_add(int a, int b)
{
  return a + b;
}

// A loop, to check that vectorization still happens.
[[gnu::noinline]] void inside_loop(val* data, int n)
{
  for (int i = 0; i < n; ++i)
    data[i] += 1_ins;
}

[[gnu::noinline]] void native_loop(int* data, int n)
{
  for (int i = 0; i < n; ++i)
    data[i] += 1;
}
