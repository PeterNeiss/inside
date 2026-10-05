// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Deterministic perf workload for the cachegrind instruction-count gate
// (tests/beman/inside/check_perf.py). It runs a fixed amount of representative inside
// arithmetic and prints a checksum; check_perf.py runs it under
// `valgrind --tool=cachegrind` and compares the total retired-instruction count
// (Ir) against tests/beman/inside/perf_baseline.json within a tolerance. Instruction counts
// are deterministic regardless of host load, so this is a stable signal even on
// noisy shared CI runners — unlike wall-clock timing.
//
// One workload per baseline key, selected by argv[1] (default
// "integer_qformat", the original combined add workload):
//   integer_qformat   — integer + Q-format same-grid add and raw round-trip
//   integer_mul       — signed integer multiply (value-index path)
//   qformat_div       — native Q-format divide (zero-free divisor grid)
//   cross_grid_assign — integer-mapping cross-grid store
//   checked_add       — add + checked narrowing assignment (runtime range branch)
//   rational_store    — exact fraction stored into a Q-format grid (rounding)
//   rational_compare  — exact fraction ordering (128-bit cross products)
//   math_table        — sin of a 256-slot input (the compile-time table)
//   math_double       — sin onto a 2^-20 grid (the double tier)
//   math_integer      — sin onto a 2^-40 grid (the integer path)
//   math_decimal      — exp of a decimal input onto a 10^-6 grid
//
// The work is intentionally small and scaled by BEMAN_INSIDE_PERF_SCALE so cachegrind
// (~20-50x slowdown) stays well inside the CI time budget. Bump the scale for a
// deeper local run; baselines are captured at scale 1.
//
// Everything funnels into a volatile sink so the optimizer can't elide the work,
// and the input sequence is a fixed integer recurrence (no RNG) so the
// instruction count is byte-for-byte reproducible.

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

#include <cstdint>
#include <cstdio>
#include <cstring>

#ifndef BEMAN_INSIDE_PERF_SCALE
#define BEMAN_INSIDE_PERF_SCALE 1
#endif

using namespace beman::inside;

namespace
{
  using I = inside<{-1000000, 1000000}>;                 // integer fast path
  using Q = inside<{{-1000, 1000}, per<16>}>;       // Q-format (fixed-point) fast path

  volatile std::int64_t g_sink = 0;                     // defeat dead-code elimination

