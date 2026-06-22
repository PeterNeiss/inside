//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
// bnd::math float engine — a small, reproducible libm in `float` (binary32).
// Bit-identical on every IEEE-754 binary32 platform compiled without
// `-ffast-math`, via the same recipe as the double engine but in single
// precision, for single-precision-only FPUs (Cortex-M4F etc.) and size/speed:
//   * NO <cmath> transcendentals — own fixed polynomials evaluated with the
//     correctly-rounded std::fma(float) (immune to FMA-contraction).
//   * Cody-Waite range reduction whose hi/lo split constants are derived at
//     compile time (mask the low mantissa bits of the float-rounded constant),
//     no external codegen — honoring the bit-exact contract.
//   * Correctly-rounded std::sqrt(float).
// Float is a THIRD value set: float ≠ double ≠ cordic, each ≤ a few notches of
// truth where the grid permits (table-maker's dilemma — see determinism.md).
// Present only when FP is available; compiled out under BND_MATH_NO_FP.
//---------------------------------------------------------------------------
#ifndef BNDcmathfloatHPP
#define BNDcmathfloatHPP

#include "bound/cmath_double.hpp"  // resolves BND_MATH_NO_FP; BND_DBL_FN; store base

#ifndef BND_MATH_NO_FP

#include <bit>     // std::bit_cast (constexpr Cody-Waite split derivation)
#include <cmath>   // std::fma, std::sqrt, std::nearbyint, std::ldexp, std::frexp (float)

namespace bnd::math::flt::detail
{
  using std::fma;

  // Constexpr Cody-Waite split of a high-precision (double) reference into a
  // float `hi` with `keep` significant mantissa bits (low bits zeroed, so k·hi
  // stays exact for modest k) plus a float `lo` carrying the remainder. Pure
  // bit manipulation — identical on every IEEE-754 target.
  inline constexpr float split_hi(double full, int keep) noexcept
  {
    float f = static_cast<float>(full);
    std::uint32_t b = std::bit_cast<std::uint32_t>(f);
    b &= ~((std::uint32_t{1} << (23 - keep)) - 1);
    return std::bit_cast<float>(b);
  }
  inline constexpr float split_lo(double full, int keep) noexcept
  {
    return static_cast<float>(full - static_cast<double>(split_hi(full, keep)));
  }

  // High-precision references (double literals; only their float projections are
  // used at runtime). 12 kept bits hold accuracy across the shared ±2^20 domain.
  inline constexpr double kPiD    = 3.14159265358979323846;
  inline constexpr double kPiO2D  = kPiD / 2;
  inline constexpr double kLn2D   = 0.69314718055994530942;

  inline constexpr float kPio2Hi    = split_hi(kPiO2D, 12);
  inline constexpr float kPio2Lo    = split_lo(kPiO2D, 12);
  inline constexpr float kTwoOverPi  = static_cast<float>(2.0 / kPiD);
  inline constexpr float kLn2Hi     = split_hi(kLn2D, 12);
  inline constexpr float kLn2Lo     = split_lo(kLn2D, 12);
  inline constexpr float kLn2Full   = static_cast<float>(kLn2D);
  inline constexpr float kLog2e     = static_cast<float>(1.44269504088896340736);
  inline constexpr float kLog10e    = static_cast<float>(0.43429448190325182765);
  inline constexpr float kSqrtHalf  = 0x1.6a09e6p-1f;             // √½
  inline constexpr float kPi        = static_cast<float>(kPiD);
  inline constexpr float kPiHalf    = static_cast<float>(kPiO2D);
  inline constexpr float kPiSixth   = static_cast<float>(kPiD / 6);
  inline constexpr float kInvSqrt3  = 0x1.279a74p-1f;             // 1/√3 = tan(π/6)
  inline constexpr float kTanPi12   = 0x1.126146p-2f;             // tan(π/12)

  // sin(r), r ∈ [−π/4, π/4]: r·P(r²) to r¹¹ (float-sufficient).
  inline BND_DBL_FN float sin_poly(float r)
  {
    float z = r * r;
    float p = -1.0f / 39916800.0f;       // −1/11!
    p = fma(p, z,  1.0f / 362880.0f);    //  1/9!
    p = fma(p, z, -1.0f / 5040.0f);      // −1/7!
    p = fma(p, z,  1.0f / 120.0f);       //  1/5!
    p = fma(p, z, -1.0f / 6.0f);         // −1/3!
    p = fma(p, z,  1.0f);                //  1
    return r * p;
  }

