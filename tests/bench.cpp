#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "ctrack.hpp"
#include "bound/bound.hpp"
#include "bound/cmath.hpp"

using namespace bnd;
using namespace bnd::detail;

//---------------------------------------------------------------------------
// compiler barrier
//---------------------------------------------------------------------------
template <typename T>
inline void do_not_optimize(T const& value)
{
  asm volatile("" : : "r,m"(value) : "memory");
}

//---------------------------------------------------------------------------
// checked_u8: minimal runtime-checked bounded integer [0, 200]
//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
// benchmarks
//---------------------------------------------------------------------------
static constexpr std::size_t N = 5'000'000;

using u200 = bound<{0, 200}, unsafe>;

void bench_construct()
{
  for (std::size_t i = 0; i < N; ++i)
  {
    { CTRACK_NAME("construct native");
      auto v = static_cast<std::uint8_t>(i % 201);
      do_not_optimize(v); }

    { CTRACK_NAME("construct clamped");
      auto v = static_cast<std::uint8_t>(std::clamp(i % std::size_t{256}, std::size_t{0}, std::size_t{200}));
      do_not_optimize(v); }

    { CTRACK_NAME("construct checked");
      checked_u8 v(static_cast<int>(i % 201));
      do_not_optimize(v.value); }

    { CTRACK_NAME("construct bound");
      u200 v(static_cast<int>(i % 201));
      do_not_optimize(v.raw()); }
  }
}

void bench_add()
{
  constexpr int a_val = 30, b_val = 50;

  for (std::size_t i = 0; i < N; ++i)
  {
    { CTRACK_NAME("add native");
      std::uint8_t a = a_val, b = b_val;
      auto c = static_cast<std::uint8_t>(a + b);
      do_not_optimize(c); }

    { CTRACK_NAME("add clamped");
      std::uint8_t a = a_val, b = b_val;
      auto c = static_cast<std::uint8_t>(std::clamp(a + b, 0, 200));
      do_not_optimize(c); }

    { CTRACK_NAME("add checked");
      checked_u8 a(a_val), b(b_val);
      auto c = a + b;
      do_not_optimize(c.value); }

    { CTRACK_NAME("add bound");
      u200 a(a_val), b(b_val);
      auto c = a + b;
      do_not_optimize(c.raw()); }
  }
}

void bench_mul()
{
  constexpr int a_val = 7, b_val = 14;

  for (std::size_t i = 0; i < N; ++i)
  {
    { CTRACK_NAME("mul native");
      std::uint8_t a = a_val, b = b_val;
      auto c = static_cast<std::uint8_t>(a * b);
      do_not_optimize(c); }

    { CTRACK_NAME("mul clamped");
      std::uint8_t a = a_val, b = b_val;
      auto c = static_cast<std::uint8_t>(std::clamp(a * b, 0, 200));
      do_not_optimize(c); }

    { CTRACK_NAME("mul checked");
      checked_u8 a(a_val), b(b_val);
      auto c = a * b;
      do_not_optimize(c.value); }

    { CTRACK_NAME("mul bound");
      u200 a(a_val), b(b_val);
      auto c = a * b;
      do_not_optimize(c.raw()); }
  }
}

void bench_div()
{
  constexpr int a_val = 100, b_val = 7;

  for (std::size_t i = 0; i < N; ++i)
  {
    { CTRACK_NAME("div native");
      std::uint8_t a = a_val, b = b_val;
      auto c = static_cast<std::uint8_t>(a / b);
      do_not_optimize(c); }

    { CTRACK_NAME("div bound (rational)");
      u200 a(a_val), b(b_val);
      auto c = a / b;
      do_not_optimize(c->raw()); }

    { CTRACK_NAME("div bound (integer)");
      u200 a(a_val), b(b_val);
      auto c = div(a, b, truncated);
      do_not_optimize(c->raw()); }
  }
}

