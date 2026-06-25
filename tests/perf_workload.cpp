// Deterministic perf workload for the cachegrind instruction-count gate
// (tests/check_perf.py). It runs a fixed amount of representative bound
// arithmetic and prints a checksum; check_perf.py runs it under
// `valgrind --tool=cachegrind` and compares the total retired-instruction count
// (Ir) against tests/perf_baseline.json within a tolerance. Instruction counts
// are deterministic regardless of host load, so this is a stable signal even on
// noisy shared CI runners — unlike wall-clock timing.
//
// The work is intentionally small and scaled by BND_PERF_SCALE so cachegrind
// (~20-50x slowdown) stays well inside the CI time budget. Bump the scale for a
// deeper local run; the baseline is captured at scale 1.
//
// Everything funnels into a volatile sink so the optimizer can't elide the work,
// and the input sequence is a fixed integer recurrence (no RNG) so the
// instruction count is byte-for-byte reproducible.

#include "bound/bound.hpp"

#include <cstdint>
#include <cstdio>

#ifndef BND_PERF_SCALE
#define BND_PERF_SCALE 1
#endif

using namespace bnd;

namespace
{
  using I = bound<{-1000000, 1000000}>;                 // integer fast path
  using Q = bound<{{-1000, 1000}, notch<1, 16>}>;       // Q-format (fixed-point) fast path

  volatile std::int64_t g_sink = 0;                     // defeat dead-code elimination
}

int main()
{
  constexpr long base = 200000;                         // ~1e6 ops at scale 1
  constexpr long iters = base * static_cast<long>(BND_PERF_SCALE);

  std::int64_t acc = 0;
  std::int64_t x = 1;                                   // deterministic recurrence

  for (long i = 0; i < iters; ++i)
  {
    // a cheap LCG step keeps the operands varied but fully deterministic
    x = x * 6364136223846793005LL + 1442695040888963407LL;
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

  g_sink = acc;
  std::printf("perf_workload scale=%d sink=%lld\n",
              static_cast<int>(BND_PERF_SCALE), static_cast<long long>(g_sink));
  return 0;
}
