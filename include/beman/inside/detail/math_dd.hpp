// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
// The double-double kernels of the math engine's dd tier (cmath_adaptive.hpp):
// values as an unevaluated sum Hi + Lo of two doubles (about 106 bits), with
// error-free sums and products (std::fma), for outputs finer than the double
// tier decides. Every constant — ln 2, π, the 2^(j/64), 2^(j/4096) and sin(jπ/128)
// tables, the Taylor coefficients — comes at compile time from the integer
// path's exact series (detail/math_adaptive.hpp), never from a generator.
// Results are within about 2^-100 relative; the tier bounds them generously
// and lets the integer path decide whenever the bound does not.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_MATH_DD_HPP
#define BEMAN_INSIDE_DETAIL_MATH_DD_HPP

#include <beman/inside/detail/math_adaptive.hpp>
#include <beman/inside/detail/math_fp.hpp>

#ifndef BEMAN_INSIDE_MATH_NO_FP

#include <array>
#include <bit>
#include <cmath>            // std::fma
#include <concepts>
#include <cstdint>
#include <utility>

namespace beman::inside::math::detail::dd
{
  using namespace ::beman::inside::detail;
  namespace ax = ::beman::inside::math::detail::ax;
  namespace fpk = ::beman::inside::math::detail::fp;

  struct dd
  {
    double Hi;
    double Lo;
  };

  //---------------------------------------------------------------------------
  // Error-free transformations and the arithmetic on them (QD-library style).
  // Products use std::fma at runtime and Dekker's split at compile time.
  //---------------------------------------------------------------------------
  // An operand of an error-free sum, fenced off from FMA contraction: under
  // -ffp-contract=fast (GCC's default) an operand that is a product, such as
  // a quotient digit a·(1/b), may be fused into the sum, which then adds the
  // exact product while the error term subtracts the rounded one. The fence
  // costs nothing at runtime. (Clang contracts only within one expression.)
  constexpr double fenced(double x) noexcept
  {
#if defined(__has_builtin)
#  if __has_builtin(__builtin_assoc_barrier)
    return __builtin_assoc_barrier(x);
#  elif __has_builtin(__arithmetic_fence)
    return __arithmetic_fence(x);
#  else
    return x;
#  endif
#else
    return x;
#endif
  }

  constexpr dd two_sum(double a, double b) noexcept
  {
    a = fenced(a);
    b = fenced(b);
    const double s = a + b, bb = s - a;
    return {s, (a - (s - bb)) + (b - bb)};
  }

  // |a| ≥ |b| (or a == 0).
  constexpr dd fast_two_sum(double a, double b) noexcept
  {
    a = fenced(a);
    b = fenced(b);
    const double s = a + b;
    return {s, b - (s - a)};
  }

  constexpr dd split(double a) noexcept
  {
    const double c = 134217729.0 * a;                     // 2^27 + 1
    const double hi = c - (c - a);
    return {hi, a - hi};
  }

  // The product rounded once, as a value the compiler cannot fuse further:
  // under -ffp-contract=fast (GCC's default) a plain a·b feeding a later
  // subtraction may become one fma, which subtracts the exact product where
  // the error-free split expects the rounded one (the error then counts
  // twice). fma(a, b, +0) is the same rounded product (+0 keeps it from
  // folding back to a·b) and costs one instruction with hardware FMA; without
  // it nothing contracts.
  inline double rounded_product(double a, double b) noexcept
  {
#if defined(__FMA__) || defined(__ARM_FEATURE_FMA)
    return std::fma(a, b, 0.0);
#else
    return a * b;
#endif
  }

  constexpr dd two_prod(double a, double b) noexcept
  {
    if consteval
    {
      const double p = a * b;
      const dd x = split(a), y = split(b);
      return {p, ((x.Hi * y.Hi - p) + x.Hi * y.Lo + x.Lo * y.Hi) + x.Lo * y.Lo};
    }
    else
    {
      const double p = rounded_product(a, b);
      return {p, std::fma(a, b, -p)};
    }
  }