void bench_accumulate()
{
  constexpr std::size_t SZ = 1000;
  constexpr std::size_t ITERS = N / SZ;

  // All use the same element type: uint32_t / bound<{0, 200'000}>
  using u200k         = bound<{0, 200'000}, unsafe>;
  using u200k_checked = bound<{0, 200'000}, checked>;

  std::vector<std::uint32_t> nv(SZ);
  std::vector<u200k> bv(SZ);
  std::vector<u200k_checked> bv_checked(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    nv[i] = static_cast<std::uint32_t>(i % 5);
    bv[i] = static_cast<int>(i % 5);
    bv_checked[i] = static_cast<int>(i % 5);
  }

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("accum native");
      std::uint32_t sum = 0;
      for (auto v : nv) sum += v;
      do_not_optimize(sum); }

    { CTRACK_NAME("accum bound");
      u200k sum(0);
      for (auto v : bv) sum += v;
      do_not_optimize(sum.raw()); }

    { CTRACK_NAME("accum bound<checked>");
      u200k_checked sum(0);
      for (auto v : bv_checked) sum += v;
      do_not_optimize(sum.raw()); }
  }
}

void bench_int_vs_bound()
{
  constexpr std::size_t SZ = 1000;
  constexpr std::size_t ITERS = N / SZ;

  // int (signed 32-bit) vs bound<{-100'000, 100'000}, unsafe> (uint32_t storage)
  // Uses negative numbers throughout
  using s100k = bound<{-100'000, 100'000}, unsafe>;

  std::vector<int> iv(SZ);
  std::vector<s100k> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    int val = static_cast<int>(i % 11) - 5; // -5 to +5
    iv[i] = val;
    bv[i] = val;
  }

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("int construct");
      int v = static_cast<int>(i % 201) - 100; // -100 to +100
      do_not_optimize(v); }

    { CTRACK_NAME("bound construct");
      s100k v(static_cast<int>(i % 201) - 100);
      do_not_optimize(v.raw()); }

    { CTRACK_NAME("int add");
      int a = -300, b = 500;
      int c = a + b;
      do_not_optimize(c); }

    { CTRACK_NAME("bound add");
      s100k a(-300), b(500);
      auto c = a + b;
      do_not_optimize(c.raw()); }

    { CTRACK_NAME("int mul");
      int a = -7, b = 14;
      int c = a * b;
      do_not_optimize(c); }

    { CTRACK_NAME("bound mul");
      s100k a(-7), b(14);
      auto c = a * b;
      do_not_optimize(c.raw()); }

    { CTRACK_NAME("int accum");
      int sum = 0;
      for (auto v : iv) sum += v;
      do_not_optimize(sum); }

    { CTRACK_NAME("bound accum");
      s100k sum(0);
      for (auto v : bv) sum += v;
      do_not_optimize(sum.raw()); }
  }
}

void bench_fixed_point()
{
  constexpr std::size_t SZ = 1000;
  constexpr std::size_t ITERS = N / SZ;

  // Fixed-point 8.8: native int16_t with manual shift vs bound<{{0, 255}, 1.0/256}>
  using fp = bound<{{0, 255}, 1.0/256}, unsafe>;

  std::vector<std::int32_t> nv(SZ);
  std::vector<fp> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    int val = static_cast<int>(i % 5);
    nv[i] = val << 8; // fixed-point: value * 256
    bv[i] = val;
  }

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("fixed native construct");
      std::int32_t v = static_cast<std::int32_t>((i % 201)) << 8;
      do_not_optimize(v); }

    { CTRACK_NAME("fixed bound construct");
      fp v(static_cast<int>(i % 201));
      do_not_optimize(v.raw()); }

    { CTRACK_NAME("fixed native add");
      std::int32_t a = 30 << 8, b = 50 << 8;
      std::int32_t c = a + b;
      do_not_optimize(c); }

    { CTRACK_NAME("fixed bound add");
      fp a(30), b(50);
      auto c = a + b;
      do_not_optimize(c.raw()); }

    { CTRACK_NAME("fixed native accum");
      std::int32_t sum = 0;
      for (auto v : nv) sum += v;
      do_not_optimize(sum); }

    { CTRACK_NAME("fixed bound accum");
      fp sum(0);
      for (auto v : bv) sum += v;
      do_not_optimize(sum.raw()); }

    { CTRACK_NAME("fixed native mul");
      std::int32_t a = 30 << 8, b = 5 << 8;
      std::int32_t c = (a * b) >> 8;  // renormalize Q8.8
      do_not_optimize(c); }

    { CTRACK_NAME("fixed bound mul");
      fp a(30), b(5);
      auto c = a * b;
      do_not_optimize(c.raw()); }

    { CTRACK_NAME("fixed native div");
      std::int32_t a = 200 << 8, b = 8 << 8;
      std::int32_t c = (a << 8) / b;  // renormalize Q8.8
      do_not_optimize(c); }

    { CTRACK_NAME("fixed bound div");
      fp a(200), b(8);
      auto c = div(a, b, truncated);
      do_not_optimize(c->raw()); }
  }
}

