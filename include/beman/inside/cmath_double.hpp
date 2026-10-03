// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
// beman::inside::math double engine — a small, reproducible libm in `double`. Bit-identical
// on every IEEE-754 binary64 platform compiled without `-ffast-math`, via:
//   * NO <cmath> transcendentals — sin/cos/exp/log are fixed polynomials here;
//     only std::fma/sqrt/nearbyint (well-defined) and the constexpr ldexp.
//   * Horner evaluation with explicit std::fma (immune to FMA-contraction).
//   * Cody-Waite range reduction for full-precision args.
// The default engine; `BEMAN_INSIDE_MATH_CORDIC` selects the integer CORDIC engine instead.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_CMATH_DOUBLE_HPP
#define BEMAN_INSIDE_CMATH_DOUBLE_HPP

#include <beman/inside/math.hpp>   // beman::inside::detail::ldexp (constexpr, reproducible)
#include <beman/inside/inside.hpp>  // complete inside/rational + has_flag/policy_of/f64 (store<>)

// BEMAN_INSIDE_MATH_NO_FP is resolved in policy_flag.hpp (included via inside.hpp).

#ifndef BEMAN_INSIDE_MATH_NO_FP   // ===== FP engine present (needs <cmath> + an FPU) =====

#include <cmath>            // std::fma, std::sqrt, std::nearbyint ONLY

// `BEMAN_INSIDE_FP_FN`: the engine cores become `constexpr` on C++26 toolchains with
// constexpr <cmath> (P1383). Inert otherwise — see BEMAN_INSIDE_MATH_FN in cmath.hpp.
#if defined(__cpp_lib_constexpr_cmath) && __cpp_lib_constexpr_cmath >= 202202L
#  define BEMAN_INSIDE_FP_FN constexpr
#else
#  define BEMAN_INSIDE_FP_FN
#endif

namespace beman::inside::math::dbl::detail
{
  using std::fma;

  // c0·z^n + c1·z^(n-1) + … + cn as an fma chain from the highest coefficient
  // down (Horner) — the same operation order as writing the chain out by hand.
  template <std::floating_point T, typename... C>
  [[gnu::always_inline]] inline BEMAN_INSIDE_FP_FN T horner(T z, T c0, C... cs)
  {
    T p = c0;
    ((p = fma(p, z, static_cast<T>(cs))), ...);
    return p;
  }

  inline constexpr double kHalfPiHi = 0x1.921fb54442d18p+0;   // π/2  high
  inline constexpr double kHalfPiLo = 0x1.1a62633145c07p-54;  // π/2  low
  inline constexpr double kTwoOverPi = 0x1.45f306dc9c883p-1;  // 2/π
  inline constexpr double kLn2Hi    = 0x1.62e42fee00000p-1;   // ln2  high
  inline constexpr double kLn2Lo    = 0x1.a39ef35793c76p-33;  // ln2  low
  inline constexpr double kLog2e    = 0x1.71547652b82fep+0;   // 1/ln2

  // sin(r), r ∈ [−π/4, π/4]: r·P(r²), P = Σ (−1)ᵏ zᵏ/(2k+1)! to z⁷ (r¹⁵).
  inline BEMAN_INSIDE_FP_FN double sin_poly(double r)
  {
    double z = r * r;
    double p = horner(z,
                      -1.0 / 1307674368000.0, 1.0 / 6227020800.0, -1.0 / 39916800.0, 1.0 / 362880.0,
                      -1.0 / 5040.0, 1.0 / 120.0, -1.0 / 6.0, 1.0);
    return r * p;
  }

  // cos(r), r ∈ [−π/4, π/4]: Q(r²), Q = Σ (−1)ᵏ zᵏ/(2k)! to z⁸ (r¹⁶).
  inline BEMAN_INSIDE_FP_FN double cos_poly(double r)
  {
    double z = r * r;
    return horner(z,
                  1.0 / 20922789888000.0, -1.0 / 87178291200.0, 1.0 / 479001600.0, -1.0 / 3628800.0,
                  1.0 / 40320.0, -1.0 / 720.0, 1.0 / 24.0, -1.0 / 2.0,
                  1.0);
  }

  // e^r, r ∈ [−ln2/2, ln2/2]: Σ rᵏ/k! to r¹².
  inline BEMAN_INSIDE_FP_FN double exp_poly(double r)
  {
    return horner(r,
                  1.0 / 479001600.0, 1.0 / 39916800.0, 1.0 / 3628800.0, 1.0 / 362880.0,
                  1.0 / 40320.0, 1.0 / 5040.0, 1.0 / 720.0, 1.0 / 120.0,
                  1.0 / 24.0, 1.0 / 6.0, 1.0 / 2.0, 1.0,
                  1.0);
  }