  // cos(r), r ∈ [−π/4, π/4]: Q(r²) to r¹⁰.
  inline BND_DBL_FN float cos_poly(float r)
  {
    float z = r * r;
    float p = -1.0f / 3628800.0f;        // −1/10!
    p = fma(p, z,  1.0f / 40320.0f);     //  1/8!
    p = fma(p, z, -1.0f / 720.0f);       // −1/6!
    p = fma(p, z,  1.0f / 24.0f);        //  1/4!
    p = fma(p, z, -1.0f / 2.0f);         // −1/2!
    p = fma(p, z,  1.0f);                //  1
    return p;
  }

  // e^r, r ∈ [−ln2/2, ln2/2]: Σ rᵏ/k! to r⁷.
  inline BND_DBL_FN float exp_poly(float r)
  {
    float p = 1.0f / 5040.0f;            // 1/7!
    p = fma(p, r, 1.0f / 720.0f);        // 1/6!
    p = fma(p, r, 1.0f / 120.0f);        // 1/5!
    p = fma(p, r, 1.0f / 24.0f);         // 1/4!
    p = fma(p, r, 1.0f / 6.0f);          // 1/3!
    p = fma(p, r, 1.0f / 2.0f);          // 1/2!
    p = fma(p, r, 1.0f);                 // 1/1!
    p = fma(p, r, 1.0f);                 // 1
    return p;
  }

  // Shared quadrant reduction: x → (r ∈ [−π/4,π/4], q = quadrant mod 4).
  inline BND_DBL_FN float reduce_quadrant(float x, long& q)
  {
    float k = std::nearbyint(x * kTwoOverPi);
    float r = fma(-k, kPio2Hi, x);
    r = fma(-k, kPio2Lo, r);
    q = static_cast<long>(k) & 3;
    return r;
  }

  inline BND_DBL_FN float d_sin(float x)
  {
    long q; float r = reduce_quadrant(x, q);
    switch (q) {
      case 0:  return sin_poly(r);
      case 1:  return cos_poly(r);
      case 2:  return -sin_poly(r);
      default: return -cos_poly(r);
    }
  }

  // cos via quadrant reduction (NOT sin(x+π/2): shifting the float input would
  // lose bits for large x — the double engine can afford that, float cannot).
  inline BND_DBL_FN float d_cos(float x)
  {
    long q; float r = reduce_quadrant(x, q);
    switch (q) {
      case 0:  return cos_poly(r);
      case 1:  return -sin_poly(r);
      case 2:  return -cos_poly(r);
      default: return sin_poly(r);
    }
  }

  // e^x = 2^k · e^r, x = k·ln2 + r, r ∈ [−ln2/2, ln2/2].
  inline BND_DBL_FN float d_exp(float x)
  {
    float k = std::nearbyint(x * kLog2e);
    float r = fma(-k, kLn2Hi, x);
    r = fma(-k, kLn2Lo, r);
    return std::ldexp(exp_poly(r), static_cast<int>(k));
  }

  inline BND_DBL_FN float d_sqrt(float x) { return std::sqrt(x); }   // correctly rounded

  // ln(x): frexp to m∈[½,1), rebalance to [√½,√2); ln = e·ln2 + 2·atanh(f),
  // f = (m−1)/(m+1). Pre: x > 0.
  inline BND_DBL_FN float d_log(float x)
  {
    int e;
    float m = std::frexp(x, &e);
    if (m < kSqrtHalf) { m += m; --e; }
    float f  = (m - 1.0f) / (m + 1.0f);
    float f2 = f * f;
    float p = 1.0f / 9.0f;
    p = fma(p, f2, 1.0f / 7.0f);
    p = fma(p, f2, 1.0f / 5.0f);
    p = fma(p, f2, 1.0f / 3.0f);
    p = fma(p, f2, 1.0f);
    float logm = 2.0f * f * p;
    float r = fma(static_cast<float>(e), kLn2Hi, logm);
    return fma(static_cast<float>(e), kLn2Lo, r);
  }

  // Compositions on the validated primitives (same shapes as the double engine).
  inline BND_DBL_FN float d_exp2(float x)  { return d_exp(x * kLn2Full); }
  inline BND_DBL_FN float d_log2(float x)  { return d_log(x) * kLog2e; }
  inline BND_DBL_FN float d_log10(float x) { return d_log(x) * kLog10e; }
  inline BND_DBL_FN float d_pow(float b, float e) { return d_exp(e * d_log(b)); }
  inline BND_DBL_FN float d_cbrt(float x)
  {
    if (x == 0.0f) return 0.0f;
    float m = d_exp(d_log(x < 0 ? -x : x) * (1.0f / 3.0f));
    return x < 0 ? -m : m;
  }
  inline BND_DBL_FN float d_sinh(float x) { float e = d_exp(x); return (e - 1.0f / e) * 0.5f; }
  inline BND_DBL_FN float d_cosh(float x) { float e = d_exp(x); return (e + 1.0f / e) * 0.5f; }
  inline BND_DBL_FN float d_tanh(float x)
  {
    float e = d_exp(x + x);             // e^{2x}
    return (e - 1.0f) / (e + 1.0f);
  }
  inline BND_DBL_FN float d_hypot(float x, float y) { return d_sqrt(x * x + y * y); }

