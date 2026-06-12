//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
// bnd::math double engine — a small, reproducible libm in `double`.
//
//   REPRODUCIBILITY CONTRACT
//   ------------------------
//   Bit-identical on every IEEE-754 binary64 platform compiled WITHOUT
//   `-ffast-math` (round-to-nearest-even). Reproducibility is achieved by:
//     * NO `<cmath>` transcendentals — sin/cos/exp/log are implemented here as
//       fixed polynomials; only `std::fma`, `std::sqrt`, `std::nearbyint` are
//       used (IEEE-754 correctly-rounded / well-defined). The `2^k` scaling
//       uses the library's own constexpr `bnd::detail::ldexp` (bit assembly,
//       no libm call).
//     * Polynomials evaluated Horner with explicit `std::fma` — immune to
//       compiler FMA-contraction differences.
//     * Cody-Waite range reduction (split constants) for full-precision args.
//
//   This is the default math engine; `BND_MATH_FIXED` selects the integer
//   CORDIC engine (the cores in `cmath.hpp`) instead.
//---------------------------------------------------------------------------
#ifndef BNDcmathdoubleHPP
#define BNDcmathdoubleHPP

#include "bound/math.hpp"   // bnd::detail::ldexp (constexpr, reproducible)

#include <cmath>            // std::fma, std::sqrt, std::nearbyint ONLY

// `BND_DBL_FN`: the engine cores become `constexpr` on C++26 toolchains with
// constexpr <cmath> (P1383). Inert otherwise — see BND_MATH_FN in cmath.hpp.
#if defined(__cpp_lib_constexpr_cmath) && __cpp_lib_constexpr_cmath >= 202202L
#  define BND_DBL_FN constexpr
#else
#  define BND_DBL_FN
#endif

namespace bnd::math::dbl::detail
{
  using std::fma;

  inline constexpr double kHalfPiHi = 0x1.921fb54442d18p+0;   // π/2  high
  inline constexpr double kHalfPiLo = 0x1.1a62633145c07p-54;  // π/2  low
  inline constexpr double kTwoOverPi = 0x1.45f306dc9c883p-1;  // 2/π
  inline constexpr double kLn2Hi    = 0x1.62e42fee00000p-1;   // ln2  high
  inline constexpr double kLn2Lo    = 0x1.a39ef35793c76p-33;  // ln2  low
  inline constexpr double kLog2e    = 0x1.71547652b82fep+0;   // 1/ln2

  // sin(r), r ∈ [−π/4, π/4]: r·P(r²), P = Σ (−1)ᵏ zᵏ/(2k+1)! to z⁷ (r¹⁵).
  inline BND_DBL_FN double sin_poly(double r)
  {
    double z = r * r;
    double p = -1.0 / 1307674368000.0;             // −1/15!
    p = fma(p, z,  1.0 / 6227020800.0);            //  1/13!
    p = fma(p, z, -1.0 / 39916800.0);              // −1/11!
    p = fma(p, z,  1.0 / 362880.0);                //  1/9!
    p = fma(p, z, -1.0 / 5040.0);                  // −1/7!
    p = fma(p, z,  1.0 / 120.0);                   //  1/5!
    p = fma(p, z, -1.0 / 6.0);                     // −1/3!
    p = fma(p, z,  1.0);                           //  1/1!
    return r * p;
  }

  // cos(r), r ∈ [−π/4, π/4]: Q(r²), Q = Σ (−1)ᵏ zᵏ/(2k)! to z⁸ (r¹⁶).
  inline BND_DBL_FN double cos_poly(double r)
  {
    double z = r * r;
    double p =  1.0 / 20922789888000.0;            //  1/16!
    p = fma(p, z, -1.0 / 87178291200.0);           // −1/14!
    p = fma(p, z,  1.0 / 479001600.0);             //  1/12!
    p = fma(p, z, -1.0 / 3628800.0);               // −1/10!
    p = fma(p, z,  1.0 / 40320.0);                 //  1/8!
    p = fma(p, z, -1.0 / 720.0);                   // −1/6!
    p = fma(p, z,  1.0 / 24.0);                    //  1/4!
    p = fma(p, z, -1.0 / 2.0);                     // −1/2!
    p = fma(p, z,  1.0);                           //  1
    return p;
  }