void bench_fixed_point_signed()
{
  // Signed Q1.14-style audio: bound<{{-1, 1}, 1/16384}, unsafe>
  // vs native int16 with explicit *16384 scaling.
  using fp = bound<{{-1, 1}, notch<1, 16384>}, unsafe>;
  static_assert(sizeof(fp) == 2);

  for (std::size_t i = 0; i < N; ++i)
  {
    int v = static_cast<int>(i % 201) - 100; // [-100, 100]

    { CTRACK_NAME("signed fixed native construct");
      std::int32_t s = (v * 16384) / 100;  // scale into Q1.14 range
      do_not_optimize(s); }

    { CTRACK_NAME("signed fixed bound construct");
      auto s = fp::from_raw(static_cast<std::uint16_t>((v + 100) * 163));  // raw-level
      do_not_optimize(s.raw()); }

    { CTRACK_NAME("signed fixed native add");
      std::int32_t a = 5000, b = -3000;
      std::int32_t c = a + b;
      do_not_optimize(c); }

    { CTRACK_NAME("signed fixed bound add");
      auto a = fp::from_raw(20000);
      auto b = fp::from_raw(10000);
      auto c = a + b;
      do_not_optimize(c.raw()); }
  }
}

void bench_fixed_point_checked()
{
  // Same Q8.8 grid as bench_fixed_point but with `checked` policy — measures
  // runtime-domain-check overhead vs the unsafe baseline.
  using fp = bound<{{0, 255}, 1.0/256}, checked>;

  for (std::size_t i = 0; i < N; ++i)
  {
    { CTRACK_NAME("checked fixed bound construct");
      fp v(static_cast<int>(i % 201));
      do_not_optimize(v.raw()); }

    { CTRACK_NAME("checked fixed bound add");
      fp a(30), b(50);
      auto c = a + b;
      do_not_optimize(c.raw()); }
  }
}

void bench_fixed_point_q16()
{
  // Q16.16: bound<{{0, 65535}, 1/65536}, unsafe> vs int64 with <<16 scaling.
  // Skip mul/div: Q16.16*Q16.16 forces a wider result type than uint32.
  using fp = bound<{{0, 65535}, notch<1, 65536>}, unsafe>;
  static_assert(sizeof(fp) == 4);

  constexpr std::size_t SZ = 1000;
  constexpr std::size_t ITERS = N / SZ;

  std::vector<std::int64_t> nv(SZ);
  std::vector<fp> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    int v = static_cast<int>(i % 5);
    nv[i] = static_cast<std::int64_t>(v) << 16;
    bv[i] = v;
  }

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("q16 native construct");
      std::int64_t v = static_cast<std::int64_t>(i % 1001) << 16;
      do_not_optimize(v); }

    { CTRACK_NAME("q16 bound construct");
      fp v(static_cast<int>(i % 1001));
      do_not_optimize(v.raw()); }

    { CTRACK_NAME("q16 native add");
      std::int64_t a = 30LL << 16, b = 50LL << 16;
      std::int64_t c = a + b;
      do_not_optimize(c); }

    { CTRACK_NAME("q16 bound add");
      fp a(30), b(50);
      auto c = a + b;
      do_not_optimize(c.raw()); }

    { CTRACK_NAME("q16 native accum");
      std::int64_t sum = 0;
      for (auto v : nv) sum += v;
      do_not_optimize(sum); }

    { CTRACK_NAME("q16 bound accum");
      fp sum(0);
      for (auto v : bv) sum += v;
      do_not_optimize(sum.raw()); }
  }
}

