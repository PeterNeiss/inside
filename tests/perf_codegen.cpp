// Codegen guard for the arithmetic fast paths.
//
// bound's headline claim is zero-overhead, autovec-friendly arithmetic: when
// every operand is integer-aligned on the same kind of grid, `a + b` must lower
// to a plain machine add — no branch into the checked/clamp/rational fallback,
// and a loop over it must still vectorize. A refactor (or a stray runtime check)
// can silently break either property while every functional test still passes.
//
// This TU is never run. It is compiled at -O2 and inspected with objdump by
// tests/check_codegen.sh, which asserts:
//   * add_fast contains no `call` — the integer fast path stayed fully inlined,
//     with no fallback to the error handler / rational path leaking in;
//   * add_loop contains a packed-integer add — the fast path still autovectorizes.
//
// Both functions are extern "C" so the symbol names survive un-mangled and the
// disassembly is easy to slice. x86-64 GNU/Clang only (see CMakeLists guard).

#include "bound/bound.hpp"
#include <cstddef>

using namespace bnd;

// A plain integer grid: native integer storage, integer-aligned. `x + y` widens
// the grid type but stays on the integer fast path.
using I = bound<{-1000000, 1000000}>;

// Scalar fast path: must be a bare integer add, no call into any fallback.
extern "C" long bnd_perf_add_fast(long a, long b)
{
  I x = I::from_raw(static_cast<I::raw_type>(a));
  I y = I::from_raw(static_cast<I::raw_type>(b));
  return static_cast<long>((x + y).raw());
}

// Vectorizable fast path: the same add over arrays must emit packed SIMD.
extern "C" void bnd_perf_add_loop(const long* __restrict a,
                                  const long* __restrict b,
                                  long* __restrict out, std::size_t n)
{
  for (std::size_t i = 0; i < n; ++i)
    out[i] = static_cast<long>((I::from_raw(static_cast<I::raw_type>(a[i]))
                              + I::from_raw(static_cast<I::raw_type>(b[i]))).raw());
}