  // e^r, r ∈ [−ln2/2, ln2/2]: Σ rᵏ/k! to r¹².
  inline BND_DBL_FN double exp_poly(double r)
  {
    double p = 1.0 / 479001600.0;                  // 1/12!
    p = fma(p, r, 1.0 / 39916800.0);               // 1/11!
    p = fma(p, r, 1.0 / 3628800.0);                // 1/10!
    p = fma(p, r, 1.0 / 362880.0);                 // 1/9!
    p = fma(p, r, 1.0 / 40320.0);                  // 1/8!
    p = fma(p, r, 1.0 / 5040.0);                   // 1/7!
    p = fma(p, r, 1.0 / 720.0);                    // 1/6!
    p = fma(p, r, 1.0 / 120.0);                    // 1/5!
    p = fma(p, r, 1.0 / 24.0);                     // 1/4!
    p = fma(p, r, 1.0 / 6.0);                      // 1/3!
    p = fma(p, r, 1.0 / 2.0);                      // 1/2!
    p = fma(p, r, 1.0);                            // 1/1!
    p = fma(p, r, 1.0);                            // 1
    return p;
  }

  inline BND_DBL_FN double d_sin(double x)
  {
    double k = std::nearbyint(x * kTwoOverPi);
    double r = fma(-k, kHalfPiHi, x);
    r = fma(-k, kHalfPiLo, r);
    long q = static_cast<long>(k) & 3;
    switch (q) {
      case 0:  return sin_poly(r);
      case 1:  return cos_poly(r);
      case 2:  return -sin_poly(r);
      default: return -cos_poly(r);
    }
  }

  inline BND_DBL_FN double d_cos(double x) { return d_sin(x + (kHalfPiHi + kHalfPiLo)); }

  // e^x = 2^k · e^r, x = k·ln2 + r, r ∈ [−ln2/2, ln2/2].
  inline BND_DBL_FN double d_exp(double x)
  {
    double k = std::nearbyint(x * kLog2e);
    double r = fma(-k, kLn2Hi, x);
    r = fma(-k, kLn2Lo, r);
    return bnd::detail::ldexp(exp_poly(r), static_cast<int>(k));
  }

  inline BND_DBL_FN double d_sqrt(double x) { return std::sqrt(x); }   // correctly rounded

  inline constexpr double kSqrtHalf = 0x1.6a09e667f3bcdp-1; // √½

  // ln(x): frexp to m∈[½,1), rebalance to [√½,√2); ln(x) = e·ln2 + 2·atanh(f),
  // f = (m−1)/(m+1) ∈ [−0.18,0.18] (atanh series converges fast). Pre: x > 0.
  inline BND_DBL_FN double d_log(double x)
  {
    int e;
    double m = bnd::detail::frexp(x, &e);
    if (m < kSqrtHalf) { m += m; --e; }
    double f  = (m - 1.0) / (m + 1.0);
    double f2 = f * f;
    double p = 1.0 / 17.0;
    p = fma(p, f2, 1.0 / 15.0);
    p = fma(p, f2, 1.0 / 13.0);
    p = fma(p, f2, 1.0 / 11.0);
    p = fma(p, f2, 1.0 / 9.0);
    p = fma(p, f2, 1.0 / 7.0);
    p = fma(p, f2, 1.0 / 5.0);
    p = fma(p, f2, 1.0 / 3.0);
    p = fma(p, f2, 1.0);
    double logm = 2.0 * f * p;
    double r = fma(static_cast<double>(e), kLn2Hi, logm);
    return fma(static_cast<double>(e), kLn2Lo, r);
  }

  inline constexpr double kLn2Full  = 0x1.62e42fefa39efp-1;  // ln2
  inline constexpr double kLog10e   = 0x1.bcb7b1526e50ep-2;  // 1/ln10

  // Compositions on the validated primitives.
  inline BND_DBL_FN double d_tan(double x)   { return d_sin(x) / d_cos(x); }
  inline BND_DBL_FN double d_exp2(double x)  { return d_exp(x * kLn2Full); }
  inline BND_DBL_FN double d_log2(double x)  { return d_log(x) * kLog2e; }
  inline BND_DBL_FN double d_log10(double x) { return d_log(x) * kLog10e; }
  inline BND_DBL_FN double d_pow(double b, double e) { return d_exp(e * d_log(b)); }
  inline BND_DBL_FN double d_cbrt(double x)
  {
    if (x == 0.0) return 0.0;
    double m = d_exp(d_log(x < 0 ? -x : x) * (1.0 / 3.0));
    return x < 0 ? -m : m;
  }
  inline BND_DBL_FN double d_sinh(double x) { double e = d_exp(x); return (e - 1.0 / e) * 0.5; }
  inline BND_DBL_FN double d_cosh(double x) { double e = d_exp(x); return (e + 1.0 / e) * 0.5; }
  inline BND_DBL_FN double d_tanh(double x)
  {
    double e = d_exp(x + x);            // e^{2x}
    return (e - 1.0) / (e + 1.0);
  }
  // √(x²+y²). The public domain caps |x|,|y| ≤ 2^20, so x²+y² ≤ 2^41 — no
  // overflow, no scaling needed; the correctly-rounded √ keeps it accurate.
  inline BND_DBL_FN double d_hypot(double x, double y) { return d_sqrt(x * x + y * y); }