  constexpr dd neg(dd a) noexcept { return {-a.Hi, -a.Lo}; }

  constexpr dd add(dd a, dd b) noexcept
  {
    dd s = two_sum(a.Hi, b.Hi);
    const dd t = two_sum(a.Lo, b.Lo);
    s = fast_two_sum(s.Hi, s.Lo + t.Hi);
    return fast_two_sum(s.Hi, s.Lo + t.Lo);
  }

  constexpr dd add(dd a, double b) noexcept
  {
    const dd s = two_sum(a.Hi, b);
    return fast_two_sum(s.Hi, s.Lo + a.Lo);
  }

  constexpr dd sub(dd a, dd b) noexcept { return add(a, neg(b)); }

  // a + b for |b| ≤ |a| (or a == 0), as in a Horner step: no cancellation, so
  // the high parts need only fast_two_sum.
  constexpr dd add_dominant(dd a, dd b) noexcept
  {
    const dd s = fast_two_sum(a.Hi, b.Hi);
    return fast_two_sum(s.Hi, s.Lo + (a.Lo + b.Lo));
  }

  constexpr dd add_dominant(dd a, double b) noexcept
  {
    const dd s = fast_two_sum(a.Hi, b);
    return fast_two_sum(s.Hi, s.Lo + a.Lo);
  }

  constexpr dd mul(dd a, dd b) noexcept
  {
    const dd p = two_prod(a.Hi, b.Hi);
    return fast_two_sum(p.Hi, p.Lo + (a.Hi * b.Lo + a.Lo * b.Hi));
  }

  constexpr dd mul(dd a, double b) noexcept
  {
    const dd p = two_prod(a.Hi, b);
    return fast_two_sum(p.Hi, p.Lo + a.Lo * b);
  }

  constexpr dd sqr(dd a) noexcept
  {
    const dd p = two_prod(a.Hi, a.Hi);
    return fast_two_sum(p.Hi, p.Lo + 2 * a.Hi * a.Lo);
  }

  // a/b: two quotient digits from one reciprocal; the remainder
  // a − q1·b cancels its high part exactly (Sterbenz).
  constexpr dd div(dd a, dd b) noexcept
  {
    const double inv = 1 / b.Hi;
    const double q1 = a.Hi * inv;
    const dd p = two_prod(q1, b.Hi);
    const double r = ((a.Hi - p.Hi) - p.Lo) + (a.Lo - q1 * b.Lo);
    return fast_two_sum(q1, r * inv);
  }

  constexpr dd ldexp(dd a, int e) noexcept
  { return {::beman::inside::detail::ldexp(a.Hi, e), ::beman::inside::detail::ldexp(a.Lo, e)}; }

