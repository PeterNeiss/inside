//---------------------------------------------------------------------------
// bound<> microbenchmarks — ankerl::nanobench.
//
// Every group is one nanobench table with the NATIVE implementation as its
// first row, so the `relative` column reads directly as "bound costs X% of
// native" (100% = parity, >100% = faster than native). Hardware perf
// counters (instructions, cycles, branch misses) appear when the kernel
// grants perf_event access; otherwise the table degrades to wall time.
//
//   ./bench                     # tables to stdout
//   ./bench <file.md>           # write tables to <file.md> (docs/performance.md)
//   ./bench <file.md> <file.json>  # additionally dump raw JSON per group
//
// The `perf_report` CMake target regenerates docs/performance.md with it.
// Unlike its ctrack predecessor this needs no print-and-clear memory
// workaround: nanobench aggregates per epoch (constant memory).
//---------------------------------------------------------------------------
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include "bound/bound.hpp"
#include "bound/cmath.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>

using namespace bnd;
using namespace bnd::detail;
using ankerl::nanobench::Bench;
using ankerl::nanobench::doNotOptimizeAway;

namespace
{
  std::ostream* g_out  = &std::cout;
  std::ostream* g_json = nullptr;

  // One table per operation; the first row is the native baseline for the
  // `relative` column. minIters pins enough work per epoch that sub-ns rows
  // measure stably even on a laptop governor (batched groups pass a small
  // count: their unit of work is already `batch` elements).
  Bench group(char const* title, std::uint64_t minIters = 300'000)
  {
    Bench bench;
    bench.output(g_out).title(title).unit("op").relative(true)
         .performanceCounters(true).minEpochIterations(minIters);
    return bench;
  }

  void finish(Bench& bench)
  {
    if (g_json != nullptr)
      bench.render(ankerl::nanobench::templates::json(), *g_json);
  }

  //-------------------------------------------------------------------------
  // checked_u8: minimal exception-checked bounded integer [0, 200] — the
  // "hand-rolled safety" baseline.
  //-------------------------------------------------------------------------
  struct checked_u8
  {
    std::uint8_t value{};
    checked_u8() = default;
    explicit checked_u8(int v)
    {
      if (v < 0 || v > 200) throw std::out_of_range("checked_u8");
      value = static_cast<std::uint8_t>(v);
    }
    friend checked_u8 operator+(checked_u8 a, checked_u8 b)
    {
      int sum = a.value + b.value;
      if (sum > 200) throw std::out_of_range("checked_u8 +");
      checked_u8 r; r.value = static_cast<std::uint8_t>(sum); return r;
    }
    friend checked_u8 operator*(checked_u8 a, checked_u8 b)
    {
      int prod = a.value * b.value;
      if (prod > 200) throw std::out_of_range("checked_u8 *");
      checked_u8 r; r.value = static_cast<std::uint8_t>(prod); return r;
    }
  };

  using u200  = bound<{0, 200}, unsafe>;
  using u200k = bound<{0, 200'000}, unsafe>;
  using u200k_checked = bound<{0, 200'000}, checked>;
  using s100k = bound<{-100'000, 100'000}, unsafe>;
  using s9k   = bound<{-500, 9000}, unsafe>;
  using u255  = bound<{0, 255}, unsafe>;

  // Cycling input pool — defeats constant folding in the scalar groups (the
  // old bench used literal constants, which let both sides fold).
  constexpr std::size_t kMask = 1023;
  struct scalar_inputs
  {
    std::vector<int> a, b;
    scalar_inputs(int alo, int ahi, int blo, int bhi)
    {
      for (std::size_t i = 0; i <= kMask; ++i)
      {
        a.push_back(alo + static_cast<int>((i * 7 + 3) % static_cast<std::size_t>(ahi - alo + 1)));
        b.push_back(blo + static_cast<int>((i * 13 + 5) % static_cast<std::size_t>(bhi - blo + 1)));
      }
    }
  };
}

//---------------------------------------------------------------------------
// scalar integer ops (u8 [0,200] value range) — native / clamped / checked /
// bound. Operand pairs are chosen so no overflow/throw ever fires.
//---------------------------------------------------------------------------
static void bench_scalar_u8()
{
  static const scalar_inputs in{0, 90, 0, 90};        // sum ≤ 180, in range
  std::size_t i = 0;

  auto bench = group("construct (u8 [0,200])");
  bench.run("native uint8", [&] {
    ++i;
    auto v = static_cast<std::uint8_t>(in.a[i & kMask]);
    doNotOptimizeAway(v);
  });
  bench.run("native clamped", [&] {
    ++i;
    auto v = static_cast<std::uint8_t>(std::clamp(in.a[i & kMask], 0, 200));
    doNotOptimizeAway(v);
  });
  bench.run("checked (exceptions)", [&] {
    ++i;
    checked_u8 v(in.a[i & kMask]);
    doNotOptimizeAway(v.value);
  });
  bench.run("bound<unsafe>", [&] {
    ++i;
    u200 v(in.a[i & kMask]);
    doNotOptimizeAway(v.raw());
  });
  finish(bench);

  auto add = group("add (u8 [0,200])");
  add.run("native uint8", [&] {
    ++i;
    auto c = static_cast<std::uint8_t>(in.a[i & kMask] + in.b[i & kMask]);
    doNotOptimizeAway(c);
  });
  add.run("native clamped", [&] {
    ++i;
    auto c = static_cast<std::uint8_t>(std::clamp(in.a[i & kMask] + in.b[i & kMask], 0, 200));
    doNotOptimizeAway(c);
  });
  add.run("checked (exceptions)", [&] {
    ++i;
    checked_u8 a(in.a[i & kMask]), b(in.b[i & kMask]);
    doNotOptimizeAway((a + b).value);
  });
  add.run("bound<unsafe>", [&] {
    ++i;
    u200 a(in.a[i & kMask]), b(in.b[i & kMask]);
    doNotOptimizeAway((a + b).raw());
  });
  finish(add);

  static const scalar_inputs min{1, 14, 1, 14};       // product ≤ 196
  auto mul = group("mul (u8 [0,200])");
  mul.run("native uint8", [&] {
    ++i;
    auto c = static_cast<std::uint8_t>(min.a[i & kMask] * min.b[i & kMask]);
    doNotOptimizeAway(c);
  });
  mul.run("checked (exceptions)", [&] {
    ++i;
    checked_u8 a(min.a[i & kMask]), b(min.b[i & kMask]);
    doNotOptimizeAway((a * b).value);
  });
  mul.run("bound<unsafe>", [&] {
    ++i;
    u200 a(min.a[i & kMask]), b(min.b[i & kMask]);
    doNotOptimizeAway((a * b).raw());
  });
  finish(mul);

  static const scalar_inputs din{20, 200, 1, 9};
  auto dv = group("div (u8 [0,200])");
  dv.run("native uint8", [&] {
    ++i;
    auto c = static_cast<std::uint8_t>(din.a[i & kMask] / din.b[i & kMask]);
    doNotOptimizeAway(c);
  });
  dv.run("bound a / b (rational result)", [&] {
    ++i;
    u200 a(din.a[i & kMask]), b(din.b[i & kMask]);
    doNotOptimizeAway((a / b)->raw());
  });
  dv.run("bound div(a, b, truncated)", [&] {
    ++i;
    u200 a(din.a[i & kMask]), b(din.b[i & kMask]);
    doNotOptimizeAway(div(a, b, truncated)->raw());
  });
  finish(dv);
}

//---------------------------------------------------------------------------
// compound ops — ++/-- and /= (Tier-1 optimization targets).
//---------------------------------------------------------------------------
static void bench_compound()
{
  auto inc = group("increment (u32 [0,200000])");
  inc.run("native ++", [&] {
    static std::uint32_t v = 0;
    ++v; if (v > 199'999) v = 0;
    doNotOptimizeAway(v);
  });
  inc.run("bound ++", [&] {
    static u200k v{0};
    ++v; if (v.raw() > 199'999) v = 0;
    doNotOptimizeAway(v.raw());
  });
  finish(inc);

  static const scalar_inputs din{20, 200, 2, 9};
  std::size_t i = 0;
  auto dva = group("compound /= (u8 [0,200])");
  dva.run("native /=", [&] {
    ++i;
    auto a = static_cast<std::uint8_t>(din.a[i & kMask]);
    a = static_cast<std::uint8_t>(a / static_cast<std::uint8_t>(din.b[i & kMask]));
    doNotOptimizeAway(a);
  });
  dva.run("bound /=", [&] {
    ++i;
    u200 a(din.a[i & kMask]);
    a /= u200(din.b[i & kMask]);
    doNotOptimizeAway(a.raw());
  });
  finish(dva);
}

//---------------------------------------------------------------------------
// accumulation over 1000 elements (per-element rates via batch()).
//---------------------------------------------------------------------------
static void bench_accumulate()
{
  constexpr std::size_t SZ = 1000;
  std::vector<std::uint32_t> nv(SZ);
  std::vector<u200k> bv(SZ);
  std::vector<u200k_checked> bvc(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    nv[i]  = static_cast<std::uint32_t>(i % 5);
    bv[i]  = static_cast<int>(i % 5);
    bvc[i] = static_cast<int>(i % 5);
  }

  auto bench = group("accumulate 1000 (u32 [0,200000], per element)", 300);
  bench.batch(SZ);
  bench.run("native loop", [&] {
    std::uint32_t sum = 0;
    for (auto v : nv) sum += v;
    doNotOptimizeAway(sum);
  });
  bench.run("bound<unsafe> loop", [&] {
    u200k sum{0};
    for (auto v : bv) sum += v;
    doNotOptimizeAway(sum.raw());
  });
  bench.run("bound<checked> loop", [&] {
    u200k_checked sum{0};
    for (auto v : bvc) sum += v;
    doNotOptimizeAway(sum.raw());
  });
  bench.run("bnd::sum<checked> (bulk check)", [&] {
    doNotOptimizeAway(bnd::sum<u200k_checked>(bvc).raw());
  });
  bench.run("std::accumulate native", [&] {
    doNotOptimizeAway(std::accumulate(nv.begin(), nv.end(), std::uint32_t{0}));
  });
  bench.run("std::accumulate bound", [&] {
    doNotOptimizeAway(std::accumulate(bv.begin(), bv.end(), u200k{0}, std::plus<>{}).raw());
  });
  finish(bench);
}

//---------------------------------------------------------------------------
// signed scalar / accumulate ([-100k, 100k]) — negative values throughout.
//---------------------------------------------------------------------------
static void bench_signed()
{
  static const scalar_inputs in{-500, 500, -500, 500};
  std::size_t i = 0;

  auto add = group("add (signed [-100k,100k])");
  add.run("native int", [&] {
    ++i;
    int c = in.a[i & kMask] + in.b[i & kMask];
    doNotOptimizeAway(c);
  });
  add.run("bound<unsafe>", [&] {
    ++i;
    s100k a(in.a[i & kMask]), b(in.b[i & kMask]);
    doNotOptimizeAway((a + b).raw());
  });
  finish(add);

  static const scalar_inputs min{-14, 14, -14, 14};
  auto mul = group("mul (signed [-100k,100k])");
  mul.run("native int", [&] {
    ++i;
    int c = min.a[i & kMask] * min.b[i & kMask];
    doNotOptimizeAway(c);
  });
  mul.run("bound<unsafe>", [&] {
    ++i;
    s100k a(min.a[i & kMask]), b(min.b[i & kMask]);
    doNotOptimizeAway((a * b).raw());
  });
  finish(mul);

  // mixed integer-grid + quarter-notch-grid add (Tier-3 fast path): the
  // native pairing is the same math on a hand-scaled 1/4 fixed-point lattice.
  using whole    = bound<{0, 100}, unsafe>;
  using quarters = bound<{{0, 1}, notch<1, 4>}, unsafe>;
  auto mixed = group("add (mixed integer + 1/4-notch grids)");
  mixed.run("native int<<2 + q2", [&] {
    ++i;
    std::int32_t a = in.a[i & kMask] << 2, b = static_cast<std::int32_t>(i & 3);
    doNotOptimizeAway(a + b);
  });
  mixed.run("bound", [&] {
    ++i;
    whole a(in.a[i & kMask]);
    quarters b = quarters::from_raw(static_cast<quarters::raw_type>(i & 3));
    doNotOptimizeAway((a + b).raw());
  });
  finish(mixed);

  constexpr std::size_t SZ = 1000;
  std::vector<int> iv(SZ);
  std::vector<s100k> bv(SZ);
  for (std::size_t j = 0; j < SZ; ++j)
  {
    int val = static_cast<int>(j % 11) - 5;
    iv[j] = val;
    bv[j] = val;
  }
  auto acc = group("accumulate 1000 (signed, per element)", 300);
  acc.batch(SZ);
  acc.run("native loop", [&] {
    int sum = 0;
    for (auto v : iv) sum += v;
    doNotOptimizeAway(sum);
  });
  acc.run("bound<unsafe> loop", [&] {
    s100k sum{0};
    for (auto v : bv) sum += v;
    doNotOptimizeAway(sum.raw());
  });
  finish(acc);
}

//---------------------------------------------------------------------------
// fixed point — Q8.8, signed Q1.14, Q16.16, and the checked Q8.8 variant.
//---------------------------------------------------------------------------
static void bench_fixed_point()
{
  using fp   = bound<{{0, 255}, 1.0 / 256}, unsafe>;
  using fp_c = bound<{{0, 255}, 1.0 / 256}, checked>;
  static const scalar_inputs in{0, 90, 0, 90};
  std::size_t i = 0;

  auto ctor = group("Q8.8 construct");
  ctor.run("native int<<8", [&] {
    ++i;
    std::int32_t v = in.a[i & kMask] << 8;
    doNotOptimizeAway(v);
  });
  ctor.run("bound<unsafe>", [&] {
    ++i;
    fp v(in.a[i & kMask]);
    doNotOptimizeAway(v.raw());
  });
  ctor.run("bound<checked>", [&] {
    ++i;
    fp_c v(in.a[i & kMask]);
    doNotOptimizeAway(v.raw());
  });
  finish(ctor);

  auto add = group("Q8.8 add");
  add.run("native", [&] {
    ++i;
    std::int32_t a = in.a[i & kMask] << 8, b = in.b[i & kMask] << 8;
    doNotOptimizeAway(a + b);
  });
  add.run("bound<unsafe>", [&] {
    ++i;
    fp a(in.a[i & kMask]), b(in.b[i & kMask]);
    doNotOptimizeAway((a + b).raw());
  });
  add.run("bound<checked>", [&] {
    ++i;
    fp_c a(in.a[i & kMask]), b(in.b[i & kMask]);
    doNotOptimizeAway((a + b).raw());
  });
  finish(add);

  static const scalar_inputs min{1, 15, 1, 15};
  auto mul = group("Q8.8 mul");
  mul.run("native (a*b)>>8", [&] {
    ++i;
    std::int32_t a = min.a[i & kMask] << 8, b = min.b[i & kMask] << 8;
    doNotOptimizeAway((a * b) >> 8);
  });
  mul.run("bound<unsafe>", [&] {
    ++i;
    fp a(min.a[i & kMask]), b(min.b[i & kMask]);
    doNotOptimizeAway((a * b).raw());
  });
  finish(mul);

  static const scalar_inputs din{16, 200, 1, 8};
  auto dv = group("Q8.8 div (truncated)");
  dv.run("native (a<<8)/b", [&] {
    ++i;
    std::int32_t a = din.a[i & kMask] << 8, b = din.b[i & kMask] << 8;
    doNotOptimizeAway((a << 8) / b);
  });
  dv.run("bound div(a, b, truncated)", [&] {
    ++i;
    fp a(din.a[i & kMask]), b(din.b[i & kMask]);
    doNotOptimizeAway(div(a, b, truncated)->raw());
  });
  finish(dv);

  constexpr std::size_t SZ = 1000;
  std::vector<std::int32_t> nv(SZ);
  std::vector<fp> bv(SZ);
  for (std::size_t j = 0; j < SZ; ++j)
  {
    int val = static_cast<int>(j % 5);
    nv[j] = val << 8;
    bv[j] = val;
  }
  auto acc = group("Q8.8 accumulate 1000 (per element)", 300);
  acc.batch(SZ);
  acc.run("native loop", [&] {
    std::int32_t sum = 0;
    for (auto v : nv) sum += v;
    doNotOptimizeAway(sum);
  });
  acc.run("bound<unsafe> loop", [&] {
    fp sum{0};
    for (auto v : bv) sum += v;
    doNotOptimizeAway(sum.raw());
  });
  finish(acc);

  // signed Q1.14 audio grid
  using q14 = bound<{{-1, 1}, notch<1, 16384>}, unsafe>;
  static_assert(sizeof(q14) == 2);
  auto q14g = group("Q1.14 signed add");
  q14g.run("native int", [&] {
    ++i;
    std::int32_t a = 5000, b = -3000 + static_cast<int>(i & 63);
    doNotOptimizeAway(a + b);
  });
  q14g.run("bound (raw-level)", [&] {
    auto a = q14::from_raw(20000);
    ++i;
    auto b = q14::from_raw(static_cast<std::uint16_t>(10000 + (i & 63)));
    doNotOptimizeAway((a + b).raw());
  });
  finish(q14g);

  // Q16.16
  using q16 = bound<{{0, 65535}, notch<1, 65536>}, unsafe>;
  static_assert(sizeof(q16) == 4);
  auto q16g = group("Q16.16 add");
  q16g.run("native int64", [&] {
    ++i;
    std::int64_t a = static_cast<std::int64_t>(in.a[i & kMask]) << 16;
    std::int64_t b = static_cast<std::int64_t>(in.b[i & kMask]) << 16;
    doNotOptimizeAway(a + b);
  });
  q16g.run("bound<unsafe>", [&] {
    ++i;
    q16 a(in.a[i & kMask]), b(in.b[i & kMask]);
    doNotOptimizeAway((a + b).raw());
  });
  finish(q16g);
}

//---------------------------------------------------------------------------
// stores & conversions — double/fraction sources, rounding, cross-grid,
// casts, comparison (Tier-2 measurement lives here).
//---------------------------------------------------------------------------
static void bench_store_convert()
{
  std::size_t i = 0;

  using celsius_trunc = bound<{{-40, 60}, 0.5}, snap>;
  using celsius_round = bound<{{-40, 60}, 0.5}, round_nearest>;
  auto assign = group("assign from double ([-40,60] step 0.5)");
  assign.run("truncate (snap)", [&] {
    ++i;
    double val = -40.0 + static_cast<double>(i % 200) * 0.5 + 0.3;
    celsius_trunc c = val;
    doNotOptimizeAway(c.raw());
  });
  assign.run("round_nearest", [&] {
    ++i;
    double val = -40.0 + static_cast<double>(i % 200) * 0.5 + 0.3;
    celsius_round c = val;
    doNotOptimizeAway(c.raw());
  });
  finish(assign);

  using fp = bound<{{0, 255}, 1.0 / 256}, round_nearest>;
  auto store = group("Q8.8 store");
  store.run("from fraction (rational)", [&] {
    ++i;
    rational q{static_cast<imax>(i & 0xFFF), 256};
    fp v{q};
    doNotOptimizeAway(v.raw());
  });
  store.run("from double", [&] {
    ++i;
    double d = static_cast<double>(i & 0xFFF) / 256.0;
    fp v{d};
    doNotOptimizeAway(v.raw());
  });
  finish(store);

  // cross-grid assignment
  using narrow  = bound<{0, 100}, unsafe>;
  using tenths  = bound<{{0, 100}, notch<1, 10>}, unsafe>;
  using quarters= bound<{{0, 100}, notch<1, 4>}, round_nearest>;
  auto cga = group("cross-grid assign");
  cga.run("same-grid copy", [&] {
    ++i;
    s9k a(static_cast<int>(i % 9000) - 500);
    s9k b = a;
    doNotOptimizeAway(b.raw());
  });
  cga.run("integer mapping ([0,100] -> [-500,9000])", [&] {
    ++i;
    narrow a(static_cast<int>(i % 101));
    s9k b = a;
    doNotOptimizeAway(b.raw());
  });
  cga.run("non-integer snap (1/10 -> 1/4 grid)", [&] {
    ++i;
    tenths a = rational{static_cast<imax>(i % 1000), 10};
    quarters b = a.with_snap();
    doNotOptimizeAway(b.raw());
  });
  finish(cga);

  // comparison
  using q14a = bound<{{-8, 8},  notch<1, 16384>}, unsafe>;
  using q14b = bound<{{-4, 12}, notch<1, 16384>}, unsafe>;
  std::vector<std::int16_t> ni(kMask + 1);
  std::vector<s9k> bi(kMask + 1);
  std::vector<q14a> ia(kMask + 1);
  std::vector<q14b> ib(kMask + 1);
  for (std::size_t j = 0; j <= kMask; ++j)
  {
    auto v = static_cast<std::int16_t>((j * 7 + 13) % 9501 - 500);
    ni[j] = v;
    bi[j] = static_cast<int>(v);
    ia[j] = rational{static_cast<imax>(j % 200) - 100, 16};
    ib[j] = rational{static_cast<imax>((j * 3) % 200) - 60, 16};
  }
  auto cmp = group("comparison <");
  cmp.run("native int16", [&] {
    ++i;
    bool r = ni[i & kMask] < ni[(i + 7) & kMask];
    doNotOptimizeAway(r);
  });
  cmp.run("bound same grid", [&] {
    ++i;
    bool r = bi[i & kMask] < bi[(i + 7) & kMask];
    doNotOptimizeAway(r);
  });
  cmp.run("bound index raw, same notch, cross interval", [&] {
    ++i;
    bool r = ia[i & kMask] < ib[i & kMask];
    doNotOptimizeAway(r);
  });
  finish(cmp);

  // casts
  auto casts = group("casts (s9k value into u200)");
  casts.run("native clamp", [&] {
    ++i;
    int v = static_cast<int>(i % 9000) - 500;
    auto c = static_cast<std::uint8_t>(std::clamp(v, 0, 200));
    doNotOptimizeAway(c);
  });
  casts.run("clamp_cast", [&] {
    ++i;
    s9k v(static_cast<int>(i % 9000) - 500);
    doNotOptimizeAway(clamp_cast<u200>(v).raw());
  });
  casts.run("checked construction (in range)", [&] {
    ++i;
    using u200c = bound<{0, 200}, checked>;
    s9k v(static_cast<int>(i % 200));
    doNotOptimizeAway(u200c{v}.raw());
  });
  finish(casts);

  // dot
  static const scalar_inputs din{-90, 90, -90, 90};
  auto dg = group("dot (2D, signed)");
  dg.run("native int", [&] {
    ++i;
    int ax = din.a[i & kMask], ay = din.b[i & kMask];
    int bx = din.b[(i + 3) & kMask], by = din.a[(i + 5) & kMask];
    doNotOptimizeAway(ax * bx + ay * by);
  });
  dg.run("bnd::dot", [&] {
    using c100 = bound<{-100, 100}, unsafe>;
    ++i;
    c100 ax(din.a[i & kMask]), ay(din.b[i & kMask]);
    c100 bx(din.b[(i + 3) & kMask]), by(din.a[(i + 5) & kMask]);
    doNotOptimizeAway(bnd::dot(ax, ay, bx, by).raw());
  });
  finish(dg);
}

//---------------------------------------------------------------------------
// cmath — bound math engines vs std double, per function. Inputs pre-built
// (grids match tests/test_cmath.cpp).
//---------------------------------------------------------------------------
static void bench_cmath()
{
  using algeb_t   = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  using sqrt_in_t = bound<{{0, 4}, notch<1, 65536>}, round_nearest | real>;
  using exp2_in_t = bound<{{-4, 4}, notch<1, 16384>}, round_nearest | real>;
  using log2_in_t = bound<{{0x1p-8_r, 256}, notch<1, 16384>}, round_nearest | real>;
  using exp_in_t  = bound<{{-10, 10}, notch<1, 16384>}, round_nearest | real>;
  using log_in_t  = bound<{{0x1p-8_r, 256}, notch<1, 256>}, round_nearest | real>;
  using pow_in_t  = bound<{{-9, 9}, notch<1, 16384>}, round_nearest | real>;
  using angle_t   = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  using angle_f32_t = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | f32>;
  using tan_in_t  = bound<{{-0.75_r, 0.75_r}, notch<1, 16384>}, round_nearest | real>;
  using atan2_in_t= bound<{{-1, 1}, notch<1, 16384>}, round_nearest | real>;
  using fmod_x_t  = bound<{{-8, 8}, notch<1, 16384>}, round_nearest>;
  using fmod_y_t  = bound<{{0.25_r, 4}, notch<1, 16384>}, round_nearest>;

  constexpr std::size_t M = 4096;
  std::vector<algeb_t>    v_alg, v_alg2; std::vector<sqrt_in_t> v_sqrt;
  std::vector<exp2_in_t>  v_exp2;  std::vector<log2_in_t>  v_log2;
  std::vector<exp_in_t>   v_exp;   std::vector<log_in_t>   v_log;
  std::vector<pow_in_t>   v_pow;   std::vector<angle_t>    v_ang;
  std::vector<angle_f32_t> v_angf;
  std::vector<tan_in_t>   v_tan;   std::vector<atan2_in_t> v_aty, v_atx;
  std::vector<fmod_x_t>   v_fmx;   std::vector<fmod_y_t>   v_fmy;
  std::vector<double> d_qs, d_q, d_log2, d_log, d_tan, d_aty, d_atx, d_fmy;
  for (std::size_t j = 0; j < M; ++j)
  {
    imax k = static_cast<imax>((j * 16) & 0xFFFF);
    rational q  = rational{k, 16384};
    rational qs = rational{k - 32768, 16384};
    rational rl2{(k % 65535) + 1, 16384};
    rational rl {(k % 65535) + 1, 256};
    rational rt {(k % 24576) - 12288, 16384};
    rational ry {(k % 32768) - 16384, 16384};
    rational rx {((k + 1) % 32768) - 16384, 16384};
    rational rfy{(k % 60) + 4, 16384};
    v_alg.push_back(algeb_t{qs});    v_alg2.push_back(algeb_t{q});
    v_sqrt.push_back(sqrt_in_t{q});
    v_exp2.push_back(exp2_in_t{qs}); v_log2.push_back(log2_in_t{rl2});
    v_exp.push_back(exp_in_t{qs});   v_log.push_back(log_in_t{rl});
    v_pow.push_back(pow_in_t{qs});   v_ang.push_back(angle_t{qs});
    v_angf.push_back(angle_f32_t{qs});
    v_tan.push_back(tan_in_t{rt});   v_aty.push_back(atan2_in_t{ry});
    v_atx.push_back(atan2_in_t{rx}); v_fmx.push_back(fmod_x_t{qs});
    v_fmy.push_back(fmod_y_t{rfy});
    d_qs.push_back(static_cast<double>(qs));   d_q.push_back(static_cast<double>(q));
    d_log2.push_back(static_cast<double>(rl2)); d_log.push_back(static_cast<double>(rl));
    d_tan.push_back(static_cast<double>(rt));   d_aty.push_back(static_cast<double>(ry));
    d_atx.push_back(static_cast<double>(rx));   d_fmy.push_back(static_cast<double>(rfy));
  }

  std::size_t i = 0;
  constexpr std::size_t J = M - 1;

  // std double first (native baseline), bound second.
#define BND_CMATH_GROUP(title, std_expr, bnd_expr)                       \
  {                                                                      \
    auto bench = group(title, 50'000);                               \
    bench.run("std double", [&] { ++i; doNotOptimizeAway(std_expr); });  \
    bench.run("bnd::math bound", [&] { ++i; doNotOptimizeAway(bnd_expr); }); \
    finish(bench);                                                       \
  }

  BND_CMATH_GROUP("math: abs",   std::abs(d_qs[i & J]),   bnd::math::abs(v_alg[i & J]).raw())
  BND_CMATH_GROUP("math: floor", std::floor(d_qs[i & J]), bnd::math::floor(v_alg[i & J]).raw())
  BND_CMATH_GROUP("math: round", std::round(d_qs[i & J]), bnd::math::round(v_alg[i & J]).raw())
  BND_CMATH_GROUP("math: sqrt",  std::sqrt(d_q[i & J]),   bnd::math::sqrt(v_sqrt[i & J]).raw())
  BND_CMATH_GROUP("math: exp2",  std::exp2(d_qs[i & J]),  bnd::math::exp2(v_exp2[i & J]).raw())
  BND_CMATH_GROUP("math: log2",  std::log2(d_log2[i & J]),bnd::math::log2(v_log2[i & J]).raw())
  BND_CMATH_GROUP("math: exp",   std::exp(d_qs[i & J]),   bnd::math::exp(v_exp[i & J]).raw())
  BND_CMATH_GROUP("math: log",   std::log(d_log[i & J]),  bnd::math::log(v_log[i & J]).raw())
  BND_CMATH_GROUP("math: pow10", std::pow(10.0, d_qs[i & J]), bnd::math::pow_base<10>(v_pow[i & J]).raw())
  BND_CMATH_GROUP("math: sin",   std::sin(d_qs[i & J]),   bnd::math::sin(v_ang[i & J]).raw())
  BND_CMATH_GROUP("math: cos",   std::cos(d_qs[i & J]),   bnd::math::cos(v_ang[i & J]).raw())
  BND_CMATH_GROUP("math: tan",   std::tan(d_tan[i & J]),  bnd::math::tan(v_tan[i & J]))
  BND_CMATH_GROUP("math: atan2", std::atan2(d_aty[i & J], d_atx[i & J]),
                                 bnd::math::atan2(v_aty[i & J], v_atx[i & J]).raw())
  BND_CMATH_GROUP("math: fmod",  std::fmod(d_qs[i & J], d_fmy[i & J]),
                                 bnd::math::fmod(v_fmx[i & J], v_fmy[i & J]).raw())
  BND_CMATH_GROUP("math: asin",  std::asin(d_aty[i & J]), bnd::math::asin(v_aty[i & J]).raw())
  BND_CMATH_GROUP("math: tanh",  std::tanh(d_qs[i & J]),  bnd::math::tanh(v_exp[i & J]).raw())
  BND_CMATH_GROUP("math: cbrt",  std::cbrt(d_qs[i & J]),  bnd::math::cbrt(v_alg[i & J]).raw())
  BND_CMATH_GROUP("math: hypot", std::hypot(d_qs[i & J], d_q[i & J]),
                                 bnd::math::hypot(v_alg[i & J], v_alg2[i & J]).raw())
#undef BND_CMATH_GROUP

#ifndef BND_MATH_FIXED
  {
    auto bench = group("math: sin (binary32 / float engine)", 50'000);
    bench.run("std sinf", [&] {
      ++i;
      doNotOptimizeAway(std::sin(static_cast<float>(d_qs[i & J])));
    });
    bench.run("bnd::math::flt::sin (f32 bound)", [&] {
      ++i;
      doNotOptimizeAway(bnd::math::flt::sin(v_angf[i & J]).raw());
    });
    finish(bench);
  }
#endif
}

//---------------------------------------------------------------------------
// STL algorithms over 10k elements (per-element rates via batch()).
//---------------------------------------------------------------------------
static void bench_algorithms()
{
  namespace rng = std::ranges;
  constexpr std::size_t SZ = 10'000;

  std::vector<std::int16_t> nv(SZ);
  std::vector<s9k> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    auto val = static_cast<std::int16_t>((i * 7 + 13) % 9501 - 500);
    nv[i] = val;
    bv[i] = static_cast<int>(val);
  }
  auto nv_copy = nv;
  auto bv_copy = bv;

  auto sort = group("sort 10k (per element)", 30);
  sort.batch(SZ);
  sort.run("native int16", [&] {
    nv = nv_copy;
    rng::sort(nv);
    doNotOptimizeAway(nv[0]);
  });
  sort.run("bound", [&] {
    bv = bv_copy;
    rng::sort(bv);
    doNotOptimizeAway(bv[0].raw());
  });
  finish(sort);

  auto nth = group("nth_element 10k (per element)", 30);
  nth.batch(SZ);
  nth.run("native int16", [&] {
    nv = nv_copy;
    std::nth_element(nv.begin(), nv.begin() + SZ / 2, nv.end());
    doNotOptimizeAway(nv[SZ / 2]);
  });
  nth.run("bound", [&] {
    bv = bv_copy;
    std::nth_element(bv.begin(), bv.begin() + SZ / 2, bv.end());
    doNotOptimizeAway(bv[SZ / 2].raw());
  });
  finish(nth);

  auto part = group("partition 10k (per element)", 30);
  part.batch(SZ);
  part.run("native int16", [&] {
    nv = nv_copy;
    auto p = std::partition(nv.begin(), nv.end(), [](std::int16_t v) { return v >= 0; });
    doNotOptimizeAway(*p);
  });
  part.run("bound", [&] {
    bv = bv_copy;
    auto p = std::partition(bv.begin(), bv.end(), [](s9k v) { return v >= 0; });
    doNotOptimizeAway(p->raw());
  });
  finish(part);

  // sorted copies for the search benches
  std::vector<std::int16_t> ns = nv_copy;
  std::vector<s9k> bs = bv_copy;
  rng::sort(ns);
  rng::sort(bs);
  std::size_t i = 0;

  auto find = group("find / count 10k (per element)", 30);
  find.batch(SZ);
  find.run("find native", [&] {
    doNotOptimizeAway(*rng::find(nv_copy, static_cast<std::int16_t>(7000)));
  });
  find.run("find bound", [&] {
    doNotOptimizeAway(rng::find(bv_copy, s9k{7000})->raw());
  });
  find.run("count native", [&] {
    doNotOptimizeAway(rng::count(nv_copy, static_cast<std::int16_t>(42)));
  });
  find.run("count bound", [&] {
    doNotOptimizeAway(rng::count(bv_copy, s9k{42}));
  });
  finish(find);

  auto mm = group("min/max_element 10k (per element)", 30);
  mm.batch(SZ);
  mm.run("min_element native", [&] { doNotOptimizeAway(*rng::min_element(nv_copy)); });
  mm.run("min_element bound",  [&] { doNotOptimizeAway(rng::min_element(bv_copy)->raw()); });
  mm.run("max_element native", [&] { doNotOptimizeAway(*rng::max_element(nv_copy)); });
  mm.run("max_element bound",  [&] { doNotOptimizeAway(rng::max_element(bv_copy)->raw()); });
  finish(mm);

  auto lb = group("lower_bound 10k");
  lb.run("native int16", [&] {
    ++i;
    auto target = static_cast<std::int16_t>(static_cast<int>(i % 9501) - 500);
    doNotOptimizeAway(*rng::lower_bound(ns, target));
  });
  lb.run("bound", [&] {
    ++i;
    auto target = s9k{static_cast<int>(i % 9501) - 500};
    doNotOptimizeAway(rng::lower_bound(bs, target)->raw());
  });
  finish(lb);

  // transform with the uint8-width type (255 is the sentinel slot -> u255 is
  // 16-bit; u254 fits uint8 and compares lane-for-lane with native).
  using u254 = bound<{0, 254}, unsafe>;
  static_assert(sizeof(u254) == 1);
  std::vector<std::uint8_t> tn(SZ);
  std::vector<u255> tb(SZ);
  std::vector<u254> t8(SZ);
  std::vector<std::uint8_t> tno(SZ);
  std::vector<u255> tbo(SZ);
  std::vector<u254> t8o(SZ);
  for (std::size_t j = 0; j < SZ; ++j)
  {
    tn[j] = static_cast<std::uint8_t>(j % 250);
    tb[j] = static_cast<int>(j % 250);
    t8[j] = static_cast<int>(j % 250);
  }
  auto tf = group("transform v+1 10k (per element)", 30);
  tf.batch(SZ);
  tf.run("native uint8", [&] {
    std::transform(tn.begin(), tn.end(), tno.begin(),
      [](std::uint8_t v) -> std::uint8_t { return static_cast<std::uint8_t>(v + 1); });
    doNotOptimizeAway(tno[0]);
  });
  tf.run("bound u255 (16-bit storage)", [&] {
    std::transform(tb.begin(), tb.end(), tbo.begin(),
      [](u255 v) { v += 1_b; return v; });
    doNotOptimizeAway(tbo[0].raw());
  });
  tf.run("bound u254 (8-bit storage)", [&] {
    std::transform(t8.begin(), t8.end(), t8o.begin(),
      [](u254 v) { v += 1_b; return v; });
    doNotOptimizeAway(t8o[0].raw());
  });
  finish(tf);
}

//---------------------------------------------------------------------------
// main
//---------------------------------------------------------------------------
int main(int argc, char** argv)
{
  std::ofstream md_file;
  std::ofstream json_file;
  if (argc > 1)
  {
    md_file.open(argv[1]);
    if (!md_file) { std::cerr << "cannot open " << argv[1] << '\n'; return 1; }
    g_out = &md_file;
  }
  if (argc > 2)
  {
    json_file.open(argv[2]);
    if (!json_file) { std::cerr << "cannot open " << argv[2] << '\n'; return 1; }
    g_json = &json_file;
  }

  if (argc > 1)
    *g_out <<
      "# Performance\n\n"
      "Generated by `bench` (tests/bench.cpp, ankerl::nanobench) — do not edit\n"
      "by hand. Regenerate with `cmake --build <build-dir> --target perf_report`\n"
      "(Release, `-O3`, no `-march`; single run on the maintainer's machine).\n\n"
      "Each table's FIRST row is the native implementation; the `relative`\n"
      "column reads as native-time / row-time, so 100% is native parity and\n"
      "smaller percentages are slower. Absolute ns/op values are host-specific\n"
      "— the ratios are the stable signal. Instruction/cycle/branch columns\n"
      "appear when the kernel grants perf-counter access.\n\n"
      "Known slow paths (measured in the tables below, by design):\n"
      "arithmetic with a rational-raw operand takes the exact rational path\n"
      "(the mixed integer/notch-offset add and cross-grid non-integer stores\n"
      "now have folded integer fast paths, gated on imax-safe spans);\n"
      "`bound<checked>` element-wise loops keep a per-element range check —\n"
      "prefer `bnd::sum`'s bulk check, which validates the total instead.\n\n";

  bench_scalar_u8();
  bench_compound();
  bench_accumulate();
  bench_signed();
  bench_fixed_point();
  bench_store_convert();
  bench_cmath();
  bench_algorithms();
  return 0;
}