  inline constexpr double kPi      = 0x1.921fb54442d18p+1;   // π
  inline constexpr double kPiHalf  = 0x1.921fb54442d18p+0;   // π/2
  inline constexpr double kPiSixth = 0x1.0c152382d7366p-1;   // π/6
  inline constexpr double kInvSqrt3 = 0x1.279a74590331cp-1;  // 1/√3 = tan(π/6)
  inline constexpr double kTanPi12 = 0x1.126145e9ecd56p-2;   // tan(π/12) ≈ 0.2679

  // atan(x). Reduce |x|>1 via reciprocal (π/2 − atan(1/x)); then |a|>tan(π/12)
  // via the π/6 addition formula → |t| ≤ tan(π/12); atan(t) = t·P(t²) Taylor.
  inline BND_DBL_FN double d_atan(double x)
  {
    bool neg = x < 0; double a = neg ? -x : x;
    bool inv = a > 1.0; if (inv) a = 1.0 / a;
    double off = 0.0;
    if (a > kTanPi12) { a = (a - kInvSqrt3) / fma(a, kInvSqrt3, 1.0); off = kPiSixth; }
    double z = a * a;
    double p = -1.0 / 23.0;
    p = fma(p, z,  1.0 / 21.0); p = fma(p, z, -1.0 / 19.0); p = fma(p, z,  1.0 / 17.0);
    p = fma(p, z, -1.0 / 15.0); p = fma(p, z,  1.0 / 13.0); p = fma(p, z, -1.0 / 11.0);
    p = fma(p, z,  1.0 / 9.0);  p = fma(p, z, -1.0 / 7.0);  p = fma(p, z,  1.0 / 5.0);
    p = fma(p, z, -1.0 / 3.0);  p = fma(p, z,  1.0);
    double r = off + a * p;
    if (inv) r = kPiHalf - r;
    return neg ? -r : r;
  }

  inline BND_DBL_FN double d_atan2(double y, double x)
  {
    if (x > 0.0) return d_atan(y / x);
    if (x < 0.0) return d_atan(y / x) + (y >= 0.0 ? kPi : -kPi);
    if (y > 0.0) return kPiHalf;
    if (y < 0.0) return -kPiHalf;
    return 0.0;
  }

  inline BND_DBL_FN double d_asin(double x) { return d_atan(x / d_sqrt((1.0 - x) * (1.0 + x))); }
  inline BND_DBL_FN double d_acos(double x) { return kPiHalf - d_asin(x); }
} // namespace bnd::math::dbl::detail

namespace bnd::math::dbl
{
  // Engine cores: `real` (double-backed) bound in → `double` math → bound out.
  // The bound I/O is a plain double read/store (operator double / Out{double}),
  // so the cost is the polynomial itself. These plug into the shared public
  // surface as `fn_core` under the default build.
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out sin_core(In x)  { return Out{detail::d_sin (x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out cos_core(In x)  { return Out{detail::d_cos (x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out exp_core(In x)  { return Out{detail::d_exp (x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out sqrt_core(In x) { return Out{detail::d_sqrt(x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out log_core(In x)  { return Out{detail::d_log (x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out tan_core(In x)  { return Out{detail::d_tan (x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out exp2_core(In x) { return Out{detail::d_exp2(x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out log2_core(In x) { return Out{detail::d_log2(x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out log10_core(In x){ return Out{detail::d_log10(x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out cbrt_core(In x) { return Out{detail::d_cbrt(x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out sinh_core(In x) { return Out{detail::d_sinh(x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out cosh_core(In x) { return Out{detail::d_cosh(x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out tanh_core(In x) { return Out{detail::d_tanh(x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out atan_core(In x) { return Out{detail::d_atan(x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out asin_core(In x) { return Out{detail::d_asin(x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out acos_core(In x) { return Out{detail::d_acos(x)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out pow_core(In b, In e)
  { return Out{detail::d_pow(b, e)}; }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out atan2_core(In y, In x)
  { return Out{detail::d_atan2(y, x)}; }
  template <typename Out, typename InX, typename InY>
  [[nodiscard]] BND_DBL_FN Out hypot_core(InX x, InY y)
  { return Out{detail::d_hypot(x, y)}; }
} // namespace bnd::math::dbl

#endif // BNDcmathdoubleHPP
