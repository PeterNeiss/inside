// Deterministic perf workload for the cachegrind instruction-count gate
// (tests/check_perf.py). It runs a fixed amount of representative bound
// arithmetic and prints a checksum; check_perf.py runs it under
// `valgrind --tool=cachegrind` and compares the total retired-instruction count
// (Ir) against tests/perf_baseline.json within a tolerance. Instruction counts
// are deterministic regardless of host load, so this is a stable signal even on
// noisy shared CI runners — unlike wall-clock timing.
//
// One workload per baseline key, selected by argv[1] (default
// "integer_qformat", the original combined add workload):
//   integer_qformat   — integer + Q-format same-grid add and raw round-trip
//   integer_mul       — four-quadrant integer multiply
//   qformat_div       — native Q-format divide (zero-free divisor grid)
//   cross_grid_assign — integer-mapping cross-grid store
//   checked_add       — add + checked narrowing assignment (runtime range branch)
//
// The work is intentionally small and scaled by BND_PERF_SCALE so cachegrind
// (~20-50x slowdown) stays well inside the CI time budget. Bump the scale for a
// deeper local run; baselines are captured at scale 1.
//
// Everything funnels into a volatile sink so the optimizer can't elide the work,
// and the input sequence is a fixed integer recurrence (no RNG) so the
// instruction count is byte-for-byte reproducible.

#include "bound/bound.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

#ifndef BND_PERF_SCALE
#define BND_PERF_SCALE 1
#endif

using namespace bnd;

namespace
{
  using I = bound<{-1000000, 1000000}>;                 // integer fast path
  using Q = bound<{{-1000, 1000}, notch<1, 16>}>;       // Q-format (fixed-point) fast path

  volatile std::int64_t g_sink = 0;                     // defeat dead-code elimination

  constexpr long iters = 200000L * BND_PERF_SCALE;      // ~1e6 ops at scale 1

  // a cheap LCG step keeps the operands varied but fully deterministic
  inline std::int64_t lcg(std::int64_t& x)
  { return x = x * 6364136223846793005LL + 1442695040888963407LL; }

  std::int64_t run_integer_qformat()
  {
    std::int64_t acc = 0, x = 1;
    for (long i = 0; i < iters; ++i)
    {
      lcg(x);
      const long a = static_cast<long>((x >> 33) % 1000000) - 500000;
      const long b = static_cast<long>((x >> 11) % 1000000) - 500000;

      // integer fast paths: add (widening) and the raw round-trip
      I ia = I::from_raw(static_cast<I::raw_type>(a));
      I ib = I::from_raw(static_cast<I::raw_type>(b));
      acc += static_cast<long>((ia + ib).raw());

      // Q-format fast path: same-grid add on a fractional notch
      Q qa = Q::from_raw(static_cast<Q::raw_type>(a % 16001));
      Q qb = Q::from_raw(static_cast<Q::raw_type>(b % 16001));
      acc ^= static_cast<long>((qa + qb).raw());
    }
    return acc;
  }

  std::int64_t run_integer_mul()
  {
    using M = bound<{-1000, 1000}>;                     // product grid fits int32
    std::int64_t acc = 0, x = 1;
    for (long i = 0; i < iters; ++i)
    {
      lcg(x);
      const long a = static_cast<long>((x >> 33) % 2000) - 1000;
      const long b = static_cast<long>((x >> 11) % 2000) - 1000;
      M ma = M::from_raw(static_cast<M::raw_type>(a));
      M mb = M::from_raw(static_cast<M::raw_type>(b));
      acc += static_cast<long>((ma * mb).raw());        // four-quadrant fast path
    }
    return acc;
  }

  std::int64_t run_qformat_div()
  {
    // The native Q-format divide requires Lower == 0 on BOTH grids
    // (IsQFormat), so the divisor grid necessarily spans zero and div returns
    // an optional — divisor raws stay >= 16, so it always has a value and the
    // measured work is the native `(a << log2 N) / b` path plus its zero test.
    using Qn = bound<{{0, 1000}, notch<1, 16>}>;
    std::int64_t acc = 0, x = 1;
    for (long i = 0; i < iters; ++i)
    {
      lcg(x);
      const long a = static_cast<long>((x >> 33) % 16001);
      const long b = static_cast<long>((x >> 11) % 15984) + 16;   // raw ∈ [16, 15999]
      Qn qa = Qn::from_raw(static_cast<Qn::raw_type>(a));
      Qn qb = Qn::from_raw(static_cast<Qn::raw_type>(b));
      acc += static_cast<long>(div(qa, qb, truncated)->raw());    // native Q divide
    }
    return acc;
  }

  std::int64_t run_cross_grid_assign()
  {
    using narrow = bound<{0, 100}>;
    using wide   = bound<{-500, 9000}>;
    std::int64_t acc = 0, x = 1;
    for (long i = 0; i < iters; ++i)
    {
      lcg(x);
      narrow a = narrow::from_raw(static_cast<narrow::raw_type>((x >> 33) % 101));
      wide   b = a;                                      // integer-mapping store
      acc += static_cast<long>(b.raw());
    }
    return acc;
  }

  std::int64_t run_checked_add()
  {
    using C = bound<{-1000000, 1000000}, checked>;
    std::int64_t acc = 0, x = 1;
    for (long i = 0; i < iters; ++i)
    {
      lcg(x);
      // unsigned extraction: operands provably in [-500000, 499999], so the
      // checked narrowing store never fires its error path (the branch is
      // what's being measured, not the throw).
      const auto ux = static_cast<unsigned long long>(x);
      const long a = static_cast<long>((ux >> 33) % 1000000) - 500000;
      const long b = static_cast<long>((ux >> 11) % 1000000) - 500000;
      C ca = C::from_raw(static_cast<C::raw_type>(a));
      C cb = C::from_raw(static_cast<C::raw_type>(b));
      C c  = ca + cb;                                    // checked narrowing store
      acc += static_cast<long>(c.raw());
    }
    return acc;
  }
}

int main(int argc, char** argv)
{
  const char* key = (argc > 1) ? argv[1] : "integer_qformat";

  std::int64_t acc;
  if      (std::strcmp(key, "integer_qformat")   == 0) acc = run_integer_qformat();
  else if (std::strcmp(key, "integer_mul")       == 0) acc = run_integer_mul();
  else if (std::strcmp(key, "qformat_div")       == 0) acc = run_qformat_div();
  else if (std::strcmp(key, "cross_grid_assign") == 0) acc = run_cross_grid_assign();
  else if (std::strcmp(key, "checked_add")       == 0) acc = run_checked_add();
  else { std::fprintf(stderr, "unknown workload key: %s\n", key); return 2; }

  g_sink = acc;
  std::printf("perf_workload key=%s scale=%d sink=%lld\n",
              key, static_cast<int>(BND_PERF_SCALE), static_cast<long long>(g_sink));
  return 0;
}
