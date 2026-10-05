// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
// The double kernels of the math engine's double tier (cmath_adaptive.hpp):
// a small libm in `double` — Taylor polynomials in Horner form with explicit
// std::fma, Cody-Waite range reduction, and only std::fma / sqrt / nearbyint
// from <cmath>.
//
// Every kernel is sized to its output and carries a proved error bound.
// A kernel takes a target T in bits; its polynomials get the fewest terms
// whose truncation stays below 2^-T relative. The bound for that size is
// computed at compile time from the polynomial's own coefficients and
// argument range: the first omitted term, each rounding of the Horner
// steps, the coefficients' and the reduction constants' roundings, and the
// reduction's own roundings. These are the usual first-order bounds of IEEE
// arithmetic in round-to-nearest, where every operation lands within
// kU = 2^-53 of its exact result, relatively. up() widens each bound by
// 2^-30 of itself, which covers the second-order terms and the roundings of
// the bound arithmetic.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_MATH_FP_HPP
#define BEMAN_INSIDE_DETAIL_MATH_FP_HPP

#include <beman/inside/math.hpp>   // beman::inside::detail::ldexp (constexpr, reproducible)
#include <beman/inside/inside.hpp>  // complete inside/rational + has_flag/policy_of/f64 (store<>)

// BEMAN_INSIDE_MATH_NO_FP is resolved in policy_flag.hpp (included via inside.hpp).

#ifndef BEMAN_INSIDE_MATH_NO_FP   // ===== FP engine present (needs <cmath> + an FPU) =====

#include <array>
#include <cmath>            // std::fma, std::sqrt, std::nearbyint ONLY
#include <cstddef>
#include <utility>


namespace beman::inside::math::detail::fp
{
  using std::fma;

  // c0·z^n + c1·z^(n-1) + … + cn as an fma chain from the highest coefficient
  // down (Horner) — the same operation order as writing the chain out by hand.
  template <std::floating_point T, typename... C>
  [[gnu::always_inline]] inline T horner(T z, T c0, C... cs)
  {
    T p = c0;
    ((p = fma(p, z, static_cast<T>(cs))), ...);
    return p;
  }