void bench_round_nearest()
{
  // Compare truncation (ignore_round) vs round_nearest for float->bound assignment
  // Celsius: -40 to 60 in 0.5 steps
  using celsius_trunc = bound<{{-40, 60}, 0.5}, ignore_round>;
  using celsius_round = bound<{{-40, 60}, 0.5}, round_nearest>;

  for (std::size_t i = 0; i < N; ++i)
  {
    double val = -40.0 + static_cast<double>(i % 200) * 0.5 + 0.3;

    { CTRACK_NAME("assign truncate");
      celsius_trunc c = val;
      do_not_optimize(c.raw()); }

    { CTRACK_NAME("assign round_nearest");
      celsius_round c = val;
      do_not_optimize(c.raw()); }
  }
}

//---------------------------------------------------------------------------
// cmath benchmarks — integer-only constexpr transcendentals.
// Inputs match the Q-formats used in tests/test_cmath.cpp.
//---------------------------------------------------------------------------
void bench_cmath()
{
  using algeb_t = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  using sqrt_in_t = bound<{{0, 4}, notch<1, 65536>}, round_nearest | real>;
  using exp2_in_t = bound<{{-4, 4}, notch<1, 16384>}, round_nearest | real>;
  using log2_in_t = bound<{{0x1p-8_r, 256}, notch<1, 16384>}, round_nearest | real>;
  using exp_in_t  = bound<{{-10, 10}, notch<1, 16384>}, round_nearest | real>;
  using log_in_t  = bound<{{0x1p-8_r, 256}, notch<1, 256>}, round_nearest | real>;
  using pow_in_t  = bound<{{-9, 9}, notch<1, 16384>}, round_nearest | real>;
  using angle_t   = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  using tan_in_t  = bound<{{-0.75_r, 0.75_r}, notch<1, 16384>}, round_nearest | real>;
  using atan2_in_t= bound<{{-1, 1}, notch<1, 16384>}, round_nearest | real>;
  using fmod_x_t  = bound<{{-8, 8}, notch<1, 16384>}, round_nearest>;
  using fmod_y_t  = bound<{{0.25_r, 4}, notch<1, 16384>}, round_nearest>;

  // 28 timed blocks per iter — use a reduced count so ctrack's per-event
  // storage (~70 B/event, no aggregation) stays in proportion with the
  // ~4-block benches above. N/16 ≈ 312K → ~8.7M events.
  constexpr std::size_t N_CMATH = N / 16;
  for (std::size_t i = 0; i < N_CMATH; ++i)
  {
    auto k = static_cast<imax>(i & 0xFFFF);  // small varying integer
    rational q  = rational{k, 16384};        // [0, ~4) in Q.14
    rational qs = rational{k - 32768, 16384}; // signed

    // ---- abs / floor / ceil / round / trunc ----
    { CTRACK_NAME("math::abs   bound");
      algeb_t x{qs};
      auto r = bnd::math::abs(x);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::abs    double");
      double d = static_cast<double>(qs);
      auto r = std::abs(d);
      do_not_optimize(r); }

    { CTRACK_NAME("math::floor bound");
      algeb_t x{qs};
      auto r = bnd::math::floor(x);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::floor  double");
      double d = static_cast<double>(qs);
      auto r = std::floor(d);
      do_not_optimize(r); }

    { CTRACK_NAME("math::round bound");
      algeb_t x{qs};
      auto r = bnd::math::round(x);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::round  double");
      double d = static_cast<double>(qs);
      auto r = std::round(d);
      do_not_optimize(r); }

    // ---- sqrt ----
    { CTRACK_NAME("math::sqrt  bound");
      sqrt_in_t x{q};
      auto r = bnd::math::sqrt(x);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::sqrt   double");
      double d = static_cast<double>(q);
      auto r = std::sqrt(d);
      do_not_optimize(r); }

    // ---- exp2 / log2 ----
    { CTRACK_NAME("math::exp2  bound");
      exp2_in_t x{qs};
      auto r = bnd::math::exp2(x);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::exp2   double");
      double d = static_cast<double>(qs);
      auto r = std::exp2(d);
      do_not_optimize(r); }

    { CTRACK_NAME("math::log2  bound");
      log2_in_t x{rational{(k % 65535) + 1, 16384}};
      auto r = bnd::math::log2(x);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::log2   double");
      double d = static_cast<double>(rational{(k % 65535) + 1, 16384});
      auto r = std::log2(d);
      do_not_optimize(r); }

    // ---- exp / log ----
    { CTRACK_NAME("math::exp   bound");
      exp_in_t x{qs};
      auto r = bnd::math::exp(x);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::exp    double");
      double d = static_cast<double>(qs);
      auto r = std::exp(d);
      do_not_optimize(r); }

    { CTRACK_NAME("math::log   bound");
      log_in_t x{rational{(k % 65535) + 1, 256}};
      auto r = bnd::math::log(x);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::log    double");
      double d = static_cast<double>(rational{(k % 65535) + 1, 256});
      auto r = std::log(d);
      do_not_optimize(r); }

    // ---- pow_base<10> ----
    { CTRACK_NAME("math::pow10 bound");
      pow_in_t x{qs};
      auto r = bnd::math::pow_base<10>(x);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::pow10  double");
      double d = static_cast<double>(qs);
      auto r = std::pow(10.0, d);
      do_not_optimize(r); }

    // ---- sin / cos ----
    { CTRACK_NAME("math::sin   bound");
      angle_t x{qs};
      auto r = bnd::math::sin(x);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::sin    double");
      double d = static_cast<double>(qs);
      auto r = std::sin(d);
      do_not_optimize(r); }

    { CTRACK_NAME("math::cos   bound");
      angle_t x{qs};
      auto r = bnd::math::cos(x);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::cos    double");
      double d = static_cast<double>(qs);
      auto r = std::cos(d);
      do_not_optimize(r); }

    // ---- tan (returns expected, unwrap with operator*) ----
    { CTRACK_NAME("math::tan   bound");
      tan_in_t x{rational{(k % 24576) - 12288, 16384}};
      auto r = bnd::math::tan(x);
      do_not_optimize(r); }
    { CTRACK_NAME("std::tan    double");
      double d = static_cast<double>(rational{(k % 24576) - 12288, 16384});
      auto r = std::tan(d);
      do_not_optimize(r); }

    // ---- atan2 ----
    { CTRACK_NAME("math::atan2 bound");
      atan2_in_t y{rational{(k % 32768) - 16384, 16384}};
      atan2_in_t x{rational{((k + 1) % 32768) - 16384, 16384}};
      auto r = bnd::math::atan2(y, x);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::atan2  double");
      double y = static_cast<double>(rational{(k % 32768) - 16384, 16384});
      double x = static_cast<double>(rational{((k + 1) % 32768) - 16384, 16384});
      auto r = std::atan2(y, x);
      do_not_optimize(r); }

    // ---- fmod ----
    { CTRACK_NAME("math::fmod  bound");
      fmod_x_t fx{qs};
      fmod_y_t fy{rational{(k % 60) + 4, 16384}};
      auto r = bnd::math::fmod(fx, fy);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::fmod   double");
      double fx = static_cast<double>(qs);
      double fy = static_cast<double>(rational{(k % 60) + 4, 16384});
      auto r = std::fmod(fx, fy);
      do_not_optimize(r); }

    // ---- extended transcendentals (#3) ----
    { CTRACK_NAME("math::asin  bound");
      atan2_in_t x{rational{(k % 32768) - 16384, 16384}};
      auto r = bnd::math::asin(x);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::asin   double");
      double d = static_cast<double>(rational{(k % 32768) - 16384, 16384});
      auto r = std::asin(d);
      do_not_optimize(r); }

    { CTRACK_NAME("math::tanh  bound");
      exp_in_t x{qs};
      auto r = bnd::math::tanh(x);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::tanh   double");
      double d = static_cast<double>(qs);
      auto r = std::tanh(d);
      do_not_optimize(r); }

    { CTRACK_NAME("math::cbrt  bound");
      algeb_t x{qs};
      auto r = bnd::math::cbrt(x);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::cbrt   double");
      double d = static_cast<double>(qs);
      auto r = std::cbrt(d);
      do_not_optimize(r); }

    { CTRACK_NAME("math::hypot bound");
      algeb_t a{qs}; algeb_t b{q};
      auto r = bnd::math::hypot(a, b);
      do_not_optimize(r.raw()); }
    { CTRACK_NAME("std::hypot  double");
      double a = static_cast<double>(qs); double b = static_cast<double>(q);
      auto r = std::hypot(a, b);
      do_not_optimize(r); }
  }
}

