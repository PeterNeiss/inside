// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// inside<> microbenchmarks — ankerl::nanobench.
//
// Every group is one nanobench table with the NATIVE implementation as its
// first row, so the `relative` column reads directly as "inside costs X% of
// native" (100% = parity, >100% = faster than native). Hardware perf
// counters (instructions, cycles, branch misses) appear when the kernel
// grants perf_event access; otherwise the table degrades to wall time.
//
//   ./bench                     # tables to stdout
//   ./bench <file.md>           # write tables to <file.md> (docs/performance.md)
//   ./bench <file.md> <file.json>  # additionally dump raw JSON per group
//
// The `beman.inside.perf_report` CMake target (BEMAN_INSIDE_BUILD_TOOLS=ON) regenerates docs/performance.md with it.
// Unlike its ctrack predecessor this needs no print-and-clear memory
// workaround: nanobench aggregates per epoch (constant memory).
//---------------------------------------------------------------------------
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>

using namespace beman::inside;
using namespace beman::inside::detail;
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

  using u200  = inside<{0, 200}, unsafe>;
  using u200k = inside<{0, 200'000}, unsafe>;
  using u200k_checked = inside<{0, 200'000}, checked>;
  using s100k = inside<{-100'000, 100'000}, unsafe>;
  using s9k   = inside<{-500, 9000}, unsafe>;
  using u255  = inside<{0, 255}, unsafe>;

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
// inside. Operand pairs are chosen so no overflow/throw ever fires.
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
  bench.run("inside<unsafe>", [&] {
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
  add.run("inside<unsafe>", [&] {
    ++i;
    u200 a(in.a[i & kMask]), b(in.b[i & kMask]);
    doNotOptimizeAway((a + b).raw());
  });
  finish(add);

  static const scalar_inputs sub_in{100, 190, 0, 90}; // difference in [10, 190]
  auto sub = group("sub (u8 [0,200])");
  sub.run("native uint8", [&] {
    ++i;
    auto c = static_cast<std::uint8_t>(sub_in.a[i & kMask] - sub_in.b[i & kMask]);
    doNotOptimizeAway(c);
  });
  sub.run("inside<unsafe>", [&] {
    ++i;
    u200 a(sub_in.a[i & kMask]), b(sub_in.b[i & kMask]);
    doNotOptimizeAway((a - b).raw());
  });
  finish(sub);

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
  mul.run("inside<unsafe>", [&] {
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
  dv.run("inside a / b (rational result)", [&] {
    ++i;
    u200 a(din.a[i & kMask]), b(din.b[i & kMask]);
    doNotOptimizeAway((a / b)->raw());
  });
  dv.run("inside div(a, b, snapped)", [&] {
    ++i;
    u200 a(din.a[i & kMask]), b(din.b[i & kMask]);
    doNotOptimizeAway(div(a, b, snapped)->raw());
  });
  finish(dv);

  // divisor grid [1,9] excludes zero -> `%` returns a plain inside; a [0,200]
  // divisor keeps the zero check and the expected vocabulary.
  using d9 = inside<{1, 9}, unsafe>;
  auto md = group("mod (u8 [20,200] % [1,9])");
  md.run("native %", [&] {
    ++i;
    auto c = static_cast<std::uint8_t>(din.a[i & kMask] % din.b[i & kMask]);
    doNotOptimizeAway(c);
  });
  md.run("inside % (divisor excludes 0)", [&] {
    ++i;
    u200 a(din.a[i & kMask]);
    d9 b(din.b[i & kMask]);
    doNotOptimizeAway((a % b).raw());
  });
  md.run("inside mod(a, b, snapped) (zero-checked)", [&] {
    ++i;
    u200 a(din.a[i & kMask]), b(din.b[i & kMask]);
    doNotOptimizeAway(mod(a, b, snapped)->raw());
  });
  finish(md);
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
  inc.run("inside ++", [&] {
    static u200k v{0};
    ++v; if (v.raw() > 199'999) v = 0;
    doNotOptimizeAway(v.raw());
  });
  finish(inc);

  static const scalar_inputs sub_in{100, 190, 0, 90}; // difference in [10, 190]
  std::size_t i = 0;

  auto sba = group("compound -= (u8 [0,200])");
  sba.run("native -=", [&] {
    ++i;
    auto a = static_cast<std::uint8_t>(sub_in.a[i & kMask]);
    a = static_cast<std::uint8_t>(a - static_cast<std::uint8_t>(sub_in.b[i & kMask]));
    doNotOptimizeAway(a);
  });
  sba.run("inside -=", [&] {
    ++i;
    u200 a(sub_in.a[i & kMask]);
    a -= u200(sub_in.b[i & kMask]);
    doNotOptimizeAway(a.raw());
  });
  finish(sba);

  // Q8.8 is index-backed: negating the rhs shifts its Lower, so `-= (+= -rhs)`
  // cannot ride the raw-level += fast path — the row this table pins.
  using fp88 = inside<{{0, 255}, 1.0 / 256}, unsafe>;
  auto sbq = group("compound -= (Q8.8)");
  sbq.run("native int32 -=", [&] {
    ++i;
    std::int32_t a = sub_in.a[i & kMask] << 8;
    a -= sub_in.b[i & kMask] << 8;
    doNotOptimizeAway(a);
  });
  sbq.run("inside -=", [&] {
    ++i;
    fp88 a(sub_in.a[i & kMask]);
    a -= fp88(sub_in.b[i & kMask]);
    doNotOptimizeAway(a.raw());
  });
  finish(sbq);

  static const scalar_inputs min_in{1, 14, 1, 14};    // product ≤ 196
  auto mla = group("compound *= (u8 [0,200])");
  mla.run("native *=", [&] {
    ++i;
    auto a = static_cast<std::uint8_t>(min_in.a[i & kMask]);
    a = static_cast<std::uint8_t>(a * static_cast<std::uint8_t>(min_in.b[i & kMask]));
    doNotOptimizeAway(a);
  });
  mla.run("inside *=", [&] {
    ++i;
    u200 a(min_in.a[i & kMask]);
    a *= u200(min_in.b[i & kMask]);
    doNotOptimizeAway(a.raw());
  });
  finish(mla);

  // rational rhs takes the exact rational branch by design (see the
  // known-slow-paths note); this row documents its cost.
  static const scalar_inputs qin{0, 90, 0, 90};
  auto rga = group("compound += rational (Q8.8)");
  rga.run("native int32 += k", [&] {
    ++i;
    std::int32_t a = qin.a[i & kMask] << 8;
    a += static_cast<std::int32_t>(i & 63);
    doNotOptimizeAway(a);
  });
  rga.run("inside += rational{k, 256}", [&] {
    ++i;
    fp88 a(qin.a[i & kMask]);
    a += rational{static_cast<imax>(i & 63), 256};
    doNotOptimizeAway(a.raw());
  });
  finish(rga);

  static const scalar_inputs din{20, 200, 2, 9};
  auto dva = group("compound /= (u8 [0,200])");
  dva.run("native /=", [&] {
    ++i;
    auto a = static_cast<std::uint8_t>(din.a[i & kMask]);
    a = static_cast<std::uint8_t>(a / static_cast<std::uint8_t>(din.b[i & kMask]));
    doNotOptimizeAway(a);
  });
  dva.run("inside /=", [&] {
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
  bench.run("inside<unsafe> loop", [&] {
    u200k sum{0};
    for (auto v : bv) sum += v;
    doNotOptimizeAway(sum.raw());
  });
  bench.run("inside<checked> loop", [&] {
    u200k_checked sum{0};
    for (auto v : bvc) sum += v;
    doNotOptimizeAway(sum.raw());
  });
  bench.run("beman::inside::sum<checked> (bulk check)", [&] {
    doNotOptimizeAway(beman::inside::sum<u200k_checked>(bvc).raw());
  });
  bench.run("std::accumulate native", [&] {
    doNotOptimizeAway(std::accumulate(nv.begin(), nv.end(), std::uint32_t{0}));
  });
  bench.run("std::accumulate inside", [&] {
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
  add.run("inside<unsafe>", [&] {
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
  mul.run("inside<unsafe>", [&] {
    ++i;
    s100k a(min.a[i & kMask]), b(min.b[i & kMask]);
    doNotOptimizeAway((a * b).raw());
  });
  finish(mul);

  auto sb = group("sub (signed [-100k,100k])");
  sb.run("native int", [&] {
    ++i;
    int c = in.a[i & kMask] - in.b[i & kMask];
    doNotOptimizeAway(c);
  });
  sb.run("inside<unsafe>", [&] {
    ++i;
    s100k a(in.a[i & kMask]), b(in.b[i & kMask]);
    doNotOptimizeAway((a - b).raw());
  });
  finish(sb);

  auto ng = group("negate (signed [-100k,100k])");
  ng.run("native int", [&] {
    ++i;
    int c = -in.a[i & kMask];
    doNotOptimizeAway(c);
  });
  ng.run("inside unary -", [&] {
    ++i;
    s100k a(in.a[i & kMask]);
    doNotOptimizeAway((-a).raw());
  });
  finish(ng);

  // mixed integer-grid + quarter-notch-grid add (Tier-3 fast path): the
  // native pairing is the same math on a hand-scaled 1/4 fixed-point lattice.
  using whole    = inside<{0, 100}, unsafe>;
  using quarters = inside<{{0, 1}, notch<1, 4>}, unsafe>;
  auto mixed = group("add (mixed integer + 1/4-notch grids)");
  mixed.run("native int<<2 + q2", [&] {
    ++i;
    std::int32_t a = in.a[i & kMask] << 2, b = static_cast<std::int32_t>(i & 3);
    doNotOptimizeAway(a + b);
  });
  mixed.run("inside", [&] {
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
  acc.run("inside<unsafe> loop", [&] {
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
  using fp   = inside<{{0, 255}, 1.0 / 256}, unsafe>;
  using fp_c = inside<{{0, 255}, 1.0 / 256}, checked>;
  static const scalar_inputs in{0, 90, 0, 90};
  std::size_t i = 0;

  auto ctor = group("Q8.8 construct");
  ctor.run("native int<<8", [&] {
    ++i;
    std::int32_t v = in.a[i & kMask] << 8;
    doNotOptimizeAway(v);
  });
  ctor.run("inside<unsafe>", [&] {
    ++i;
    fp v(in.a[i & kMask]);
    doNotOptimizeAway(v.raw());
  });
  ctor.run("inside<checked>", [&] {
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
  add.run("inside<unsafe>", [&] {
    ++i;
    fp a(in.a[i & kMask]), b(in.b[i & kMask]);
    doNotOptimizeAway((a + b).raw());
  });
  add.run("inside<checked>", [&] {
    ++i;
    fp_c a(in.a[i & kMask]), b(in.b[i & kMask]);
    doNotOptimizeAway((a + b).raw());
  });
  finish(add);

  static const scalar_inputs sub_in{100, 190, 0, 90};
  auto subq = group("Q8.8 sub");
  subq.run("native", [&] {
    ++i;
    std::int32_t a = sub_in.a[i & kMask] << 8, b = sub_in.b[i & kMask] << 8;
    doNotOptimizeAway(a - b);
  });
  subq.run("inside<unsafe>", [&] {
    ++i;
    fp a(sub_in.a[i & kMask]), b(sub_in.b[i & kMask]);
    doNotOptimizeAway((a - b).raw());
  });
  finish(subq);

  static const scalar_inputs min{1, 15, 1, 15};
  auto mul = group("Q8.8 mul");
  mul.run("native (a*b)>>8", [&] {
    ++i;
    std::int32_t a = min.a[i & kMask] << 8, b = min.b[i & kMask] << 8;
    doNotOptimizeAway((a * b) >> 8);
  });
  mul.run("inside<unsafe>", [&] {
    ++i;
    fp a(min.a[i & kMask]), b(min.b[i & kMask]);
    doNotOptimizeAway((a * b).raw());
  });
  finish(mul);

  static const scalar_inputs din{16, 200, 1, 8};
  auto dv = group("Q8.8 div (snapped)");
  dv.run("native (a<<8)/b", [&] {
    ++i;
    std::int32_t a = din.a[i & kMask] << 8, b = din.b[i & kMask] << 8;
    doNotOptimizeAway((a << 8) / b);
  });
  dv.run("inside div(a, b, snapped)", [&] {
    ++i;
    fp a(din.a[i & kMask]), b(din.b[i & kMask]);
    doNotOptimizeAway(div(a, b, snapped)->raw());
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
  acc.run("inside<unsafe> loop", [&] {
    fp sum{0};
    for (auto v : bv) sum += v;
    doNotOptimizeAway(sum.raw());
  });
  finish(acc);

  // signed Q1.14 audio grid
  using q14 = inside<{{-1, 1}, notch<1, 16384>}, unsafe>;
  static_assert(sizeof(q14) == 2);
  auto q14g = group("Q1.14 signed add");
  q14g.run("native int", [&] {
    ++i;
    std::int32_t a = 5000, b = -3000 + static_cast<int>(i & 63);
    doNotOptimizeAway(a + b);
  });
  q14g.run("inside (raw-level)", [&] {
    auto a = q14::from_raw(20000);
    ++i;
    auto b = q14::from_raw(static_cast<std::uint16_t>(10000 + (i & 63)));
    doNotOptimizeAway((a + b).raw());
  });
  finish(q14g);

  // Q16.16
  using q16 = inside<{{0, 65535}, notch<1, 65536>}, unsafe>;
  static_assert(sizeof(q16) == 4);
  auto q16g = group("Q16.16 add");
  q16g.run("native int64", [&] {
    ++i;
    std::int64_t a = static_cast<std::int64_t>(in.a[i & kMask]) << 16;
    std::int64_t b = static_cast<std::int64_t>(in.b[i & kMask]) << 16;
    doNotOptimizeAway(a + b);
  });
  q16g.run("inside<unsafe>", [&] {
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

  using celsius_trunc = inside<{{-40, 60}, 0.5}, snap>;
  using celsius_round = inside<{{-40, 60}, 0.5}, round_nearest>;
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

  using fp = inside<{{0, 255}, 1.0 / 256}, round_nearest>;
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
  using narrow  = inside<{0, 100}, unsafe>;
  using tenths  = inside<{{0, 100}, notch<1, 10>}, unsafe>;
  using quarters= inside<{{0, 100}, notch<1, 4>}, round_nearest>;
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
  using q14a = inside<{{-8, 8},  notch<1, 16384>}, unsafe>;
  using q14b = inside<{{-4, 12}, notch<1, 16384>}, unsafe>;
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
  cmp.run("inside same grid", [&] {
    ++i;
    bool r = bi[i & kMask] < bi[(i + 7) & kMask];
    doNotOptimizeAway(r);
  });
  cmp.run("inside index raw, same notch, cross interval", [&] {
    ++i;
    bool r = ia[i & kMask] < ib[i & kMask];
    doNotOptimizeAway(r);
  });
  finish(cmp);

  // equality — same-grid raw compare, plus the inside-vs-raw-scalar overloads
  // (value-raw compares in imax; index-raw Q8.8 goes through rational).
  using fp_u = inside<{{0, 255}, 1.0 / 256}, unsafe>;
  std::vector<fp_u> qv(kMask + 1);
  for (std::size_t j = 0; j <= kMask; ++j)
    qv[j] = static_cast<int>(j % 250);
  auto eq = group("comparison ==");
  eq.run("native int16", [&] {
    ++i;
    bool r = ni[i & kMask] == ni[(i + 7) & kMask];
    doNotOptimizeAway(r);
  });
  eq.run("inside same grid", [&] {
    ++i;
    bool r = bi[i & kMask] == bi[(i + 7) & kMask];
    doNotOptimizeAway(r);
  });
  eq.run("inside == int scalar (value raw)", [&] {
    ++i;
    bool r = bi[i & kMask] == 42;
    doNotOptimizeAway(r);
  });
  eq.run("inside Q8.8 == int scalar (index raw)", [&] {
    ++i;
    bool r = qv[i & kMask] == 42;
    doNotOptimizeAway(r);
  });
  finish(eq);

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
    using u200c = inside<{0, 200}, checked>;
    s9k v(static_cast<int>(i % 200));
    doNotOptimizeAway(u200c{v}.raw());
  });
  finish(casts);

  auto wrp = group("wrap_cast (s9k value into u200)");
  wrp.run("native euclidean mod", [&] {
    ++i;
    int v = static_cast<int>(i % 9000) - 500;
    auto c = static_cast<std::uint8_t>((v % 201 + 201) % 201);
    doNotOptimizeAway(c);
  });
  wrp.run("wrap_cast", [&] {
    ++i;
    s9k v(static_cast<int>(i % 9000) - 500);
    doNotOptimizeAway(wrap_cast<u200>(v).raw());
  });
  finish(wrp);

  // conversions OUT of inside — the API-boundary direction (stores are above).
  using dyadic_real = inside<{{-8, 8}, notch<1, 16384>}, round_nearest | f64>;
  std::vector<dyadic_real> rv(kMask + 1);
  std::vector<u200> uv(kMask + 1);
  for (std::size_t j = 0; j <= kMask; ++j)
  {
    rv[j] = rational{static_cast<imax>(j * 16) - 8192, 16384};
    uv[j] = static_cast<int>(j % 201);
  }
  auto conv = group("convert out (inside -> scalar)");
  conv.run("native int passthrough", [&] {
    ++i;
    int v = ni[i & kMask];
    doNotOptimizeAway(v);
  });
  conv.run("static_cast<imax> (integer grid)", [&] {
    ++i;
    doNotOptimizeAway(static_cast<imax>(bi[i & kMask]));
  });
  conv.run("to<uint8_t> (expected)", [&] {
    ++i;
    doNotOptimizeAway(*uv[i & kMask].to<std::uint8_t>());
  });
  conv.run("static_cast<double> (f64-backed)", [&] {
    ++i;
    doNotOptimizeAway(static_cast<double>(rv[i & kMask]));
  });
  conv.run("numerator() (Q8.8)", [&] {
    ++i;
    doNotOptimizeAway(qv[i & kMask].numerator());
  });
  finish(conv);

  // dot
  static const scalar_inputs din{-90, 90, -90, 90};
  auto dg = group("dot (2D, signed)");
  dg.run("native int", [&] {
    ++i;
    int ax = din.a[i & kMask], ay = din.b[i & kMask];
    int bx = din.b[(i + 3) & kMask], by = din.a[(i + 5) & kMask];
    doNotOptimizeAway(ax * bx + ay * by);
  });
  dg.run("beman::inside::dot", [&] {
    using c100 = inside<{-100, 100}, unsafe>;
    ++i;
    c100 ax(din.a[i & kMask]), ay(din.b[i & kMask]);
    c100 bx(din.b[(i + 3) & kMask]), by(din.a[(i + 5) & kMask]);
    doNotOptimizeAway(beman::inside::dot(ax, ay, bx, by).raw());
  });
  finish(dg);
}

//---------------------------------------------------------------------------
// generic helpers — min/max (same + mixed grid), midpoint, lerp, cross,
// add_all/mul_all.
//---------------------------------------------------------------------------
static void bench_helpers()
{
  static const scalar_inputs in{-90, 90, -90, 90};
  std::size_t i = 0;
  using c100 = inside<{-100, 100}, unsafe>;

  auto mn = group("min / max (same grid)");
  mn.run("native std::min", [&] {
    ++i;
    doNotOptimizeAway(std::min(in.a[i & kMask], in.b[i & kMask]));
  });
  mn.run("beman::inside::min", [&] {
    ++i;
    c100 a(in.a[i & kMask]), b(in.b[i & kMask]);
    doNotOptimizeAway(beman::inside::min(a, b).raw());
  });
  mn.run("beman::inside::max", [&] {
    ++i;
    c100 a(in.a[i & kMask]), b(in.b[i & kMask]);
    doNotOptimizeAway(beman::inside::max(a, b).raw());
  });
  finish(mn);

  // mixed grids: both operands convert into the hull type ([0,100] 1/10 grid).
  using narrow = inside<{0, 100}, unsafe>;
  using tenths = inside<{{0, 100}, notch<1, 10>}, unsafe>;
  static const scalar_inputs pin{0, 100, 0, 100};
  std::vector<tenths> tv(kMask + 1);
  for (std::size_t j = 0; j <= kMask; ++j)
    tv[j] = rational{static_cast<imax>((j * 13 + 5) % 1001), 10};
  auto mm = group("min (mixed grids -> hull)");
  mm.run("native int on 1/10 lattice", [&] {
    ++i;
    int a = pin.a[i & kMask] * 10, b = static_cast<int>((i * 13 + 5) % 1001);
    doNotOptimizeAway(std::min(a, b));
  });
  mm.run("beman::inside::min(narrow, tenths)", [&] {
    ++i;
    narrow a(pin.a[i & kMask]);
    doNotOptimizeAway(beman::inside::min(a, tv[i & kMask]).raw());
  });
  finish(mm);

  static const scalar_inputs uin{0, 90, 0, 90};
  auto mp = group("midpoint (u8 [0,200])");
  mp.run("native (a+b)/2", [&] {
    ++i;
    doNotOptimizeAway((uin.a[i & kMask] + uin.b[i & kMask]) / 2);
  });
  mp.run("beman::inside::midpoint", [&] {
    ++i;
    u200 a(uin.a[i & kMask]), b(uin.b[i & kMask]);
    doNotOptimizeAway(beman::inside::midpoint(a, b).raw());
  });
  finish(mp);

  // lerp on Q8.8 endpoints with a [0,1] 1/256 parameter.
  using fp88 = inside<{{0, 255}, 1.0 / 256}, unsafe>;
  using t256 = inside<{{0, 1}, notch<1, 256>}, unsafe>;
  auto lp = group("lerp (Q8.8, t in [0,1])");
  lp.run("native a + (((b-a)*t)>>8)", [&] {
    ++i;
    std::int32_t a = uin.a[i & kMask] << 8, b = uin.b[i & kMask] << 8;
    std::int32_t t = static_cast<std::int32_t>(i & 255);
    doNotOptimizeAway(a + (((b - a) * t) >> 8));
  });
  lp.run("beman::inside::lerp", [&] {
    ++i;
    fp88 a(uin.a[i & kMask]), b(uin.b[i & kMask]);
    t256 t = t256::from_raw(static_cast<t256::raw_type>(i & 255));
    doNotOptimizeAway(beman::inside::lerp(a, b, t).raw());
  });
  finish(lp);

  auto cg = group("cross (2D, signed)");
  cg.run("native int", [&] {
    ++i;
    int ax = in.a[i & kMask], ay = in.b[i & kMask];
    int bx = in.b[(i + 3) & kMask], by = in.a[(i + 5) & kMask];
    doNotOptimizeAway(ax * by - ay * bx);
  });
  cg.run("beman::inside::cross", [&] {
    ++i;
    c100 ax(in.a[i & kMask]), ay(in.b[i & kMask]);
    c100 bx(in.b[(i + 3) & kMask]), by(in.a[(i + 5) & kMask]);
    doNotOptimizeAway(beman::inside::cross(ax, ay, bx, by).raw());
  });
  finish(cg);

  auto aa = group("add_all (4 operands)");
  aa.run("native a+b+c+d", [&] {
    ++i;
    doNotOptimizeAway(in.a[i & kMask] + in.b[i & kMask]
                      + in.a[(i + 3) & kMask] + in.b[(i + 5) & kMask]);
  });
  aa.run("beman::inside::add_all", [&] {
    ++i;
    c100 a(in.a[i & kMask]), b(in.b[i & kMask]);
    c100 c(in.a[(i + 3) & kMask]), d(in.b[(i + 5) & kMask]);
    doNotOptimizeAway(add_all(a, b, c, d).raw());
  });
  finish(aa);

  using c15 = inside<{1, 15}, unsafe>;
  static const scalar_inputs min_in{1, 14, 1, 14};
  auto ma = group("mul_all (3 operands)");
  ma.run("native a*b*c", [&] {
    ++i;
    doNotOptimizeAway(min_in.a[i & kMask] * min_in.b[i & kMask]
                      * min_in.a[(i + 3) & kMask]);
  });
  ma.run("beman::inside::mul_all", [&] {
    ++i;
    c15 a(min_in.a[i & kMask]), b(min_in.b[i & kMask]), c(min_in.a[(i + 3) & kMask]);
    doNotOptimizeAway(mul_all(a, b, c).raw());
  });
  finish(ma);
}

//---------------------------------------------------------------------------
// f64-backed (real) arithmetic — one double op behind the fp fast arm;
// parity with native double is the expectation being pinned.
//---------------------------------------------------------------------------
static void bench_fp_backed()
{
  using dyadic_real = inside<{{-8, 8}, notch<1, 16384>}, round_nearest | f64>;
  std::vector<dyadic_real> va(kMask + 1), vb(kMask + 1);
  std::vector<double> da(kMask + 1), db(kMask + 1);
  for (std::size_t j = 0; j <= kMask; ++j)
  {
    rational qa{static_cast<imax>((j * 16) & 0x3FFF) - 8192, 16384};
    rational qb{static_cast<imax>((j * 48 + 16) & 0x3FFF) - 8192, 16384};
    va[j] = qa;  vb[j] = qb;
    da[j] = static_cast<double>(qa);  db[j] = static_cast<double>(qb);
  }
  std::size_t i = 0;

  auto add = group("f64-backed add (dyadic 1/16384 grid)");
  add.run("native double", [&] {
    ++i;
    doNotOptimizeAway(da[i & kMask] + db[i & kMask]);
  });
  add.run("inside<f64>", [&] {
    ++i;
    doNotOptimizeAway((va[i & kMask] + vb[i & kMask]).raw());
  });
  finish(add);

  auto mul = group("f64-backed mul (dyadic 1/16384 grid)");
  mul.run("native double", [&] {
    ++i;
    doNotOptimizeAway(da[i & kMask] * db[i & kMask]);
  });
  mul.run("inside<f64>", [&] {
    ++i;
    doNotOptimizeAway((va[i & kMask] * vb[i & kMask]).raw());
  });
  finish(mul);
}

//---------------------------------------------------------------------------
// rational computations — the exact path. Native baseline is a hand-rolled
// reduced int64 fraction (cross-multiply + gcd), i.e. the code a user would
// write for the same exactness guarantee.
//---------------------------------------------------------------------------
namespace
{
  struct fraction64
  {
    std::int64_t num{0}, den{1};
  };

  fraction64 fraction_reduce(std::int64_t n, std::int64_t d)
  {
    if (d < 0) { n = -n; d = -d; }
    std::int64_t g = std::gcd(n < 0 ? -n : n, d);
    if (g == 0) g = 1;
    return {n / g, d / g};
  }

  fraction64 fraction_add(fraction64 a, fraction64 b)
  { return fraction_reduce(a.num * b.den + b.num * a.den, a.den * b.den); }

  fraction64 fraction_mul(fraction64 a, fraction64 b)
  { return fraction_reduce(a.num * b.num, a.den * b.den); }

  fraction64 fraction_div(fraction64 a, fraction64 b)
  { return fraction_reduce(a.num * b.den, a.den * b.num); }
}

static void bench_rational()
{
  using exact_grid = inside<{{-8, 8}, notch<1, 1024>}, exact>;
  std::vector<exact_grid> ea(kMask + 1), eb(kMask + 1), ep(kMask + 1);
  std::vector<rational> qa(kMask + 1), qb(kMask + 1);
  std::vector<fraction64> fa(kMask + 1), fb(kMask + 1), fp_(kMask + 1);
  std::vector<double> da(kMask + 1), db(kMask + 1);
  std::vector<s9k> ib(kMask + 1);
  for (std::size_t j = 0; j <= kMask; ++j)
  {
    imax ka = static_cast<imax>((j * 7 + 3) % 16384) - 8192;
    imax kb = static_cast<imax>((j * 13 + 5) % 16384) - 8192;
    imax kp = static_cast<imax>((j * 11 + 7) % 8191) + 1;   // positive divisor
    ea[j] = rational{ka, 1024};  eb[j] = rational{kb, 1024};
    ep[j] = rational{kp, 1024};
    qa[j] = rational{ka, 1024};  qb[j] = rational{kb, 1024};
    fa[j] = fraction_reduce(ka, 1024);  fb[j] = fraction_reduce(kb, 1024);
    fp_[j] = fraction_reduce(kp, 1024);
    da[j] = static_cast<double>(qa[j]);  db[j] = static_cast<double>(qb[j]);
    ib[j] = static_cast<int>(j % 200);
  }
  std::size_t i = 0;
  constexpr std::uint64_t kIters = 100'000;

  auto add = group("exact-backed add (rational storage)", kIters);
  add.run("native fraction", [&] {
    ++i;
    doNotOptimizeAway(fraction_add(fa[i & kMask], fb[i & kMask]).num);
  });
  add.run("std double (reference)", [&] {
    ++i;
    doNotOptimizeAway(da[i & kMask] + db[i & kMask]);
  });
  add.run("inside<exact>", [&] {
    ++i;
    doNotOptimizeAway((ea[i & kMask] + eb[i & kMask]).raw().Numerator);
  });
  finish(add);

  auto mul = group("exact-backed mul (rational storage)", kIters);
  mul.run("native fraction", [&] {
    ++i;
    doNotOptimizeAway(fraction_mul(fa[i & kMask], fb[i & kMask]).num);
  });
  mul.run("inside<exact>", [&] {
    ++i;
    doNotOptimizeAway((ea[i & kMask] * eb[i & kMask]).raw().Numerator);
  });
  finish(mul);

  auto dv = group("exact-backed div (rational storage)", kIters);
  dv.run("native fraction", [&] {
    ++i;
    doNotOptimizeAway(fraction_div(fa[i & kMask], fp_[i & kMask]).num);
  });
  dv.run("inside<exact>", [&] {
    ++i;
    doNotOptimizeAway((ea[i & kMask] / ep[i & kMask])->raw().Numerator);
  });
  finish(dv);

  auto cp = group("exact-backed compare <", kIters);
  cp.run("native fraction cross-multiply", [&] {
    ++i;
    fraction64 a = fa[i & kMask], b = fb[i & kMask];
    doNotOptimizeAway(a.num * b.den < b.num * a.den);
  });
  cp.run("inside<exact>", [&] {
    ++i;
    doNotOptimizeAway(ea[i & kMask] < eb[i & kMask]);
  });
  finish(cp);

  // rational-raw operand meets an integer-backed inside: the residual rational
  // branch the Tier-3 integer gates deliberately exclude.
  auto mx = group("mixed exact + integer-grid add", kIters);
  mx.run("native fraction + int", [&] {
    ++i;
    fraction64 a = fa[i & kMask];
    std::int64_t n = static_cast<std::int64_t>(ib[i & kMask].raw());
    doNotOptimizeAway(fraction_add(a, fraction64{n, 1}).num);
  });
  mx.run("inside<exact> + inside integer grid", [&] {
    ++i;
    doNotOptimizeAway((ea[i & kMask] + ib[i & kMask]).raw().Numerator);
  });
  finish(mx);

  // detail::rational directly — the primitive every slow path sits on.
  auto ra = group("rational engine add", kIters);
  ra.run("native fraction", [&] {
    ++i;
    doNotOptimizeAway(fraction_add(fa[i & kMask], fb[i & kMask]).num);
  });
  ra.run("detail::rational", [&] {
    ++i;
    doNotOptimizeAway((qa[i & kMask] + qb[i & kMask])->Numerator);
  });
  finish(ra);

  auto rm = group("rational engine mul", kIters);
  rm.run("native fraction", [&] {
    ++i;
    doNotOptimizeAway(fraction_mul(fa[i & kMask], fb[i & kMask]).num);
  });
  rm.run("detail::rational", [&] {
    ++i;
    doNotOptimizeAway((qa[i & kMask] * qb[i & kMask])->Numerator);
  });
  finish(rm);

  auto rc = group("rational engine compare <", kIters);
  rc.run("native fraction cross-multiply", [&] {
    ++i;
    fraction64 a = fa[i & kMask], b = fb[i & kMask];
    doNotOptimizeAway(a.num * b.den < b.num * a.den);
  });
  rc.run("detail::rational", [&] {
    ++i;
    doNotOptimizeAway(qa[i & kMask] < qb[i & kMask]);
  });
  finish(rc);
}

//---------------------------------------------------------------------------
// inside_range iteration — materialize every grid value into an array (a sum
// would let the compiler close-form the whole loop on both sides).
//---------------------------------------------------------------------------
static void bench_range()
{
  using int_range  = inside_range<{0, 999}, unsafe>;
  using frac_range = inside_range<{{0, 4}, notch<1, 256>}, unsafe>;  // 1025 slots

  std::vector<std::int16_t> out_n(1000);
  std::vector<std::int16_t> out_b(1000);
  auto rg = group("iterate inside_range 1000 (per element)", 300);
  rg.batch(1000);
  rg.run("native counting loop", [&] {
    std::size_t k = 0;
    for (int v = 0; v <= 999; ++v) out_n[k++] = static_cast<std::int16_t>(v);
    doNotOptimizeAway(out_n[0]);
  });
  rg.run("inside_range integer grid", [&] {
    std::size_t k = 0;
    for (auto b : int_range{}) out_b[k++] = static_cast<std::int16_t>(b.raw());
    doNotOptimizeAway(out_b[0]);
  });
  finish(rg);

  std::vector<std::int16_t> qout_n(frac_range::slot_count);
  std::vector<std::int16_t> qout_b(frac_range::slot_count);
  auto rq = group("iterate inside_range Q-grid 1/256 (per element)", 300);
  rq.batch(frac_range::slot_count);
  rq.run("native counting loop", [&] {
    std::size_t k = 0;
    for (int v = 0; v < static_cast<int>(frac_range::slot_count); ++v)
      qout_n[k++] = static_cast<std::int16_t>(v);
    doNotOptimizeAway(qout_n[0]);
  });
  rq.run("inside_range 1/256 grid", [&] {
    std::size_t k = 0;
    for (auto b : frac_range{}) qout_b[k++] = static_cast<std::int16_t>(b.raw());
    doNotOptimizeAway(qout_b[0]);
  });
  finish(rq);
}

//---------------------------------------------------------------------------
// cmath — inside math engines vs std double, per function. Inputs pre-built
// (grids match tests/beman/inside/cmath.test.cpp).
//---------------------------------------------------------------------------
static void bench_cmath()
{
  using algeb_t   = inside<{{-8, 8}, notch<1, 16384>}, round_nearest | f64>;
  using sqrt_in_t = inside<{{0, 4}, notch<1, 65536>}, round_nearest | f64>;
  using exp2_in_t = inside<{{-4, 4}, notch<1, 16384>}, round_nearest | f64>;
  using log2_in_t = inside<{{0x1p-8_r, 256}, notch<1, 16384>}, round_nearest | f64>;
  using exp_in_t  = inside<{{-10, 10}, notch<1, 16384>}, round_nearest | f64>;
  using log_in_t  = inside<{{0x1p-8_r, 256}, notch<1, 256>}, round_nearest | f64>;
  using pow_in_t  = inside<{{-9, 9}, notch<1, 16384>}, round_nearest | f64>;
  using angle_t   = inside<{{-8, 8}, notch<1, 16384>}, round_nearest | f64>;
  using angle_f32_t = inside<{{-8, 8}, notch<1, 16384>}, round_nearest | f32>;
  using tan_in_t  = inside<{{-0.75_r, 0.75_r}, notch<1, 16384>}, round_nearest | f64>;
  using atan2_in_t= inside<{{-1, 1}, notch<1, 16384>}, round_nearest | f64>;
  using fmod_x_t  = inside<{{-8, 8}, notch<1, 16384>}, round_nearest>;
  using fmod_y_t  = inside<{{0.25_r, 4}, notch<1, 16384>}, round_nearest>;
  using pow_b_t   = inside<{{1, 16}, notch<1, 65536>}, round_nearest | f64>;
  using pow_e_t   = inside<{{-2, 2}, notch<1, 16384>}, round_nearest | f64>;

  constexpr std::size_t M = 4096;
  std::vector<algeb_t>    v_alg, v_alg2; std::vector<sqrt_in_t> v_sqrt;
  std::vector<exp2_in_t>  v_exp2;  std::vector<log2_in_t>  v_log2;
  std::vector<exp_in_t>   v_exp;   std::vector<log_in_t>   v_log;
  std::vector<pow_in_t>   v_pow;   std::vector<angle_t>    v_ang;
  std::vector<angle_f32_t> v_angf;
  std::vector<tan_in_t>   v_tan;   std::vector<atan2_in_t> v_aty, v_atx;
  std::vector<fmod_x_t>   v_fmx;   std::vector<fmod_y_t>   v_fmy;
  std::vector<pow_b_t>    v_powb;  std::vector<pow_e_t>    v_powe;
  std::vector<double> d_qs, d_q, d_log2, d_log, d_tan, d_aty, d_atx, d_fmy,
                      d_powb, d_powe;
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
    rational rpb{65536 + (k % 65536), 65536};            // base in [1, 2)
    v_powb.push_back(pow_b_t{rpb});  v_powe.push_back(pow_e_t{qs});
    d_powb.push_back(static_cast<double>(rpb));
    d_powe.push_back(static_cast<double>(qs));
    d_qs.push_back(static_cast<double>(qs));   d_q.push_back(static_cast<double>(q));
    d_log2.push_back(static_cast<double>(rl2)); d_log.push_back(static_cast<double>(rl));
    d_tan.push_back(static_cast<double>(rt));   d_aty.push_back(static_cast<double>(ry));
    d_atx.push_back(static_cast<double>(rx));   d_fmy.push_back(static_cast<double>(rfy));
  }

  std::size_t i = 0;
  constexpr std::size_t J = M - 1;

  // std double first (native baseline), inside second.
#define BEMAN_INSIDE_CMATH_GROUP(title, std_expr, ins_expr)                       \
  {                                                                      \
    auto bench = group(title, 50'000);                               \
    bench.run("std double", [&] { ++i; doNotOptimizeAway(std_expr); });  \
    bench.run("beman::inside::math inside", [&] { ++i; doNotOptimizeAway(ins_expr); }); \
    finish(bench);                                                       \
  }

  BEMAN_INSIDE_CMATH_GROUP("math: abs",   std::abs(d_qs[i & J]),   beman::inside::math::abs(v_alg[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: floor", std::floor(d_qs[i & J]), beman::inside::math::floor(v_alg[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: round", std::round(d_qs[i & J]), beman::inside::math::round(v_alg[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: sqrt",  std::sqrt(d_q[i & J]),   beman::inside::math::sqrt(v_sqrt[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: exp2",  std::exp2(d_qs[i & J]),  beman::inside::math::exp2(v_exp2[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: log2",  std::log2(d_log2[i & J]),beman::inside::math::log2(v_log2[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: exp",   std::exp(d_qs[i & J]),   beman::inside::math::exp(v_exp[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: log",   std::log(d_log[i & J]),  beman::inside::math::log(v_log[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: pow10", std::pow(10.0, d_qs[i & J]), beman::inside::math::pow_base<10>(v_pow[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: sin",   std::sin(d_qs[i & J]),   beman::inside::math::sin(v_ang[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: cos",   std::cos(d_qs[i & J]),   beman::inside::math::cos(v_ang[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: tan",   std::tan(d_tan[i & J]),  beman::inside::math::tan(v_tan[i & J]))
  BEMAN_INSIDE_CMATH_GROUP("math: atan2", std::atan2(d_aty[i & J], d_atx[i & J]),
                                 beman::inside::math::atan2(v_aty[i & J], v_atx[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: fmod",  std::fmod(d_qs[i & J], d_fmy[i & J]),
                                 beman::inside::math::fmod(v_fmx[i & J], v_fmy[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: asin",  std::asin(d_aty[i & J]), beman::inside::math::asin(v_aty[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: tanh",  std::tanh(d_qs[i & J]),  beman::inside::math::tanh(v_exp[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: cbrt",  std::cbrt(d_qs[i & J]),  beman::inside::math::cbrt(v_alg[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: hypot", std::hypot(d_qs[i & J], d_q[i & J]),
                                 beman::inside::math::hypot(v_alg[i & J], v_alg2[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: ceil",  std::ceil(d_qs[i & J]),  beman::inside::math::ceil(v_alg[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: atan",  std::atan(d_aty[i & J]), beman::inside::math::atan(v_aty[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: acos",  std::acos(d_aty[i & J]), beman::inside::math::acos(v_aty[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: sinh",  std::sinh(d_qs[i & J]),  beman::inside::math::sinh(v_exp[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: log10", std::log10(d_log[i & J]),beman::inside::math::log10(v_log[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: pown<3>", d_qs[i & J] * d_qs[i & J] * d_qs[i & J],
                                 beman::inside::math::pown<3>(v_alg[i & J]).raw())
  BEMAN_INSIDE_CMATH_GROUP("math: pow (expected)",
                                 std::pow(d_powb[i & J], d_powe[i & J]),
                                 (*beman::inside::math::pow(v_powb[i & J], v_powe[i & J])).raw())
#undef BEMAN_INSIDE_CMATH_GROUP

#ifndef BEMAN_INSIDE_MATH_CORDIC
  {
    auto bench = group("math: sin (binary32 / float engine)", 50'000);
    bench.run("std sinf", [&] {
      ++i;
      doNotOptimizeAway(std::sin(static_cast<float>(d_qs[i & J])));
    });
    bench.run("beman::inside::math::flt::sin (f32 inside)", [&] {
      ++i;
      doNotOptimizeAway(beman::inside::math::flt::sin(v_angf[i & J]).raw());
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
  sort.run("inside", [&] {
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
  nth.run("inside", [&] {
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
  part.run("inside", [&] {
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
  find.run("find inside", [&] {
    doNotOptimizeAway(rng::find(bv_copy, s9k{7000})->raw());
  });
  find.run("count native", [&] {
    doNotOptimizeAway(rng::count(nv_copy, static_cast<std::int16_t>(42)));
  });
  find.run("count inside", [&] {
    doNotOptimizeAway(rng::count(bv_copy, s9k{42}));
  });
  finish(find);

  auto mm = group("min/max_element 10k (per element)", 30);
  mm.batch(SZ);
  mm.run("min_element native", [&] { doNotOptimizeAway(*rng::min_element(nv_copy)); });
  mm.run("min_element inside",  [&] { doNotOptimizeAway(rng::min_element(bv_copy)->raw()); });
  mm.run("max_element native", [&] { doNotOptimizeAway(*rng::max_element(nv_copy)); });
  mm.run("max_element inside",  [&] { doNotOptimizeAway(rng::max_element(bv_copy)->raw()); });
  finish(mm);

  auto lb = group("lower_bound 10k");
  lb.run("native int16", [&] {
    ++i;
    auto target = static_cast<std::int16_t>(static_cast<int>(i % 9501) - 500);
    doNotOptimizeAway(*rng::lower_bound(ns, target));
  });
  lb.run("inside", [&] {
    ++i;
    auto target = s9k{static_cast<int>(i % 9501) - 500};
    doNotOptimizeAway(rng::lower_bound(bs, target)->raw());
  });
  finish(lb);

  // transform with the uint8-width type: u255 fills uint8 and compares
  // lane-for-lane with native.
  static_assert(sizeof(u255) == 1);
  std::vector<std::uint8_t> tn(SZ);
  std::vector<u255> tb(SZ);
  std::vector<std::uint8_t> tno(SZ);
  std::vector<u255> tbo(SZ);
  for (std::size_t j = 0; j < SZ; ++j)
  {
    tn[j] = static_cast<std::uint8_t>(j % 250);
    tb[j] = static_cast<int>(j % 250);
  }
  auto tf = group("transform v+1 10k (per element)", 30);
  tf.batch(SZ);
  tf.run("native uint8", [&] {
    std::transform(tn.begin(), tn.end(), tno.begin(),
      [](std::uint8_t v) -> std::uint8_t { return static_cast<std::uint8_t>(v + 1); });
    doNotOptimizeAway(tno[0]);
  });
  tf.run("inside u255 (8-bit storage)", [&] {
    std::transform(tb.begin(), tb.end(), tbo.begin(),
      [](u255 v) { v += 1_ins; return v; });
    doNotOptimizeAway(tbo[0].raw());
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
      "Generated by `beman.inside.bench` (tests/beman/inside/bench.cpp, ankerl::nanobench) — do not edit\n"
      "by hand. Regenerate with `cmake --build <build-dir> --target beman.inside.perf_report`\n"
      "(Release, `-O3 -DNDEBUG`, GCC 15, `-mfma` (BEMAN_INSIDE_FMA), no `-march`; one run pinned to a single\n"
      "core with `taskset` on the maintainer's machine, `powersave` governor with\n"
      "turbo — sub-ns rows move ±15–20% between runs, instruction counts don't).\n\n"
      "Each table's FIRST row is the native implementation; the `relative`\n"
      "column reads as native-time / row-time, so 100% is native parity and\n"
      "smaller percentages are slower. Absolute ns/op values are host-specific\n"
      "— the ratios are the stable signal. Instruction/cycle/branch columns\n"
      "appear when the kernel grants perf-counter access.\n\n"
      "Known slow paths (measured in the tables below, by design):\n"
      "arithmetic with a rational-raw operand takes the exact rational path —\n"
      "its cost is measured directly in the exact-backed / rational-engine\n"
      "tables against a hand-rolled reduced int64 fraction (the mixed\n"
      "integer/notch-offset add and cross-grid non-integer stores have folded\n"
      "integer fast paths, gated on imax-safe spans); a rational RHS in a\n"
      "compound assign takes the same exact branch; `inside<checked>`\n"
      "element-wise loops keep a per-element range check — prefer `beman::inside::sum`'s\n"
      "bulk check, which validates the total instead.\n\n"
      "## Comparison with `slim::optional` (1bb5fd9 → d07d272)\n\n"
      "One-time snapshot of the switch from the sentinel-encoded `slim::optional`\n"
      "to `std::expected<T, errc>` and full-range storage. Both commits were built\n"
      "with identical flags and run as 5 interleaved rounds pinned to one core;\n"
      "values are medians. Across all 202 rows present in both builds the median\n"
      "after/before time ratio is 1.00 and 185 rows are within ±10%. Of the 17\n"
      "outside that band, 16 have identical instruction counts before and after\n"
      "(12 of them native/reference rows that share no library code) — timing noise\n"
      "under the powersave governor; the remaining one is `clamp_cast` below. The\n"
      "table lists the rows whose code path returns `std::expected` or whose\n"
      "instruction count changed.\n\n"
      "| Group / row | ns before | ns after | instr. before → after | Verdict |\n"
      "|---|--:|--:|--:|---|\n"
      "| div (u8) `a / b`, `div(a, b, snapped)` | 2.04 | 2.06 | 13 → 11 | unchanged |\n"
      "| compound `/=` (u8) | 2.04 | 2.05 | 18 → 15 | unchanged |\n"
      "| exact-backed div `inside<exact>` | 17.01 | 18.16 | 177 → 161 | noise (wide spread before, 13.0–17.5) |\n"
      "| rational engine add | 8.32 | 8.17 | 177 → 183 | within noise |\n"
      "| rational engine mul | 10.44 | 10.48 | 202 → 194 | within noise |\n"
      "| exact-backed add `inside<exact>` | 6.36 | 6.86 | 134 → 146 | harness artifact¹ |\n"
      "| casts `clamp_cast` | 0.75 | 1.52 | 18 → 39 | harness artifact¹ |\n"
      "| mod (u8) `%`, `mod(a, b, snapped)` | 2.04 | 2.05 | 14 → 14, 11 → 11 | unchanged |\n"
      "| transform v+1, `inside<{0,255}>` vs native | 1.92× native | 1.00× native | 7528 → 3775 | **faster** — now uint8 storage² |\n\n"
      "¹ Standalone microbenchmarks of these exact kernels, built against both\n"
      "commits, show identical instruction counts and timings (`clamp_cast` 18 instr. /\n"
      "0.76 ns, exact add 143 instr. / 6.7 ns) and identical `clamp_cast` assembly;\n"
      "the difference comes from how the large bench TU inlines neighbouring lambdas\n"
      "after the sentinel group was removed.\n"
      "² Without the reserved sentinel slot, `inside<{0,255}>` fits uint8 and runs at\n"
      "native lane width; before it needed uint16 (the old workaround was\n"
      "`inside<{0,254}>`). The `sentinel store` group no longer exists.\n\n"
      "Takeaway: returning `std::expected` instead of the sentinel optional costs\n"
      "nothing measurable on these paths — the wrapper only exists on the fallible\n"
      "return and is consumed immediately.\n\n";

  bench_scalar_u8();
  bench_compound();
  bench_accumulate();
  bench_signed();
  bench_fixed_point();
  bench_store_convert();
  bench_helpers();
  bench_fp_backed();
  bench_rational();
  bench_range();
  bench_cmath();
  bench_algorithms();
  return 0;
}