  // Shared quadrant reduction: x → (r ∈ [−π/4,π/4], q = quadrant mod 4).
  inline BEMAN_INSIDE_FP_FN double reduce_quadrant(double x, long& q)
  {
    double k = std::nearbyint(x * kTwoOverPi);
    double r = fma(-k, kHalfPiHi, x);
    r = fma(-k, kHalfPiLo, r);
    q = static_cast<long>(k) & 3;
    return r;
  }

  inline BEMAN_INSIDE_FP_FN double fp_sin(double x)
  {
    long q; double r = reduce_quadrant(x, q);
    switch (q) {
      case 0:  return sin_poly(r);
      case 1:  return cos_poly(r);
      case 2:  return -sin_poly(r);
      default: return -cos_poly(r);
    }
  }

  inline BEMAN_INSIDE_FP_FN double fp_cos(double x)
  {
    long q; double r = reduce_quadrant(x, q);
    switch (q) {
      case 0:  return cos_poly(r);
      case 1:  return -sin_poly(r);
      case 2:  return -cos_poly(r);
      default: return sin_poly(r);
    }
  }

  // tan from one reduction: s/c in even quadrants, −c/s in odd ones. False on
  // a pole (odd quadrant with s == 0).
  inline BEMAN_INSIDE_FP_FN bool fp_tan(double x, double& t)
  {
    long q; double r = reduce_quadrant(x, q);
    const double s = sin_poly(r), c = cos_poly(r);
    if (q & 1)
    {
      if (s == 0.0) return false;
      t = -c / s;
    }
    else
      t = s / c;
    return true;
  }

  // e^x = 2^k · e^r, x = k·ln2 + r, r ∈ [−ln2/2, ln2/2].
  inline BEMAN_INSIDE_FP_FN double fp_exp(double x)
  {
    double k = std::nearbyint(x * kLog2e);
    double r = fma(-k, kLn2Hi, x);
    r = fma(-k, kLn2Lo, r);
    return beman::inside::detail::ldexp(exp_poly(r), static_cast<int>(k));
  }

  inline BEMAN_INSIDE_FP_FN double fp_sqrt(double x) { return std::sqrt(x); }   // correctly rounded

  inline constexpr double kSqrtHalf = 0x1.6a09e667f3bcdp-1; // √½

  // ln(x): frexp to m∈[½,1), rebalance to [√½,√2); ln(x) = e·ln2 + 2·atanh(f),
  // f = (m−1)/(m+1) ∈ [−0.18,0.18] (atanh series converges fast). Pre: x > 0.
  inline BEMAN_INSIDE_FP_FN double fp_log(double x)
  {
    int e;
    double m = beman::inside::detail::frexp(x, &e);
    if (m < kSqrtHalf) { m += m; --e; }
    double f  = (m - 1.0) / (m + 1.0);
    double f2 = f * f;
    double p = horner(f2,
                      1.0 / 17.0, 1.0 / 15.0, 1.0 / 13.0, 1.0 / 11.0,
                      1.0 / 9.0, 1.0 / 7.0, 1.0 / 5.0, 1.0 / 3.0,
                      1.0);
    double logm = 2.0 * f * p;
    double r = fma(static_cast<double>(e), kLn2Hi, logm);
    return fma(static_cast<double>(e), kLn2Lo, r);
  }

  inline constexpr double kLn2Full  = 0x1.62e42fefa39efp-1;  // ln2
  inline constexpr double kLog10e   = 0x1.bcb7b1526e50ep-2;  // 1/ln10

  // Compositions on the validated primitives.
  inline BEMAN_INSIDE_FP_FN double fp_exp2(double x)  { return fp_exp(x * kLn2Full); }
  inline BEMAN_INSIDE_FP_FN double fp_log2(double x)  { return fp_log(x) * kLog2e; }
  inline BEMAN_INSIDE_FP_FN double fp_log10(double x) { return fp_log(x) * kLog10e; }
  inline BEMAN_INSIDE_FP_FN double fp_pow(double b, double e) { return fp_exp(e * fp_log(b)); }
  inline BEMAN_INSIDE_FP_FN double fp_cbrt(double x)
  {
    if (x == 0.0) return 0.0;
    double m = fp_exp(fp_log(x < 0 ? -x : x) * (1.0 / 3.0));
    return x < 0 ? -m : m;
  }
  inline BEMAN_INSIDE_FP_FN double fp_sinh(double x) { double e = fp_exp(x); return (e - 1.0 / e) * 0.5; }
  inline BEMAN_INSIDE_FP_FN double fp_cosh(double x) { double e = fp_exp(x); return (e + 1.0 / e) * 0.5; }
  inline BEMAN_INSIDE_FP_FN double fp_tanh(double x)
  {
    double e = fp_exp(x + x);            // e^{2x}
    return (e - 1.0) / (e + 1.0);
  }
  // √(x²+y²). The public domain caps |x|,|y| ≤ 2^20, so x²+y² ≤ 2^41 — no
  // overflow, no scaling needed; the correctly-rounded √ keeps it accurate.
  inline BEMAN_INSIDE_FP_FN double fp_hypot(double x, double y) { return fp_sqrt(x * x + y * y); }