//---------------------------------------------------------------------------
// STL algorithm benchmarks
//---------------------------------------------------------------------------
namespace rng = std::ranges;

using u255 = bound<{0, 255}, unsafe>;
using s9k  = bound<{-500, 9000}, unsafe>;

void bench_sort_algo()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

  // prepare identical data
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

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("sort native");
      nv = nv_copy;
      rng::sort(nv);
      do_not_optimize(nv[0]); }

    { CTRACK_NAME("sort bound");
      bv = bv_copy;
      rng::sort(bv);
      do_not_optimize(bv[0].raw()); }
  }
}

void bench_find_algo()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

  std::vector<std::int16_t> nv(SZ);
  std::vector<s9k> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    auto val = static_cast<std::int16_t>(i % 9501 - 500);
    nv[i] = val;
    bv[i] = static_cast<int>(val);
  }

  auto target_n = static_cast<std::int16_t>(7000);
  s9k target_b(7000);

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("find native");
      auto it = rng::find(nv, target_n);
      do_not_optimize(*it); }

    { CTRACK_NAME("find bound");
      auto it = rng::find(bv, target_b);
      do_not_optimize(it->raw()); }

    { CTRACK_NAME("count native");
      auto c = rng::count(nv, static_cast<std::int16_t>(42));
      do_not_optimize(c); }

    { CTRACK_NAME("count bound");
      auto c = rng::count(bv, s9k{42});
      do_not_optimize(c); }
  }
}