  // atan(x): reduce |x|>1 via reciprocal; |a|>tan(π/12) via the π/6 addition
  // formula → |t| ≤ tan(π/12); atan(t) = t·P(t²) Taylor.
  inline BND_DBL_FN float d_atan(float x)
  {
    bool neg = x < 0; float a = neg ? -x : x;
    bool inv = a > 1.0f; if (inv) a = 1.0f / a;
    float off = 0.0f;
    if (a > kTanPi12) { a = (a - kInvSqrt3) / fma(a, kInvSqrt3, 1.0f); off = kPiSixth; }
    float z = a * a;
    float p = 1.0f / 13.0f;
    p = fma(p, z, -1.0f / 11.0f); p = fma(p, z,  1.0f / 9.0f); p = fma(p, z, -1.0f / 7.0f);
    p = fma(p, z,  1.0f / 5.0f);  p = fma(p, z, -1.0f / 3.0f); p = fma(p, z,  1.0f);
    float r = off + a * p;
    if (inv) r = kPiHalf - r;
    return neg ? -r : r;
  }

  inline BND_DBL_FN float d_atan2(float y, float x)
  {
    if (x > 0.0f) return d_atan(y / x);
    if (x < 0.0f) return d_atan(y / x) + (y >= 0.0f ? kPi : -kPi);
    if (y > 0.0f) return kPiHalf;
    if (y < 0.0f) return -kPiHalf;
    return 0.0f;
  }

  inline BND_DBL_FN float d_asin(float x) { return d_atan(x / d_sqrt((1.0f - x) * (1.0f + x))); }
  inline BND_DBL_FN float d_acos(float x) { return kPiHalf - d_asin(x); }
} // namespace bnd::math::flt::detail

namespace bnd::math::flt
{
  // Engine cores: bound in → `float` math → bound out. Storing the float result:
  // a `real` (double-backed) bound takes Out{double(f)} (widening is exact); a
  // non-`real` snap grid assigns through the rational path, snapping via Out's
  // round policy. (An f32-backed bound — Phase 4b — will store the float raw
  // directly; until then it routes through the same paths.)
  template <typename Out>
  [[nodiscard]] BND_DBL_FN Out store(float f)
  {
    if constexpr (has_flag(BoundPolicy<Out>, real)) return Out{static_cast<double>(f)};
    else { Out o{}; o = bnd::detail::rational{static_cast<double>(f)}; return o; }
  }

  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out sin_core(In x)  { return store<Out>(detail::d_sin(static_cast<float>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out cos_core(In x)  { return store<Out>(detail::d_cos(static_cast<float>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out exp_core(In x)  { return store<Out>(detail::d_exp(static_cast<float>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out sqrt_core(In x) { return store<Out>(detail::d_sqrt(static_cast<float>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out log_core(In x)  { return store<Out>(detail::d_log(static_cast<float>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out exp2_core(In x) { return store<Out>(detail::d_exp2(static_cast<float>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out log2_core(In x) { return store<Out>(detail::d_log2(static_cast<float>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out log10_core(In x){ return store<Out>(detail::d_log10(static_cast<float>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out cbrt_core(In x) { return store<Out>(detail::d_cbrt(static_cast<float>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out sinh_core(In x) { return store<Out>(detail::d_sinh(static_cast<float>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out cosh_core(In x) { return store<Out>(detail::d_cosh(static_cast<float>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out tanh_core(In x) { return store<Out>(detail::d_tanh(static_cast<float>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out atan_core(In x) { return store<Out>(detail::d_atan(static_cast<float>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out asin_core(In x) { return store<Out>(detail::d_asin(static_cast<float>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out acos_core(In x) { return store<Out>(detail::d_acos(static_cast<float>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out atan2_core(In y, In x)
  { return store<Out>(detail::d_atan2(static_cast<float>(y), static_cast<float>(x))); }
  template <typename Out, typename InX, typename InY>
  [[nodiscard]] BND_DBL_FN Out hypot_core(InX x, InY y)
  { return store<Out>(detail::d_hypot(static_cast<float>(x), static_cast<float>(y))); }
} // namespace bnd::math::flt

#endif // !BND_MATH_NO_FP

#endif // BNDcmathfloatHPP