  inline constexpr double kPi      = 0x1.921fb54442d18p+1;   // π
  inline constexpr double kPiHalf  = 0x1.921fb54442d18p+0;   // π/2
  inline constexpr double kPiSixth = 0x1.0c152382d7366p-1;   // π/6
  inline constexpr double kInvSqrt3 = 0x1.279a74590331cp-1;  // 1/√3 = tan(π/6)
  inline constexpr double kTanPi12 = 0x1.126145e9ecd56p-2;   // tan(π/12) ≈ 0.2679

  // atan(x). Reduce |x|>1 via reciprocal (π/2 − atan(1/x)); then |a|>tan(π/12)
  // via the π/6 addition formula → |t| ≤ tan(π/12); atan(t) = t·P(t²) Taylor.
  inline BEMAN_INSIDE_FP_FN double fp_atan(double x)
  {
    bool neg = x < 0; double a = neg ? -x : x;
    bool inv = a > 1.0; if (inv) a = 1.0 / a;
    double off = 0.0;
    if (a > kTanPi12) { a = (a - kInvSqrt3) / fma(a, kInvSqrt3, 1.0); off = kPiSixth; }
    double z = a * a;
    double p = horner(z,
                      -1.0 / 23.0, 1.0 / 21.0, -1.0 / 19.0, 1.0 / 17.0,
                      -1.0 / 15.0, 1.0 / 13.0, -1.0 / 11.0, 1.0 / 9.0,
                      -1.0 / 7.0, 1.0 / 5.0, -1.0 / 3.0, 1.0);
    double r = off + a * p;
    if (inv) r = kPiHalf - r;
    return neg ? -r : r;
  }

  inline BEMAN_INSIDE_FP_FN double fp_atan2(double y, double x)
  {
    if (x > 0.0) return fp_atan(y / x);
    if (x < 0.0) return fp_atan(y / x) + (y >= 0.0 ? kPi : -kPi);
    if (y > 0.0) return kPiHalf;
    if (y < 0.0) return -kPiHalf;
    return 0.0;
  }

  inline BEMAN_INSIDE_FP_FN double fp_asin(double x) { return fp_atan(x / fp_sqrt((1.0 - x) * (1.0 + x))); }
  inline BEMAN_INSIDE_FP_FN double fp_acos(double x) { return kPiHalf - fp_asin(x); }
} // namespace beman::inside::math::dbl::detail

namespace beman::inside::math::dbl::detail
{
  // Engine cores: `f64` (double-backed) inside in → `double` math → inside out.
  // The inside I/O is a plain double read/store (operator double / Out{double}),
  // so the cost is the polynomial itself. These plug into the shared public
  // surface as `fn_core` under the default build.
  template <typename Out>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out store(double d)
  {
    // An fp-backed Out (f64 or f32) stores the value directly via its raw (an f32
    // Out narrows double→float, lossless on its float-exact grid); a non-fp snap
    // grid assigns through the rational path, snapping via Out's round policy.
    if constexpr (beman::inside::detail::fp_raw<Out>) return Out{d};
    else { Out o{}; o = beman::inside::detail::rational{d}; return o; }
  }

  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out sin_core(In x)  { return store<Out>(detail::fp_sin(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out cos_core(In x)  { return store<Out>(detail::fp_cos(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out exp_core(In x)  { return store<Out>(detail::fp_exp(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out sqrt_core(In x) { return store<Out>(detail::fp_sqrt(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out log_core(In x)  { return store<Out>(detail::fp_log(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out exp2_core(In x) { return store<Out>(detail::fp_exp2(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out log2_core(In x) { return store<Out>(detail::fp_log2(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out log10_core(In x){ return store<Out>(detail::fp_log10(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out cbrt_core(In x) { return store<Out>(detail::fp_cbrt(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out sinh_core(In x) { return store<Out>(detail::fp_sinh(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out cosh_core(In x) { return store<Out>(detail::fp_cosh(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out tanh_core(In x) { return store<Out>(detail::fp_tanh(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out atan_core(In x) { return store<Out>(detail::fp_atan(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out asin_core(In x) { return store<Out>(detail::fp_asin(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out acos_core(In x) { return store<Out>(detail::fp_acos(static_cast<double>(x))); }
  template <typename Out, typename InY, typename InX>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out atan2_core(InY y, InX x)
  { return store<Out>(detail::fp_atan2(static_cast<double>(y), static_cast<double>(x))); }
  template <typename Out, typename InX, typename InY>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out hypot_core(InX x, InY y)
  { return store<Out>(detail::fp_hypot(static_cast<double>(x), static_cast<double>(y))); }
} // namespace beman::inside::math::dbl

#endif // !BEMAN_INSIDE_MATH_NO_FP

#endif // BEMAN_INSIDE_CMATH_DOUBLE_HPP