void bench_transform_algo()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

  std::vector<std::uint8_t> nv(SZ);
  std::vector<u255> bv(SZ);
  std::vector<std::uint8_t> nout(SZ);
  std::vector<u255> bout(SZ);

  for (std::size_t i = 0; i < SZ; ++i)
  {
    nv[i] = static_cast<std::uint8_t>(i % 250);
    bv[i] = static_cast<int>(i % 250);
  }

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("transform native");
      std::transform(nv.begin(), nv.end(), nout.begin(),
        [](std::uint8_t v) -> std::uint8_t { return static_cast<std::uint8_t>(v + 1); });
      do_not_optimize(nout[0]); }

    { CTRACK_NAME("transform bound");
      std::transform(bv.begin(), bv.end(), bout.begin(),
        [](u255 v) { v += 1; return v; });
      do_not_optimize(bout[0].raw()); }
  }
}

void bench_minmax_algo()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

  std::vector<std::int16_t> nv(SZ);
  std::vector<s9k> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    auto val = static_cast<std::int16_t>((i * 7 + 13) % 9501 - 500);
    nv[i] = val;
    bv[i] = static_cast<int>(val);
  }

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("min_element native");
      auto it = rng::min_element(nv);
      do_not_optimize(*it); }

    { CTRACK_NAME("min_element bound");
      auto it = rng::min_element(bv);
      do_not_optimize(it->raw()); }

    { CTRACK_NAME("max_element native");
      auto it = rng::max_element(nv);
      do_not_optimize(*it); }

    { CTRACK_NAME("max_element bound");
      auto it = rng::max_element(bv);
      do_not_optimize(it->raw()); }
  }
}

void bench_lower_bound_algo()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

  std::vector<std::int16_t> nv(SZ);
  std::vector<s9k> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    auto val = static_cast<std::int16_t>(static_cast<int>(i) - 500);
    nv[i] = val;
    bv[i] = static_cast<int>(val);
  }
  // already sorted

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    auto target = static_cast<std::int16_t>(static_cast<int>(i % 9501) - 500);

    { CTRACK_NAME("lower_bound native");
      auto it = rng::lower_bound(nv, target);
      do_not_optimize(*it); }

    { CTRACK_NAME("lower_bound bound");
      auto it = rng::lower_bound(bv, s9k{static_cast<int>(target)});
      do_not_optimize(it->raw()); }
  }
}