  constexpr long iters = 200000L * BEMAN_INSIDE_PERF_SCALE;      // ~1e6 ops at scale 1

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
    using M = inside<{-1000, 1000}>;                     // product grid fits int32
    std::int64_t acc = 0, x = 1;
    for (long i = 0; i < iters; ++i)
    {
      lcg(x);
      const long a = static_cast<long>((x >> 33) % 2000) - 1000;
      const long b = static_cast<long>((x >> 11) % 2000) - 1000;
      M ma = M::from_raw(static_cast<M::raw_type>(a));
      M mb = M::from_raw(static_cast<M::raw_type>(b));
      acc += static_cast<long>((ma * mb).raw());        // value-index fast path
    }
    return acc;
  }

  std::int64_t run_qformat_div()
  {
    // The native Q-format divide requires Lower == 0 on BOTH grids
    // (is_qformat), so the divisor grid necessarily spans zero and div returns
    // an expected — divisor raws stay >= 16, so it always has a value and the
    // measured work is the native `(a << log2 N) / b` path plus its zero test.
    using Qn = inside<{{0, 1000}, per<16>}>;
    std::int64_t acc = 0, x = 1;
    for (long i = 0; i < iters; ++i)
    {
      lcg(x);
      const long a = static_cast<long>((x >> 33) % 16001);
      const long b = static_cast<long>((x >> 11) % 15984) + 16;   // raw ∈ [16, 15999]
      Qn qa = Qn::from_raw(static_cast<Qn::raw_type>(a));
      Qn qb = Qn::from_raw(static_cast<Qn::raw_type>(b));
      acc += static_cast<long>(div(qa, qb, snapped)->raw());    // native Q divide
    }
    return acc;
  }

  std::int64_t run_cross_grid_assign()
  {
    using narrow = inside<{0, 100}>;
    using wide   = inside<{-500, 9000}>;
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
    using C = inside<{-1000000, 1000000}, checked>;
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

  std::int64_t run_rational_store()
  {
    using R = detail::rational;
    using Q8 = inside<{{0, 255}, per<256>}, round_nearest>;
    std::int64_t acc = 0, x = 1;
    Q8 q = 0;
    for (long i = 0; i < iters; ++i)
    {
      lcg(x);
      const auto ux = static_cast<unsigned long long>(x);
      // a fraction in [0, 255) whose denominator is not a power of two, so
      // the store rounds through the general fraction path
      const R v{static_cast<umax>((ux >> 33) % 76500), static_cast<imax>(300 + (ux >> 60))};
      q = v;
      acc += static_cast<long>(q.raw());
    }
    return acc;
  }

  std::int64_t run_rational_compare()
  {
    using R = detail::rational;
    std::int64_t acc = 0, x = 1;
    for (long i = 0; i < iters; ++i)
    {
      lcg(x);
      const auto ux = static_cast<unsigned long long>(x);
      // large numerators and denominators: the cross products need 128 bits
      const R a{(ux >> 3) | 1u, static_cast<imax>((ux >> 20) | 1u)};
      const R b{(ux >> 5) | 1u, static_cast<imax>((ux >> 22) | 1u)};
      acc += (a < b) ? 1 : 0;
    }
    return acc;
  }

  // Math: a fixed sweep over the input grid, one call per step.
  constexpr long math_iters = 10000L * BEMAN_INSIDE_PERF_SCALE;

  template <typename In, typename F>
  std::int64_t run_math(F f)
  {
    std::int64_t acc = 0, x = 1;
    const auto slots = static_cast<std::uint64_t>(static_cast<std::int64_t>(grid_of<In>.slot_count())) + 1;
    for (long i = 0; i < math_iters; ++i)
    {
      lcg(x);
      const In v = In::from_raw(detail::raw_from_offset<In>((static_cast<std::uint64_t>(x) >> 20) % slots));
      acc += static_cast<std::int64_t>(f(v).raw());
    }
    return acc;
  }

  using angle8 = inside<{{-4, detail::rational{127, 32}}, per<32>}, round_nearest>;   // 256 slots
  using angle  = inside<{{-4, 4}, per<512>}, round_nearest>;
  using dec    = inside<{{-4, 4}, per<1000>}, round_nearest>;
  using out20  = inside<{{-64, 64}, per<(1u << 20)>}, round_nearest>;
  using out40  = inside<{{-64, 64}, per<(std::uint64_t{1} << 40)>}, round_nearest>;
  using out6   = inside<{{0, 64}, per<1000000>}, round_nearest>;

  std::int64_t run_math_table()   { return run_math<angle8>([](angle8 v) { return math::sin_into<out20>(v); }); }
  std::int64_t run_math_double()  { return run_math<angle>([](angle v) { return math::sin_into<out20>(v); }); }
  std::int64_t run_math_integer() { return run_math<angle>([](angle v) { return math::sin_into<out40>(v); }); }
  std::int64_t run_math_decimal() { return run_math<dec>([](dec v) { return math::exp_into<out6>(v); }); }
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
  else if (std::strcmp(key, "rational_store")    == 0) acc = run_rational_store();
  else if (std::strcmp(key, "rational_compare")  == 0) acc = run_rational_compare();
  else if (std::strcmp(key, "math_table")        == 0) acc = run_math_table();
  else if (std::strcmp(key, "math_double")       == 0) acc = run_math_double();
  else if (std::strcmp(key, "math_integer")      == 0) acc = run_math_integer();
  else if (std::strcmp(key, "math_decimal")      == 0) acc = run_math_decimal();
  else { std::fprintf(stderr, "unknown workload key: %s\n", key); return 2; }

  g_sink = acc;
  std::printf("perf_workload key=%s scale=%d sink=%lld\n",
              key, static_cast<int>(BEMAN_INSIDE_PERF_SCALE), static_cast<long long>(g_sink));
  return 0;
}