  // The same over a coefficient array, highest degree first.
  template <std::size_t N>
  [[gnu::always_inline]] inline double horner(double z, std::array<double, N> const& c)
  {
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
      double p = c[0];
      ((p = fma(p, z, c[I + 1])), ...);
      return p;
    }(std::make_index_sequence<N - 1>{});
  }

  inline constexpr double kHalfPiHi = 0x1.921fb54442d18p+0;   // π/2  high
  inline constexpr double kHalfPiLo = 0x1.1a62633145c07p-54;  // π/2  low
  inline constexpr double kTwoOverPi = 0x1.45f306dc9c883p-1;  // 2/π
  inline constexpr double kLn2Hi    = 0x1.62e42fee00000p-1;   // ln2  high (33 bits)
  inline constexpr double kLn2Lo    = 0x1.a39ef35793c76p-33;  // ln2  low
  inline constexpr double kLog2e    = 0x1.71547652b82fep+0;   // 1/ln2
  inline constexpr double kLn2Full  = 0x1.62e42fefa39efp-1;   // ln2
  inline constexpr double kLog10e   = 0x1.bcb7b1526e50ep-2;   // 1/ln10
  inline constexpr double kSqrtHalf = 0x1.6a09e667f3bcdp-1;   // √½
  inline constexpr double kPi       = 0x1.921fb54442d18p+1;   // π
  inline constexpr double kPiHalf   = 0x1.921fb54442d18p+0;   // π/2
  inline constexpr double kPiSixth  = 0x1.0c152382d7366p-1;   // π/6
  inline constexpr double kInvSqrt3 = 0x1.279a74590331cp-1;   // 1/√3 = tan(π/6)
  inline constexpr double kTanPi12  = 0x1.126145e9ecd56p-2;   // tan(π/12) ≈ 0.2679
  inline constexpr double kThird    = 1.0 / 3.0;

  //---------------------------------------------------------------------------
  // The bound arithmetic.
  //---------------------------------------------------------------------------
  inline constexpr double kU = 0x1p-53;               // unit roundoff
  // Absolute slack for results near the bottom of the double range (a
  // subnormal or underflowing ldexp, a square below 2^-1022): far below
  // every grid the tier serves.
  inline constexpr double kTiny = 0x1p-500;

  // How far each constant is from its value (computed offline at 300 bits,
  // rounded up).
  inline constexpr double kHalfPiRes = 0x1p-109;      // |π/2 − Hi − Lo|      (2^-109.04)
  inline constexpr double kLn2Res    = 0x1p-86;       // |ln2 − Hi − Lo|      (2^-86.15)
  inline constexpr double kLn2FullErr  = 1.25 * 0x1p-55; // relative          (2^-54.73)
  inline constexpr double kLog2eErr    = 1.02 * 0x1p-56; // relative          (2^-55.98)
  inline constexpr double kLog10eErr   = 1.0 * 0x1p-55;  // relative          (2^-55.13)
  inline constexpr double kPiErr       = 1.11 * 0x1p-53; // absolute          (2^-52.86)
  inline constexpr double kPiHalfErr   = 1.11 * 0x1p-54; // absolute          (2^-53.86)
  inline constexpr double kPiSixthErr  = 1.0 * 0x1p-54;  // absolute          (2^-54.05)
  inline constexpr double kInvSqrt3Err = 1.21 * 0x1p-55; // absolute          (2^-54.73)

  constexpr double up(double b) { return b * (1 + 0x1p-30); }
  constexpr double pow_n(double x, int n) { double r = 1; for (int i = 0; i < n; ++i) r *= x; return r; }
  constexpr double factorial(int n) { double r = 1; for (int i = 2; i <= n; ++i) r *= i; return r; }
  constexpr double max_d(double a, double b) { return a > b ? a : b; }

  // A polynomial's coefficients, highest degree first, each with its
  // relative distance from the exact rational it stands for.
  template <std::size_t N>
  struct poly
  {
    std::array<double, N> C;
    std::array<double, N> Err;
  };

  // Σ_{k<N} sign(k)/den(k)·z^k: each coefficient one correctly rounded
  // division of integers below 2^53, so within kU of its value (exact when
  // den is a power of two).
  template <std::size_t N, typename F>
  consteval poly<N> series(F term)
  {
    poly<N> p{};
    for (std::size_t k = 0; k < N; ++k)
    {
      const auto [sign, den] = term(static_cast<int>(k));
      if (!(den < 0x1p53)) std::unreachable();             // a denominator must be a double exactly
      int e = 0;
      const bool pow2 = ::beman::inside::detail::frexp(den, &e) == 0.5;
      p.C[N - 1 - k] = sign / den;
      p.Err[N - 1 - k] = pow2 ? 0.0 : kU;
    }
    return p;
  }

  struct term { double Sign, Den; };

  // Horner's evaluation of p (one fma per step) for |z| ≤ Z, where the z it
  // is given lies within relative Dz of the exact argument: Mag bounds |p|
  // and Err the distance of the computed value from p at the exact argument.
  // Step k's value lies within Err of p_k = p_(k−1)·z + c_k; the fma's
  // rounding adds kU of it.
  struct horner_bound { double Mag, Err; };

  template <std::size_t N>
  consteval horner_bound horner_error(poly<N> const& p, double Z, double Dz)
  {
    auto abs = [](double v) { return v < 0 ? -v : v; };
    double P = abs(p.C[0]), E = abs(p.C[0]) * p.Err[0];
    for (std::size_t k = 1; k < N; ++k)
    {
      E = E * Z * (1 + Dz) + P * Z * Dz + abs(p.C[k]) * p.Err[k];
      P = P * Z + abs(p.C[k]);
      E += kU * (P + E);
    }
    return {P, E};
  }

  // The fewest terms n ≤ Max whose truncation trunc(n) is at most 2^-T.
  template <typename F>
  consteval int terms_for(int T, int Max, F trunc)
  {
    double target = 1;
    for (int i = 0; i < T; ++i) target *= 0.5;
    for (int n = 1; n < Max; ++n)
      if (trunc(n) <= target) return n;
    return Max;
  }

  // The target of the full kernels: truncation below 2^-58, a thirty-second
  // of their rounding error.
  inline constexpr int kFullBits = 58;

  //---------------------------------------------------------------------------
  // sin and cos on the reduced argument r, |r| ≤ kRTrig. The reduction of
  // |x| ≤ 2^20 leaves |r| ≤ π/4 + 2^-32 (x·2/π is within 2^-32.6 of exact).
  //---------------------------------------------------------------------------
  inline constexpr double kRTrig = 0x1.9220p-1;              // 0.78540039 ≥ π/4 + 2^-30
  inline constexpr double kZTrig = kRTrig * kRTrig;
  inline constexpr double kSinMin = 1 - kZTrig / 6;          // ≤ sin r / r
  inline constexpr double kCosMin = 1 - kZTrig / 2 + kZTrig * kZTrig / 24 - kZTrig * kZTrig * kZTrig / 720;  // ≤ cos r

  // sin r = r·Σ (−1)^k z^k/(2k+1)!; cos r = Σ (−1)^k z^k/(2k)!, z = r². Both
  // series alternate with falling terms, so the first omitted term bounds
  // the tail.
  constexpr double sin_trunc(int n) { return pow_n(kZTrig, n) / factorial(2 * n + 1) / kSinMin; }
  constexpr double cos_trunc(int n) { return pow_n(kZTrig, n) / factorial(2 * n) / kCosMin; }
  template <int T> inline constexpr int sin_terms = terms_for(T, 9, sin_trunc);
  template <int T> inline constexpr int cos_terms = terms_for(T, 10, cos_trunc);

  template <int N>
  inline constexpr poly<N> sin_c = series<N>([](int k) { return term{k % 2 ? -1.0 : 1.0, factorial(2 * k + 1)}; });
  template <int N>
  inline constexpr poly<N> cos_c = series<N>([](int k) { return term{k % 2 ? -1.0 : 1.0, factorial(2 * k)}; });

  // Relative errors of sin_poly and cos_poly at the r they are given: the
  // Horner error and the truncation over the least |sin r / r| or cos r,
  // and for sin the final product's rounding.
  template <int N>
  inline constexpr double sin_poly_rel = [] {
    const horner_bound h = horner_error(sin_c<N>, kZTrig, kU);
    return up((h.Err + sin_trunc(N) * kSinMin) / kSinMin + kU);
  }();
  template <int N>
  inline constexpr double cos_poly_rel = [] {
    const horner_bound h = horner_error(cos_c<N>, kZTrig, kU);
    return up((h.Err + cos_trunc(N) * kCosMin) / kCosMin);
  }();

  template <int N>
  [[gnu::always_inline]] inline double sin_poly(double r)
  {
    const double z = r * r;
    return r * horner(z, sin_c<N>.C);
  }

  template <int N>
  [[gnu::always_inline]] inline double cos_poly(double r)
  {
    return horner(r * r, cos_c<N>.C);
  }

  // Quadrant reduction: x → r = x − k·π/2 and q = k mod 4. r1 = x − k·Hi is
  // exact: k ≠ 0 needs |x| > ½, so both terms are multiples of 2^-53, and
  // |r1| < 1. The second fma rounds once, and Hi + Lo misses π/2 by
  // kHalfPiRes: r is within kU·|r| + |k|·kHalfPiRes of x − k·π/2.
  inline double reduce_quadrant(double x, long& q, double& k)
  {
    k = std::nearbyint(x * kTwoOverPi);
    double r = fma(-k, kHalfPiHi, x);
    r = fma(-k, kHalfPiLo, r);
    q = static_cast<long>(k) & 3;
    return r;
  }

  // sin, cos and tan of |x| ≤ 2^20, sized to T bits. The reduction's kU·|r|
  // moves sin r by kU·|r| ≤ kU·|sin r|/kSinMin and cos r by kU·r² ≤
  // kU·kZTrig/cos r, relatively; its |k|·kHalfPiRes is absolute, with
  // |k| ≤ 1.2·max(1, |x|). So each value is within Rel·|v| + AbsX·max(1, |x|).
  template <int T>
  struct trig_k
  {
    static constexpr int NS = sin_terms<T>, NC = cos_terms<T>;
    static constexpr double Rel = up(max_d(sin_poly_rel<NS> + kU / kSinMin, cos_poly_rel<NC> + kU * kZTrig / kCosMin));
    static constexpr double AbsX = up(1.2 * kHalfPiRes);
    // tan = s/c or −c/s: both relative errors and the division's; the
    // reduction's kU·|r| moves tan r by 2kU·|r|/|sin 2r| ≤ 1.571kU of it,
    // and |k|·kHalfPiRes by sec² = 1 + t² times itself.
    static constexpr double TanRel = up(sin_poly_rel<NS> + cos_poly_rel<NC> + kU + 1.5710 * kU);

    static double sin(double x, double& bound)
    {
      long q; double k;
      const double r = reduce_quadrant(x, q, k);
      double v;
      switch (q)
      {
        case 0:  v = sin_poly<NS>(r); break;
        case 1:  v = cos_poly<NC>(r); break;
        case 2:  v = -sin_poly<NS>(r); break;
        default: v = -cos_poly<NC>(r); break;
      }
      bound = Rel * __builtin_fabs(v) + AbsX * (__builtin_fabs(x) > 1 ? __builtin_fabs(x) : 1.0) + kTiny;
      return v;
    }

    static double cos(double x, double& bound)
    {
      long q; double k;
      const double r = reduce_quadrant(x, q, k);
      double v;
      switch (q)
      {
        case 0:  v = cos_poly<NC>(r); break;
        case 1:  v = -sin_poly<NS>(r); break;
        case 2:  v = -cos_poly<NC>(r); break;
        default: v = sin_poly<NS>(r); break;
      }
      bound = Rel * __builtin_fabs(v) + AbsX * (__builtin_fabs(x) > 1 ? __builtin_fabs(x) : 1.0) + kTiny;
      return v;
    }

    // False on a pole (odd quadrant with s == 0).
    static bool tan(double x, double& t, double& bound)
    {
      long q; double k;
      const double r = reduce_quadrant(x, q, k);
      const double s = sin_poly<NS>(r), c = cos_poly<NC>(r);
      if (q & 1)
      {
        if (s == 0.0) return false;
        t = -c / s;
      }
      else
        t = s / c;
      const double mx = __builtin_fabs(x) > 1 ? __builtin_fabs(x) : 1.0;
      bound = TanRel * __builtin_fabs(t) + (1 + t * t) * AbsX * mx + kTiny;
      return true;
    }
  };

  //---------------------------------------------------------------------------
  // e^r on |r| ≤ kRExp: Σ r^k/k!, whose tail is at most r^n/n!·e^|r|.
  //---------------------------------------------------------------------------
  inline constexpr double kRExp = 0x1.63p-2;                 // 0.34668 ≥ ln2/2 + 2^-40
  inline constexpr double kExpMax = 1.4144;                  // ≥ e^kRExp
  constexpr double exp_trunc(int n) { return pow_n(kRExp, n) / factorial(n) * kExpMax * kExpMax; }
  template <int T> inline constexpr int exp_terms = terms_for(T, 15, exp_trunc);

  template <int N>
  inline constexpr poly<N> exp_c = series<N>([](int k) { return term{1.0, factorial(k)}; });

  // Relative error of exp_poly at the r it is given (e^r ≥ 1/kExpMax).
  template <int N>
  inline constexpr double exp_poly_rel = [] {
    const horner_bound h = horner_error(exp_c<N>, kRExp, 0);
    return up(h.Err * kExpMax + exp_trunc(N));
  }();

  template <int N>
  [[gnu::always_inline]] inline double exp_poly(double r) { return horner(r, exp_c<N>.C); }

  // e^x = 2^k·e^r, x = k·ln2 + r, for |x| ≤ 745 (|k| ≤ 1100) — larger |x|
  // overflow to infinity or underflow to 0, both within the bound's kTiny
  // or failing every test downstream. r1 = x − k·Hi is exact (Hi has 33
  // bits, k at most 11, and x − k·Hi is a multiple of 2^-54 below ½); the
  // second fma rounds once (kU·|r|, moving e^r by kU·kRExp relatively) and
  // Hi + Lo misses ln 2 by kLn2Res (|k|·kLn2Res). An optional low part of x
  // adds one more rounding of r.
  template <int T>
  struct exp_k
  {
    static constexpr int N = exp_terms<T>;
    static constexpr double Rel = up(exp_poly_rel<N> + kU * kRExp + 1100 * kLn2Res);
    static constexpr double RelLo = up(Rel + kU * kRExp);

    [[gnu::always_inline]] static double value(double x)
    {
      const double k = std::nearbyint(x * kLog2e);
      double r = fma(-k, kLn2Hi, x);
      r = fma(-k, kLn2Lo, r);
      return ::beman::inside::detail::ldexp(exp_poly<N>(r), static_cast<int>(k));
    }
    [[gnu::always_inline]] static double value(double x, double lo)
    {
      const double k = std::nearbyint(x * kLog2e);
      double r = fma(-k, kLn2Hi, x);
      r = fma(-k, kLn2Lo, r) + lo;
      return ::beman::inside::detail::ldexp(exp_poly<N>(r), static_cast<int>(k));
    }
    static double exp(double x, double& bound)
    {
      const double v = value(x);
      bound = Rel * v + kTiny;
      return v;
    }

    // 2^x = 2^k·e^((x − k)·ln 2): x − k is exact (|x| < 2^52), and its
    // product with ln 2 rounds once on top of the constant's 2^-54.5.
    static constexpr double Rel2 = up(exp_poly_rel<N> + (kU + kLn2FullErr) * kRExp);
    static double exp2(double x, double& bound)
    {
      const double k = std::nearbyint(x);
      const double r = (x - k) * kLn2Full;
      const double v = ::beman::inside::detail::ldexp(exp_poly<N>(r), static_cast<int>(k));
      bound = Rel2 * v + kTiny;
      return v;
    }

    // The hyperbolics from e = e^x within Rel: cosh's two positive terms
    // keep it relative (1/e adds one rounding, the sum another); sinh's
    // difference is within (Rel + kU)·cosh x + kU·|sinh x| and cosh ≤
    // |sinh| + 1; tanh = (e^2x − 1)/(e^2x + 1) is within Rel + |t|·(Rel + 3kU).
    static constexpr double SinhRel = up(Rel + 2 * kU), SinhAbs = up(Rel + kU);
    static constexpr double CoshRel = up(Rel + 2 * kU);
    static constexpr double TanhRel = up(Rel + 3 * kU), TanhAbs = up(Rel);

    static double sinh(double x, double& bound)
    {
      const double e = value(x);
      const double v = (e - 1.0 / e) * 0.5;
      bound = SinhRel * __builtin_fabs(v) + SinhAbs + kTiny;
      return v;
    }
    static double cosh(double x, double& bound)
    {
      const double e = value(x);
      const double v = (e + 1.0 / e) * 0.5;
      bound = CoshRel * v + kTiny;
      return v;
    }
    static double tanh(double x, double& bound)
    {
      const double e = value(x + x);
      const double v = (e - 1.0) / (e + 1.0);
      bound = TanhRel * __builtin_fabs(v) + TanhAbs + kTiny;
      return v;
    }
  };

  //---------------------------------------------------------------------------
  // ln x for x > 0: frexp to m ∈ [√½, √2), ln x = e·ln2 + 2·atanh f with
  // f = (m − 1)/(m + 1), |f| ≤ kFMax, and 2·atanh f = 2f·Σ f^2k/(2k+1),
  // whose tail is at most f^2n/((2n+1)(1 − f²)) of the sum (which is ≥ 1).
  //---------------------------------------------------------------------------
  inline constexpr double kFMax = 0.1716;                    // ≥ (√2 − 1)/(√2 + 1) = 0.171573
  inline constexpr double kZLog = kFMax * kFMax;
  constexpr double log_trunc(int n) { return pow_n(kZLog, n) / ((2 * n + 1) * (1 - kZLog)); }
  template <int T> inline constexpr int log_terms = terms_for(T, 11, log_trunc);

  template <int N>
  inline constexpr poly<N> log_c = series<N>([](int k) { return term{1.0, 2.0 * k + 1}; });

  // m − 1 is exact (Sterbenz), m + 1 and the quotient round: f within 2kU,
  // which moves atanh f by 2kU/(1 − f²) of itself. The Horner error and
  // the truncation are relative (the sum is ≥ 1); 2f·p rounds once. Then
  // e·Hi is exact and the two fmas round once each, relative to |v|: for
  // e ≠ 0, |v| ≥ ln2/2 ≥ |2 atanh f|, and |e|·kLn2Res ≤ 3·kLn2Res·|v|;
  // for e = 0 both fmas return 2f·p exactly.
  template <int T>
  struct log_k
  {
    static constexpr int N = log_terms<T>;
    static constexpr double MRel = [] {
      const horner_bound h = horner_error(log_c<N>, kZLog, kU);
      return h.Err + log_trunc(N) + kU + 2 * kU / (1 - kZLog);
    }();
    static constexpr double Rel = up(MRel + 2 * kU + 3 * kLn2Res);
    static constexpr double Rel2 = up(Rel + kU + kLog2eErr);
    static constexpr double Rel10 = up(Rel + kU + kLog10eErr);

    [[gnu::always_inline]] static double value(double x)
    {
      int e;
      double m = ::beman::inside::detail::frexp(x, &e);
      if (m < kSqrtHalf) { m += m; --e; }
      const double f = (m - 1.0) / (m + 1.0);
      const double logm = 2.0 * f * horner(f * f, log_c<N>.C);
      const double r = fma(static_cast<double>(e), kLn2Hi, logm);
      return fma(static_cast<double>(e), kLn2Lo, r);
    }
    static double log(double x, double& bound)
    {
      const double v = value(x);
      bound = Rel * __builtin_fabs(v) + kTiny;
      return v;
    }
    static double log2(double x, double& bound)
    {
      const double v = value(x) * kLog2e;
      bound = Rel2 * __builtin_fabs(v) + kTiny;
      return v;
    }
    static double log10(double x, double& bound)
    {
      const double v = value(x) * kLog10e;
      bound = Rel10 * __builtin_fabs(v) + kTiny;
      return v;
    }

    // asinh |x| ≤ 1: ln(a + √(a² + 1)), whose argument is within 2.5kU
    // (fma, √, sum); above 1: ln a + ln(1 + √(1 + 1/a²)), the second
    // argument within 2kU and the sum one more rounding. Both: within
    // (Rel + kU)·|v| + 2.5kU.
    static constexpr double AhRel = up(Rel + kU), AsinhAbs = up(2.5 * kU);
    static double asinh(double x, double& bound)
    {
      const double a = x < 0 ? -x : x;
      const double m = a <= 1.0 ? value(a + std::sqrt(fma(a, a, 1.0)))
                                : value(a) + value(1.0 + std::sqrt(1.0 + 1.0 / (a * a)));
      bound = AhRel * m + AsinhAbs + kTiny;
      return x < 0 ? -m : m;
    }

    // acosh x ≥ 1: ln x + ln(1 + s), s = √d, d = 1 − 1/x². 1/x² is within
    // 2kU, and d (exact by Sterbenz, else one rounding of at most kU·d) is
    // within 2kU of 1 − 1/x², which moves √d by at most 2kU/s; the root, the
    // sum and the logs add (Rel + kU)·|v| + 2kU. Pre: x ≥ 1 (s = 0 at x = 1
    // makes the bound infinite).
    static double acosh(double x, double& bound)
    {
      const double s = std::sqrt(1.0 - 1.0 / (x * x));
      const double v = value(x) + value(1.0 + s);
      bound = AhRel * v + up(2 * kU) + up(2 * kU * (1 + 0x1p-50)) / s + kTiny;
      return v;
    }

    // atanh |x| < 1: ½·ln((1 + a)/(1 − a)), the quotient within 3kU.
    static constexpr double AtanhAbs = up(1.5 * kU);
    static double atanh(double x, double& bound)
    {
      const double a = x < 0 ? -x : x;
      const double m = 0.5 * value((1.0 + a) / (1.0 - a));
      bound = Rel * m + AtanhAbs + kTiny;
      return x < 0 ? -m : m;
    }
  };

  //---------------------------------------------------------------------------
  // atan: |x| > 1 through π/2 − atan(1/x); then a > tan(π/12) through the
  // π/6 addition formula, so |t| ≤ kTMax; atan t = t·Σ (−1)^k t^2k/(2k+1),
  // alternating with falling terms.
  //---------------------------------------------------------------------------
  inline constexpr double kTMax = 0.2681;                    // ≥ tan(π/12) = 0.267949
  inline constexpr double kZAtan = kTMax * kTMax;
  inline constexpr double kAtanMin = 1 - kZAtan / 3;         // ≤ atan t / t
  constexpr double atan_trunc(int n) { return pow_n(kZAtan, n) / (2 * n + 1) / kAtanMin; }
  template <int T> inline constexpr int atan_terms = terms_for(T, 15, atan_trunc);

  template <int N>
  inline constexpr poly<N> atan_c = series<N>([](int k) { return term{k % 2 ? -1.0 : 1.0, 2.0 * k + 1}; });

  // Unreduced (|x| ≤ tan(π/12), the only case where |v| can be small):
  // the Horner error and truncation over atan t / t, and fma(t, p, 0)'s
  // rounding — relative. Every other path adds absolute errors, in units of
  // kU: t's three roundings (a − c, the fma, the quotient) move atan by
  // 3·kTMax; the constants c and π/6 by 0.23 and 0.5 (atan(c̃) is within
  // |c̃ − c|·¾ of π/6); fma(t, p, π/6) rounds by π/4; 1/a moves atan by ½;
  // π/2 − r by 0.56 (the constant) and π/2 (the rounding); and t·(Horner +
  // truncation) of the polynomial itself.
  template <int T>
  struct atan_k
  {
    static constexpr int N = atan_terms<T>;
    static constexpr double PolyAbs = [] {
      const horner_bound h = horner_error(atan_c<N>, kZAtan, kU);
      return h.Err + atan_trunc(N) * kAtanMin;
    }();
    static constexpr double Rel = up(PolyAbs / kAtanMin + kU);
    static constexpr double Abs = up(kTMax * PolyAbs + 3 * kTMax * kU + kInvSqrt3Err * 0.75 + kPiSixthErr
                                     + kRTrig * kU + 0.5 * kU + kPiHalfErr + 1.5708 * kU);

    [[gnu::always_inline]] static double value(double x)
    {
      const bool neg = x < 0;
      double a = neg ? -x : x;
      const bool inv = a > 1.0;
      if (inv) a = 1.0 / a;
      double off = 0.0;
      if (a > kTanPi12) { a = (a - kInvSqrt3) / fma(a, kInvSqrt3, 1.0); off = kPiSixth; }
      double r = fma(a, horner(a * a, atan_c<N>.C), off);
      if (inv) r = kPiHalf - r;
      return neg ? -r : r;
    }
    static double atan(double x, double& bound)
    {
      const double v = value(x);
      bound = Rel * __builtin_fabs(v) + Abs + kTiny;
      return v;
    }

    // atan2: y/x rounds (½kU on the angle); x < 0 adds ±π (its constant
    // and one rounding).
    static constexpr double Atan2Rel = up(Rel + kU), Atan2Abs = up(Abs + 0.5 * kU + kPiErr);
    static double atan2(double y, double x, double& bound)
    {
      double v;
      if (x > 0.0) v = value(y / x);
      else if (x < 0.0) v = value(y / x) + (y >= 0.0 ? kPi : -kPi);
      else v = y > 0.0 ? kPiHalf : y < 0.0 ? -kPiHalf : 0.0;
      bound = Atan2Rel * __builtin_fabs(v) + Atan2Abs + kTiny;
      return v;
    }

    // asin = atan(x/√((1 − x)(1 + x))): the quotient within 3.5kU, which
    // moves atan by 3.5kU of itself at most. acos = π/2 − asin: absolute.
    static constexpr double AsinRel = up(Rel + 3.5 * kU), AsinAbs = Abs;
    static constexpr double AcosRel = kU, AcosAbs = up(AsinRel * 1.5708 + AsinAbs + kPiHalfErr);
    [[gnu::always_inline]] static double asin_value(double x) { return value(x / std::sqrt((1.0 - x) * (1.0 + x))); }
    static double asin(double x, double& bound)
    {
      const double v = asin_value(x);
      bound = AsinRel * __builtin_fabs(v) + AsinAbs + kTiny;
      return v;
    }
    static double acos(double x, double& bound)
    {
      const double v = kPiHalf - asin_value(x);
      bound = AcosRel * __builtin_fabs(v) + AcosAbs + kTiny;
      return v;
    }
  };

  //---------------------------------------------------------------------------
  // Compositions.
  //---------------------------------------------------------------------------
  // e^(y), y = ln a / 3 (or e·ln b): ln's relative error Rel_log and the
  // product's roundings move y by |y|·(Rel_log + 2kU), and e^y by that much
  // relatively, on top of Rel_exp. T counts the bits of the result; the
  // log's error is multiplied by |y| (below 2^10), so it gets 10 more.
  template <int T>
  struct pow_k
  {
    using L = log_k<T + 10 < kFullBits ? T + 10 : kFullBits>;
    using E = exp_k<T>;
    static constexpr double YRel = up(L::Rel + 2 * kU);

    static double cbrt(double x, double& bound)
    {
      if (x == 0.0) { bound = 0; return 0.0; }
      const double y = L::value(x < 0 ? -x : x) * kThird;
      const double m = E::value(y);
      bound = m * (E::Rel + __builtin_fabs(y) * YRel) + kTiny;
      return x < 0 ? -m : m;
    }

    // b^e for b > 0, |e·ln b| ≤ 745 (else false).
    static bool pow(double b, double e, double& v, double& bound, double& y)
    {
      y = e * L::value(b);
      if (!(__builtin_fabs(y) <= 745)) return false;
      v = E::value(y);
      bound = v * (E::Rel + __builtin_fabs(y) * YRel) + kTiny;
      return true;
    }

    // Base^x with ln Base as a double-double (Hi, Lo within 2^-104 of it
    // relatively): y = x·ln Base as an exact product plus x·Lo, so e^y sees
    // only the low part's extra rounding and 2^-100·|y|.
    static double pow_ln(double x, double lnHi, double lnLo, double& bound, double& y)
    {
      y = x * lnHi;
      const double ylo = fma(x, lnHi, -y) + x * lnLo;
      const double v = E::value(y, ylo);
      bound = v * (E::RelLo + __builtin_fabs(y) * 0x1p-100) + kTiny;
      return v;
    }
  };

  // √ (correctly rounded) and √(x² + y²): the squares and their sum within
  // 2kU, the root halves that and rounds once more. |x|, |y| ≤ 2^500 keep
  // the squares finite; squares below 2^-1022 lose at most 2^-1074, which
  // moves the root by far less than kTiny.
  inline double fp_sqrt(double x) { return std::sqrt(x); }
  inline constexpr double kSqrtRel = up(kU), kHypotRel = up(2 * kU);
  inline double fp_hypot(double x, double y) { return std::sqrt(fma(x, x, y * y)); }

  // Seeds of the dd tier's Newton steps: one step squares a seed's error,
  // so 2^-48 is enough (the dd kernels' test checks the result).
  inline constexpr int kSeedBits = 48;
  inline double seed_log(double x) { return log_k<kSeedBits>::value(x); }
  inline double seed_cbrt(double x)
  {
    if (x == 0.0) return 0.0;
    const double m = exp_k<kSeedBits>::value(log_k<kSeedBits>::value(x < 0 ? -x : x) * kThird);
    return x < 0 ? -m : m;
  }

} // namespace beman::inside::math::detail::fp

#endif // !BEMAN_INSIDE_MATH_NO_FP

#endif // BEMAN_INSIDE_DETAIL_MATH_FP_HPP