void bench_std_sort()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

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

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("std::sort native");
      nv = nv_copy;
      std::sort(nv.begin(), nv.end());
      do_not_optimize(nv[0]); }

    { CTRACK_NAME("std::sort bound");
      bv = bv_copy;
      std::sort(bv.begin(), bv.end());
      do_not_optimize(bv[0].raw()); }
  }
}

void bench_nth_element()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

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
  auto nth = static_cast<long>(SZ / 2);

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("nth_element native");
      nv = nv_copy;
      std::nth_element(nv.begin(), nv.begin() + nth, nv.end());
      do_not_optimize(nv[static_cast<std::size_t>(nth)]); }

    { CTRACK_NAME("nth_element bound");
      bv = bv_copy;
      std::nth_element(bv.begin(), bv.begin() + nth, bv.end());
      do_not_optimize(bv[static_cast<std::size_t>(nth)].raw()); }
  }
}

void bench_partition()
{
  constexpr std::size_t SZ = 10'000;
  constexpr std::size_t ITERS = N / SZ;

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

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("partition native");
      nv = nv_copy;
      auto p = std::partition(nv.begin(), nv.end(),
        [](std::int16_t v) { return v >= 0; });
      do_not_optimize(*p); }

    { CTRACK_NAME("partition bound");
      bv = bv_copy;
      auto p = std::partition(bv.begin(), bv.end(),
        [](s9k v) { return v >= 0; });
      do_not_optimize(p->raw()); }
  }
}

void bench_std_accumulate()
{
  constexpr std::size_t SZ = 1000;
  constexpr std::size_t ITERS = N / SZ;

  using u200k = bound<{0, 200'000}, unsafe>;

  std::vector<std::uint32_t> nv(SZ);
  std::vector<u200k> bv(SZ);
  for (std::size_t i = 0; i < SZ; ++i)
  {
    nv[i] = static_cast<std::uint32_t>(i % 5);
    bv[i] = static_cast<int>(i % 5);
  }

  for (std::size_t i = 0; i < ITERS; ++i)
  {
    { CTRACK_NAME("std::accumulate native");
      auto sum = std::accumulate(nv.begin(), nv.end(), std::uint32_t{0});
      do_not_optimize(sum); }

    { CTRACK_NAME("std::accumulate bound");
      auto sum = std::accumulate(bv.begin(), bv.end(), u200k{0}, std::plus<>{});
      do_not_optimize(sum.raw()); }
  }
}

//---------------------------------------------------------------------------
// main
//---------------------------------------------------------------------------
int main()
{
  std::cout << "bound<> benchmark (" << N << " iterations)\n\n";

  // Print + clear after each bench: ctrack stores every Event uncompressed
  // (~70 B/event), so accumulating all benches before a single print pushed
  // peak RSS toward 30 GB. Clearing between functions bounds peak memory
  // to a single bench's event volume.
  #define RUN(fn) do { std::cout << "\n=== " #fn " ===\n"; fn(); ctrack::result_print(); } while (0)
  RUN(bench_construct);
  RUN(bench_add);
  RUN(bench_mul);
  RUN(bench_div);
  RUN(bench_accumulate);
  RUN(bench_int_vs_bound);
  RUN(bench_fixed_point);
  RUN(bench_fixed_point_signed);
  RUN(bench_fixed_point_checked);
  RUN(bench_fixed_point_q16);
  RUN(bench_round_nearest);
  RUN(bench_cmath);
  RUN(bench_sort_algo);
  RUN(bench_find_algo);
  RUN(bench_transform_algo);
  RUN(bench_minmax_algo);
  RUN(bench_lower_bound_algo);
  RUN(bench_std_sort);
  RUN(bench_nth_element);
  RUN(bench_partition);
  RUN(bench_std_accumulate);
  #undef RUN
  return 0;
}
