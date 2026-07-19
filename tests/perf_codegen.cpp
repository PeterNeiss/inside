// Codegen guard for the arithmetic fast paths.
//
// bound's headline claim is zero-overhead, autovec-friendly arithmetic: when
// every operand is integer-aligned on the same kind of grid, `a + b` must lower
// to a plain machine add — no branch into the checked/clamp/rational fallback,
// and a loop over it must still vectorize. A refactor (or a stray runtime check)
// can silently break either property while every functional test still passes.
//
// This TU is never run. It is compiled at -O2 and inspected with objdump by
// tests/check_codegen.sh, which asserts that both functions are call-free — i.e.
// the integer fast path stayed fully inlined to a bare machine add, with no
// fallback to the error handler / rational path leaking in. Whether add_loop
// autovectorizes is reported but not gated (it varies by compiler version).
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

// Multiply fast path: the four-quadrant integer multiply must likewise stay
// call-free (no fallback into the rational branch).
using M = bound<{-1000, 1000}>;

extern "C" long bnd_perf_mul_fast(long a, long b)
{
  M x = M::from_raw(static_cast<M::raw_type>(a));
  M y = M::from_raw(static_cast<M::raw_type>(b));
  return static_cast<long>((x * y).raw());
}

// f64-backed fast arm: add on a dyadic `real` grid must lower to a bare
// double add (one addsd) — the fp arm skips snap_double because the result
// grid is double-exact by construction.
using D = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;

extern "C" double bnd_perf_fp_add(D a, D b)
{
  return (a + b).raw();
}

// Compound subtraction fast path: `-=` on same-notch integer-backed grids
// must subtract raws directly (index-raw Q8.8 here — the case that would
// otherwise fall into the full binary-add + cross-grid-assign engine via
// `+= (-rhs)`).
using Q = bound<{{0, 255}, notch<1, 256>}, snap>;

extern "C" long bnd_perf_sub_compound(long a, long b)
{
  Q x = Q::from_raw(static_cast<Q::raw_type>(a));
  x -= Q::from_raw(static_cast<Q::raw_type>(b));
  return static_cast<long>(x.raw());
}

// Range decode fast path: iterating a grid must stay call-free integer code —
// both the value-storage arm (integer grid) and the index-storage arm
// (fractional grid) of bound_range::iterator::operator*. A call means the
// decode fell back into the rational/assignment engine.
extern "C" long bnd_perf_range_sum()
{
  long sum = 0;
  for (auto whole : bound_range<{0, 999}>{})
    sum += static_cast<long>(whole.raw());
  for (auto fract : bound_range<{{0, 4}, notch<1, 256>}>{})
    sum += static_cast<long>(fract.raw());
  return sum;
}