  //---------------------------------------------------------------------------
  // Constants from the integer path's fixed-point values Y·2^-S.
  //---------------------------------------------------------------------------
  template <std::size_t K>
  constexpr dd of_fixed(wide_sint<K> y, int S) noexcept
  {
    const bool negative = y.negative();
    if (negative) y = -y;
    if (y.is_zero()) return {0, 0};
    const int n = bit_width_of(y);
    if (n <= 53) return {::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(y)), -S), 0};
    const int sh = n - 53;
    const wide_sint<K> top = y >> sh;
    const wide_sint<K> rest = y - (top << sh);
    const double hi = ::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(top)), sh - S);
    const double lo = sh > 53
        ? ::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(rest >> (sh - 53))), sh - 53 - S)
        : ::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(rest)), -S);
    const dd r = fast_two_sum(hi, lo);
    return negative ? neg(r) : r;
  }

  inline constexpr int kS = 136;                          // the scale constants are read at
  using fixed = ax::fixed_t<2 * kS + 16>;

  template <std::size_t K>
  constexpr dd of_fixed_any(wide_sint<K> const& y, int S) noexcept { return of_fixed(static_cast<fixed>(y), S); }

  // c·2^-S cut into N parts of B bits each (the last one of 53), so that
  // k·part is exact for |k| < 2^(53−B): Cody–Waite reduction constants.
  template <std::size_t N>
  constexpr std::array<double, N> parts(fixed y, int S, int B) noexcept
  {
    std::array<double, N> r{};
    int n = bit_width_of(y);                              // the bits still to place
    for (std::size_t i = 0; i < N && !y.is_zero(); ++i)
    {
      const int bits = i + 1 < N ? B : 53;
      const int sh = n > bits ? n - bits : 0;
      const fixed top = y >> sh;
      r[i] = ::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(top)), sh - S);
      y = y - (top << sh);
      n = sh;
    }
    return r;
  }

  // 1/n! and 1/n at full dd precision.
  template <int S>
  constexpr dd inv_fact(int n) noexcept
  {
    fixed f{1};
    for (int i = 2; i <= n; ++i) f = f * fixed{i};
    return of_fixed((fixed{1} << (2 * S)) / f, 2 * S);
  }
  template <int S>
  constexpr dd inv_int(int n) noexcept { return of_fixed((fixed{1} << (2 * S)) / fixed{n}, 2 * S); }

  // 2^(j/2^shift), j = 0 … 63: the root 2^(1/2^shift) from the integer exp,
  // then its powers at scale S (a unit lost per step).
  template <int S>
  constexpr std::array<dd, 64> exp2_table(int shift) noexcept
  {
    const auto a = ax::exp_fixed<S>(static_cast<fixed>(ax::ln2_q<S>) >> shift, 1, 4);
    const fixed root = a.Scale == S ? a.Value : a.Value << (S - a.Scale);
    std::array<dd, 64> t{};
    fixed p = fixed{1} << S;
    for (std::size_t j = 0; j < 64; ++j)
    {
      t[j] = of_fixed(p, S);
      p = (p * root) >> S;
    }
    return t;
  }

  // sin(jπ/128), j = 0 … 255: cos and sin of π/128 by five half-angle square
  // roots from cos π/4, then j rotations at scale kS (a unit lost per step).
  template <int S>
  constexpr std::array<dd, 256> sin_table() noexcept
  {
    const fixed one = fixed{1} << S;
    auto root = [](fixed v) { return ax::isqrt(v << S); };        // √(v·2^-S) at scale S
    fixed c = root(one >> 1);                                        // cos π/4
    for (int i = 0; i < 4; ++i) c = root((one + c) >> 1);          // cos π/64
    const fixed s1 = root((one - c) >> 1), c1 = root((one + c) >> 1);   // π/128
    std::array<dd, 256> t{};
    fixed sj{0}, cj = one;
    for (int j = 0; j <= 64; ++j)
    {
      const dd v = of_fixed(sj, S);
      t[static_cast<std::size_t>(j)] = v;
      t[static_cast<std::size_t>(128 - j)] = v;
      t[static_cast<std::size_t>(128 + j)] = neg(v);
      if (j > 0) t[static_cast<std::size_t>(256 - j)] = neg(v);
      const fixed sn = (sj * c1 + cj * s1) >> S;
      cj = (cj * c1 - sj * s1) >> S;
      sj = sn;
    }
    t[64] = dd{1, 0};
    t[192] = dd{-1, 0};
    return t;
  }

  // atan(j/32), j = 0 … 32, from the integer atan: one constant expression
  // per entry, so each has the compiler's whole evaluation budget.
  template <int S, int J>
  inline constexpr dd atan_entry = of_fixed(ax::atan_fixed<S>(fixed{J} << (S - 5), 0).Value, S);

  template <int S>
  constexpr std::array<dd, 33> atan_table() noexcept
  {
    return []<int... J>(std::integer_sequence<int, J...>) {
      return std::array<dd, 33>{atan_entry<S, J>...};
    }(std::make_integer_sequence<int, 33>{});
  }

  // ln(j/128), j = 96 … 192: sums of ln(i/(i − 1)) = 2·atanh(1/(2i − 1))
  // outward from j = 128, each a short series at scale S + 16 (a unit lost
  // per term).
  template <int S>
  constexpr std::array<dd, 97> log_table() noexcept
  {
    constexpr int W = S + 16;
    auto step = [](int i) {
      const fixed d{2 * i - 1}, d2 = d * d;
      fixed t = (fixed{1} << W) / d, sum{0};
      for (int k = 1; !t.is_zero(); k += 2)
      {
        sum = sum + t / fixed{k};
        t = t / d2;
      }
      return sum + sum;
    };
    std::array<dd, 97> r{};
    fixed acc{0};
    for (int j = 129; j <= 192; ++j) r[static_cast<std::size_t>(j - 96)] = of_fixed(acc = acc + step(j), W);
    acc = fixed{0};
    for (int j = 127; j >= 96; --j) r[static_cast<std::size_t>(j - 96)] = of_fixed(acc = acc - step(j + 1), W);
    return r;
  }

  // Every constant and table, as members of a class template: computed on
  // first use only, so a translation unit that never reaches the dd tier
  // pays nothing for them (the scale S is a parameter so that GCC cannot
  // fold the initializers early). D is always dd.
  template <typename D, int S = kS>
  struct consts
  {
    static constexpr D Ln2    = of_fixed_any(ax::ln2_q<S>, S);
    static constexpr D Log2e  = of_fixed_any(ax::log2e_q<S>, S);
    static constexpr D Log10e = of_fixed_any(ax::log10e_q<S>, S);
    static constexpr D Pi     = of_fixed_any(ax::pi_q<S>, S);
    static constexpr D HalfPi = ldexp(Pi, -1);
    // ln 2/4096 in parts of 31 bits (|k| < 2^22: |x| ≤ 700 · 4096/ln 2), and
    // π/128 in parts of 27 bits (|n| < 2^26: |x| ≤ 2^20 · 128/π).
    static constexpr std::array<double, 3> Ln2By4096 = parts<3>(static_cast<fixed>(ax::ln2_q<S>), S + 12, 31);
    static constexpr std::array<double, 4> PiBy128   = parts<4>(static_cast<fixed>(ax::pi_q<S>), S + 7, 27);
    static constexpr std::array<D, 64> Exp2By64   = exp2_table<S>(6);
    static constexpr std::array<D, 64> Exp2By4096 = exp2_table<S>(12);
    static constexpr std::array<D, 256> Sin = sin_table<S>();
    static constexpr std::array<D, 33> Atan = atan_table<S>();
    static constexpr std::array<D, 97> Log = log_table<S>();
    static constexpr D F2 = inv_fact<S>(2), F3 = inv_fact<S>(3), F4 = inv_fact<S>(4), F5 = inv_fact<S>(5);
    static constexpr D F6 = inv_fact<S>(6), F7 = inv_fact<S>(7);
    static constexpr D I3 = inv_int<S>(3), I5 = inv_int<S>(5), I7 = inv_int<S>(7);
  };

  inline constexpr double kInvLn2By4096 = 4096 * fpk::kLog2e;
  inline constexpr double kInvPiBy128 = 128 / 0x1.921fb54442d18p+1;

  // a·2^m for |m| ≤ 1022 (a power-of-two factor, exact unless the result is
  // subnormal).
  inline dd scale(dd a, long m) noexcept
  {
    const double f = std::bit_cast<double>(static_cast<std::uint64_t>(m + 1023) << 52);
    return {a.Hi * f, a.Lo * f};
  }

  // x − k·(c0 + c1 + c2 + c3), the parts from parts<>: x.Hi − k·c0 is exact
  // (k·c0 fits 53 bits and lies within a factor 2 of x.Hi), k·c1 and k·c2
  // are exact and summed beside it, and k·c3 is far below the result's
  // last bit — so the chain is two sums long.
  inline dd reduce(dd x, double k, double c0, double c1, double c2, double c3) noexcept
  {
    const double a = std::fma(-k, c0, x.Hi);
    const dd w = fast_two_sum(k * c1, k * c2);
    const dd d = two_sum(a, -w.Hi);
    return fast_two_sum(d.Hi, d.Lo + ((x.Lo - w.Lo) - k * c3));
  }

  //---------------------------------------------------------------------------
  // Kernels. Each is within about 2^-96 of its value (relative, plus an
  // absolute 2^-99 where a subtraction cancels; dd_kernels_stay_far_inside_
  // their_bound in math_adaptive.test.cpp checks it). Horner steps add a
  // small product to a larger coefficient, so they use add_dominant. They are
  // templates on D = dd only so that consts<D> is instantiated on first use.
  //---------------------------------------------------------------------------
  template <typename D>
  concept dd_type = std::same_as<D, dd>;

  // e^x for |x| ≤ 700: x = k·ln 2/4096 + r, |r| ≤ ln 2/8192, k = 4096m + 64i + j,
  // e^x = 2^m · 2^(i/64) · 2^(j/4096) · (1 + p(r)). The terms of p from r^4
  // on are below 2^-53 relative and summed in double.
  template <dd_type D>
  inline D exp(D x) noexcept
  {
    using C = consts<D>;
    const double k = __builtin_nearbyint(x.Hi * kInvLn2By4096);
    const D r = reduce(x, k, C::Ln2By4096[0], C::Ln2By4096[1], C::Ln2By4096[2], 0);
    const double rh = r.Hi;
    const double tail = fpk::horner(rh, 1.0 / 5040, 1.0 / 720, 1.0 / 120, 1.0 / 24);
    D p = add_dominant(C::F3, rh * tail);                  // 1/3! + r/4! + …
    p = add_dominant(C::F2, mul(r, p));                    // 1/2! + r/3! + …
    p = add_dominant(r, mul(sqr(r), p));                   // e^r − 1
    const long ik = static_cast<long>(k);
    const D t = mul(C::Exp2By64[static_cast<std::size_t>((ik >> 6) & 63)],
                    C::Exp2By4096[static_cast<std::size_t>(ik & 63)]);
    return scale(add_dominant(t, mul(t, p)), ik >> 12);
  }

  // ln x for normal x > 0: x = 2^m·f, f in [0.75, 1.5), c = j/128 the
  // table point nearest f, ln x = m·ln 2 + ln c + 2·atanh s with
  // s = (f − c)/(f + c), |s| ≤ 1/384. atanh s = s·Σ s^2k/(2k+1); the terms
  // from s^7 on are below 2^-53 relative and summed in double, those from
  // s^13 on below 2^-106.
  template <dd_type D>
  inline D log(D x) noexcept
  {
    using C = consts<D>;
    long m = static_cast<long>(std::bit_cast<std::uint64_t>(x.Hi) >> 52) - 1023;
    D f = scale(x, -m);                                    // [1, 2)
    if (f.Hi >= 1.5) { f = {f.Hi * 0.5, f.Lo * 0.5}; ++m; }
    const double j = __builtin_nearbyint(f.Hi * 128);
    const double c = j * 0x1p-7;
    const D s = div(two_sum(f.Hi - c, f.Lo), add(f, c));   // f.Hi − c is exact
    const D z = sqr(s);
    const double zh = z.Hi;
    D p = add_dominant(C::I5, zh * fpk::horner(zh, 1.0 / 11, 1.0 / 9, 1.0 / 7));
    p = add_dominant(C::I3, mul(z, p));                    // 1/3 + z/5 + …
    p = add_dominant(s, mul(mul(s, z), p));                // atanh s
    const D t = add(mul(C::Ln2, static_cast<double>(m)), C::Log[static_cast<std::size_t>(j) - 96]);
    return add(t, D{2 * p.Hi, 2 * p.Lo});
  }

  struct sincos_t { dd Sin, Cos; };

  // sin and cos for |x| ≤ 2^20: x = n·π/128 + r, |r| ≤ π/256, then
  // sin(a + r) = sin a + (sin a·(cos r − 1) + cos a·sin r) and the like with
  // the table. The Taylor terms from r^9 (sin) and r^8 (cos) on are below
  // 2^-53 relative and summed in double.
  template <dd_type D>
  [[gnu::always_inline]] inline sincos_t sincos(D x) noexcept
  {
    using C = consts<D>;
    const double n = __builtin_nearbyint(x.Hi * kInvPiBy128);
    const D r = reduce(x, n, C::PiBy128[0], C::PiBy128[1], C::PiBy128[2], C::PiBy128[3]);
    const D z = sqr(r);
    const double zh = z.Hi;
    const double st = fpk::horner(zh, 1.0 / 6227020800.0, -1.0 / 39916800.0, 1.0 / 362880.0);
    const double ct = fpk::horner(zh, 1.0 / 479001600.0, -1.0 / 3628800.0, 1.0 / 40320.0);
    D s = add_dominant(neg(C::F7), zh * st);              // −1/7! + z/9! − …
    s = add_dominant(C::F5, mul(z, s));
    s = add_dominant(neg(C::F3), mul(z, s));
    const D sr = add_dominant(r, mul(mul(r, z), s));      // sin r
    D c = add_dominant(neg(C::F6), zh * ct);
    c = add_dominant(C::F4, mul(z, c));
    c = add_dominant(neg(C::F2), mul(z, c));
    const D cm = mul(z, c);                                // cos r − 1
    const long j = static_cast<long>(n);
    const D sa = C::Sin[static_cast<std::size_t>(j & 255)];
    const D ca = C::Sin[static_cast<std::size_t>((j + 64) & 255)];
    return {add_dominant(sa, add(mul(sa, cm), mul(ca, sr))),
            add_dominant(ca, sub(mul(ca, cm), mul(sa, sr)))};
  }

  template <dd_type D> inline D sin(D x) noexcept { return sincos(x).Sin; }
  template <dd_type D> inline D cos(D x) noexcept { return sincos(x).Cos; }

  // atan a for 0 ≤ a ≤ 1: c = j/32 nearest a, atan a = atan c + atan t with
  // t = (a − c)/(1 + a·c), |t| ≤ 1/64. The series terms from t^9 on (2^-51
  // relative at most) are summed in double, within 2^-104.
  template <dd_type D>
  inline D atan_unit(D a) noexcept
  {
    using C = consts<D>;
    const double j = __builtin_nearbyint(a.Hi * 32);
    const double c = j * (1.0 / 32);
    const D t = div(add(a, -c), add_dominant(D{1, 0}, mul(a, c)));
    const D z = sqr(t);
    const double zh = z.Hi;
    const double tail = fpk::horner(zh, -1.0 / 19, 1.0 / 17, -1.0 / 15, 1.0 / 13, -1.0 / 11, 1.0 / 9);
    D p = add_dominant(neg(C::I7), zh * tail);            // −1/7 + z/9 − …
    p = add_dominant(C::I5, mul(z, p));
    p = add_dominant(neg(C::I3), mul(z, p));
    const D r = add_dominant(t, mul(mul(t, z), p));        // atan t
    return add_dominant(C::Atan[static_cast<std::size_t>(j)], r);
  }

  // atan2(y, x): the ratio of the smaller to the larger magnitude, then the
  // octant.
  template <dd_type D>
  inline D atan2(D y, D x) noexcept
  {
    using C = consts<D>;
    const double ay = y.Hi < 0 ? -y.Hi : y.Hi, ax_ = x.Hi < 0 ? -x.Hi : x.Hi;
    if (ay == 0 && ax_ == 0) return D{0, 0};
    if (ay <= ax_)
    {
      const D q = div(y, x);
      const D r = q.Hi < 0 ? neg(atan_unit(neg(q))) : atan_unit(q);
      if (x.Hi > 0) return r;
      return y.Hi < 0 ? sub(r, C::Pi) : add(r, C::Pi);
    }
    const D q = div(x, y);                                // |q| < 1
    const D r = q.Hi < 0 ? neg(atan_unit(neg(q))) : atan_unit(q);
    return y.Hi > 0 ? sub(C::HalfPi, r) : sub(neg(C::HalfPi), r);
  }

  template <dd_type D>
  inline D atan(D x) noexcept
  {
    const bool negative = x.Hi < 0;
    const D a = negative ? neg(x) : x;
    const D r = a.Hi <= 1 ? atan_unit(a) : sub(consts<D>::HalfPi, atan_unit(div(D{1, 0}, a)));
    return negative ? neg(r) : r;
  }

  // √x: one Newton step from the correctly rounded double root.
  template <dd_type D>
  inline D sqrt(D x) noexcept
  {
    if (!(x.Hi > 0)) return D{0, 0};
    const double y = fpk::fp_sqrt(x.Hi);
    const D e = sub(x, two_prod(y, y));
    return fast_two_sum(y, e.Hi / (2 * y));
  }

  // ∛x: one Newton step from the double root.
  template <dd_type D>
  inline D cbrt(D x) noexcept
  {
    if (x.Hi == 0) return D{0, 0};
    const double y = fpk::seed_cbrt(x.Hi);
    const D e = sub(mul(two_prod(y, y), y), x);          // y³ − x
    return fast_two_sum(y, -e.Hi / (3 * y * y));
  }

  template <dd_type D> inline D tan(D x) noexcept { const sincos_t sc = sincos(x); return div(sc.Sin, sc.Cos); }

  template <dd_type D> inline D exp2(D x) noexcept  { return exp(mul(x, consts<D>::Ln2)); }
  template <dd_type D> inline D log2(D x) noexcept  { return mul(log(x), consts<D>::Log2e); }
  template <dd_type D> inline D log10(D x) noexcept { return mul(log(x), consts<D>::Log10e); }
  template <dd_type D> inline D sinh(D x) noexcept  { const D e = exp(x); return ldexp(sub(e, div(D{1, 0}, e)), -1); }
  template <dd_type D> inline D cosh(D x) noexcept  { const D e = exp(x); return ldexp(add(e, div(D{1, 0}, e)), -1); }
  template <dd_type D> inline D tanh(D x) noexcept  { const D e = exp(ldexp(x, 1)); return div(add(e, -1.0), add(e, 1.0)); }

  // The inverse hyperbolics on |x|, as in the double tier.
  template <dd_type D>
  inline D asinh(D x) noexcept
  {
    const bool negative = x.Hi < 0;
    const D a = negative ? neg(x) : x;
    const D m = log(add(a, sqrt(add(sqr(a), 1.0))));
    return negative ? neg(m) : m;
  }
  template <dd_type D> inline D acosh(D x) noexcept { return log(add(x, sqrt(mul(add(x, -1.0), add(x, 1.0))))); }
  template <dd_type D>
  inline D atanh(D x) noexcept
  {
    const bool negative = x.Hi < 0;
    const D a = negative ? neg(x) : x;
    const D m = ldexp(log(div(add(a, 1.0), add(neg(a), 1.0))), -1);
    return negative ? neg(m) : m;
  }
  template <dd_type D> inline D asin(D x) noexcept { return atan2(x, sqrt(mul(add(neg(x), 1.0), add(x, 1.0)))); }
  template <dd_type D> inline D acos(D x) noexcept { return atan2(sqrt(mul(add(neg(x), 1.0), add(x, 1.0))), x); }
  template <dd_type D> inline D hypot(D x, D y) noexcept { return sqrt(add(sqr(x), sqr(y))); }
} // namespace beman::inside::math::detail::dd

#endif // !BEMAN_INSIDE_MATH_NO_FP

#endif // BEMAN_INSIDE_DETAIL_MATH_DD_HPP
