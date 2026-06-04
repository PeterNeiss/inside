//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDcmathHPP
#define BNDcmathHPP

#include "bound/bound.hpp"

#include <bit>
#include <expected>

//---------------------------------------------------------------------------
// bnd::math — integer-only constexpr transcendentals for `bound`/`rational`.
//
// =====================================================================
//   BIT-EXACT REPRODUCIBILITY CONTRACT
// =====================================================================
// Every function in this header must produce bit-identical output for the
// same input across compiler (gcc/clang/msvc), platform (x86/ARM/RISC-V/
// WASM), optimisation level, and build flags (`-ffast-math`, FMA on/off,
// strict vs. fast FP, etc.). Downstream code relies on this for fuzzing
// corpora, record-and-replay debugging, networked deterministic simulation,
// and cross-platform regression testing.
//
// Requirements that uphold the contract:
//   1. NO `<cmath>`, NO FPU dependency, NO platform intrinsics. Hot paths
//      use only integer arithmetic over signed/unsigned `int64_t`.
//   2. NO runtime-derived tables. All coefficient tables are derived at
//      compile time from `rational` literals (Taylor / economized-Taylor),
//      then quantized to integer Q-format via `constexpr` rounding.
//   3. NO external code generators (Python, Sage, …). Coefficient
//      derivation is constexpr C++; the library is one-language by
//      construction.
//   4. C++20+ is required (we rely on the well-defined arithmetic-shift
//      semantics of signed right shift introduced in C++20).
//   5. Every transcendental ships with checked-in `static_assert` test
//      vectors that pin its bit-exact output for a representative sweep.
//
// Implementation pattern per function:
//   - Range-reduce the input to a small interval using integer ops.
//   - Evaluate a small-degree polynomial in fixed-point Q.30 via Horner.
//   - Quantize the result to `Out`'s grid through the existing `bound`
//     assignment policy (round / clamp / etc. — caller's choice).
//
// =====================================================================
//---------------------------------------------------------------------------
namespace bnd::math
{
  // Public irrational constants. Derived from the same high-precision
  // rational source the internal Q.30 cores use — bit-identical across
  // platforms via constexpr rational arithmetic.
  inline constexpr rational pi{1068966896, 340262731};
  inline constexpr rational two_pi = just<2> * pi;

  namespace detail
  {
    // Q.30 fixed-point unit (1.0) and the marshalling between the rational API
    // and the Q.30 integer cores. Centralises the scale so the `<< 30` constant
    // and the round-trip recipe live in one place.
    inline constexpr imax kQ30One = imax{1} << 30;

    constexpr imax to_q30(rational v) noexcept
    { return rational::mul_unchecked(v, rational{kQ30One}).round(); }

    constexpr rational q30_to_rational(imax x_q30) noexcept
    { return rational{x_q30, kQ30One}; }

    // Internal phase shape used by the Q.30 cores: Q.N turns, period
    // implicit in the unsigned raw's modular wrap. The public sin/cos/tan
    // accept radians and route through this shape internally; callers do
    // not construct it directly. Phase-accumulator patterns (e.g.
    // `examples/oscillator.cpp`) use a plain integer counter for the same
    // free-modular-wrap property and convert to radians at the sin call.
    template <int N>
    using turns_t = bound<{{0_r,
                            rational{(imax{1} << N) - 1, imax{1} << N}},
                           notch<1, (imax{1} << N)>}>;

    // round(r · 2^Bits) — compile-time quantization of a `rational` Taylor
    // coefficient into a fixed-point integer at the requested precision.
    // Returns int64; the call site is responsible for asserting the bit
    // budget. Uses checked rational multiplication and asserts non-overflow
    // by dereferencing the optional — a coefficient that overflows during
    // derivation fails the build, not the run.
    constexpr imax to_q(rational r, int bits) noexcept
    {
      auto scaled = r * rational{imax{1} << bits};
      return scaled.value().round();
    }

    // Q.30 Taylor coefficients for sin(x) on [0, π/2]:
    //   sin(x) = x·(1 + x²·(c0 + x²·(c1 + x²·(c2 + x²·(c3 + x²·c4)))))
    // The leading `1` is materialized as `1 << 30` at evaluation time so
    // the table holds only the higher-order corrections.
    inline constexpr imax kSinCoeffsQ30[5] = {
      to_q(rational{-1,         6}, 30),  // -1/6      = -178956971
      to_q(rational{ 1,       120}, 30),  //  1/120    =    8947849
      to_q(rational{-1,      5040}, 30),  // -1/5040   =    -213044
      to_q(rational{ 1,    362880}, 30),  //  1/362880 =       2959
      to_q(rational{-1, 39916800}, 30),   // -1/...    =        -27
    };

    // Internal alias for the public `bnd::math::two_pi`. Keeps the rest
    // of the detail-namespace code (kRadPerSlotQ30, kTwoPiQ30, …) reading
    // unchanged while consolidating the constant's source.
    inline constexpr rational kTwoPi = bnd::math::two_pi;

    // Per-slot radians for a turns_t<N>, expressed in Q.30. With N ≤ 30
    // the scale is integer (kTwoPi · 2^(30-N)); for N > 30 it's the
    // reciprocal form (kTwoPi / 2^(N-30)). Limit N ≤ 30 in this revision —
    // beyond that the per-slot constant drops below 1 ULP and the
    // polynomial needs to move to a Q.62 internal tier (planned).
    template <int N>
    inline constexpr imax kRadPerSlotQ30 =
      (N <= 30)
        ? rational::mul_unchecked(kTwoPi, rational{imax{1} << (30 - N)}).round()
        : rational::mul_unchecked(kTwoPi, rational{1, imax{1} << (N - 30)}).round();

    // log2(d) for a power-of-2 imax d. Constexpr loop; cheap at compile time.
    constexpr int log2_pow2(imax d) noexcept
    {
      int n = 0;
      while (d > 1) { d >>= 1; ++n; }
      return n;
    }

    // Extract the N from a turns-shaped input bound. Alias templates can't
    // be reverse-deduced, so sin/cos take an arbitrary `boundable In` and
    // derive N from its grid (notch denominator is 2^N). This lets the
    // caller spell the input as `turns_t<16>` while keeping the function
    // signature deducible from a single explicit template arg (`Out`).
    template <boundable In>
    inline constexpr int turn_bits = []{
      static_assert(Lower<In> == 0,
                    "bnd::math: turn-phase input must have Lower == 0");
      static_assert(Notch<In>.Numerator == 1,
                    "bnd::math: turn-phase input must have notch 1/2^N");
      return log2_pow2(abs_den(Notch<In>.Denominator));
    }();

    // Evaluate sin(x) for x ∈ [0, π/2], with x in Q.30 radians.
    // Returns sin(x) in Q.30 ∈ [0, 2^30].
    //
    // Overflow audit: x_q30 ≤ ⌈π/2 · 2^30⌉ = 1'686'629'714 < 2^31.
    //   x²_q30 fits in int63 (squared int31). After `>> 30` it's < 2^32.
    //   Coefficients fit in int29. Each Horner step: int32 · int32 → int64,
    //   `>> 30` → int32. Final `x · one_plus`: int31 · int31 → int62. Safe.
    constexpr imax sin_q30_quadrant1(imax x_q30) noexcept
    {
      imax p   = (x_q30 * x_q30) >> 30;                       // x², Q.30
      imax s   = kSinCoeffsQ30[4];                            // c4
      s = kSinCoeffsQ30[3] + ((p * s) >> 30);                 // c3 + p·c4
      s = kSinCoeffsQ30[2] + ((p * s) >> 30);                 // c2 + p·(…)
      s = kSinCoeffsQ30[1] + ((p * s) >> 30);                 // c1 + p·(…)
      s = kSinCoeffsQ30[0] + ((p * s) >> 30);                 // c0 + p·(…)
      imax one_plus = kQ30One + ((p * s) >> 30);              // 1 + p·bracket
      return (x_q30 * one_plus) >> 30;                        // x · (…)
    }

    // 2π in Q.30 — round(2π · 2^30) ≈ 6'748'060'595. Used by the
    // radians-input path to convert a quadrant-local Q.30 turn into Q.30
    // radians via `(local · kTwoPiQ30) >> 30`. Derived at compile time
    // from `kTwoPi` via `mul_unchecked` (the rational form has numerator
    // 2·1068966896 = 2'137'933'792; numerator · 2^30 ≈ 2.295e18, just
    // inside int63).
    inline constexpr imax kTwoPiQ30 =
      detail::to_q30(kTwoPi);

    // 1/(2π) in Q.30, for converting Q.30 radians to Q.30 turns.
    // round(0.1591549431 · 2^30) = 170891319. (Defined here, ahead of
    // sin/cos, because the radians-input workers reference it.)
    inline constexpr imax kOneOver2PiQ30 = 170891319;
    static_assert(kOneOver2PiQ30 == to_q(rational{1591549431,
                                                  10000000000}, 30));

    // (x · scale) in Q.30, computed as integer_part · scale + (frac_part ·
    // scale) >> 30. The split avoids the imax overflow that a naïve
    // `(x · scale) >> 30` would hit when x has a large integer part.
    constexpr imax scaled_mul_q30(imax x_q30, imax scale_q30) noexcept
    {
      imax k_x     = x_q30 >> 30;
      imax f_x_q30 = x_q30 - (k_x << 30);
      return k_x * scale_q30 + ((f_x_q30 * scale_q30) >> 30);
    }

    // sin (turn-input, internal). Q.N turn-phase → Q.14-rationalized
    // amplitude on `Out`'s grid. The input is `detail::turns_t<N>`-shaped;
    // the raw value already plays the role of `phase mod 1` via the
    // underlying unsigned wrap. Public radians `sin` lowers to this
    // worker after converting the input from radians to a Q.30 turn.
    template <boundable Out, boundable In>
    constexpr Out sin_turn_impl(In phase) noexcept
    {
      constexpr int N = turn_bits<In>;
      static_assert(N >= 2 && N <= 30,
                    "bnd::math: N must be in [2, 30] for the Q.30 internal tier");
      static_assert(Lower<Out> <= -1 && Upper<Out> >= 1,
                    "bnd::math: Out must cover [-1, 1]");

      // Range reduce to first quadrant. Layout of `raw` (N bits):
      //   bit N-1 = half-turn flag (sin is odd about π → negate result)
      //   bit N-2 = quadrant flag  (within half-turn: reflect about π/4 axis)
      //   bits N-3..0 = quadrant-local phase, 0…2^(N-2) = 0…π/2
      constexpr imax half_turn    = imax{1} << (N - 1);
      constexpr imax quarter_turn = imax{1} << (N - 2);
      constexpr imax half_mask    = half_turn - 1;

      auto raw       = static_cast<imax>(phase.Raw);
      bool flip_sign = (raw & half_turn) != 0;
      raw &= half_mask;
      if (raw > quarter_turn)
        raw = half_turn - raw;          // reflect: sin(π - x) = sin(x)

      imax x_q30   = raw * kRadPerSlotQ30<N>;
      imax sin_q30 = sin_q30_quadrant1(x_q30);
      if (flip_sign) sin_q30 = -sin_q30;

      return Out{detail::q30_to_rational(sin_q30)};
    }

    // cos (turn-input, internal). cos(x) = sin(x + π/2) — shift the phase
    // by one quarter-turn (modular wrap on the raw) and reuse sin. The
    // shift is integer-exact, no precision cost at this tier.
    template <boundable Out, boundable In>
    constexpr Out cos_turn_impl(In phase) noexcept
    {
      constexpr int  N            = turn_bits<In>;
      constexpr imax full_mask    = (imax{1} << N) - 1;
      constexpr imax quarter_turn = imax{1} << (N - 2);

      In shifted;
      shifted.Raw = static_cast<typename In::raw_type>(
          (static_cast<imax>(phase.Raw) + quarter_turn) & full_mask);
      return sin_turn_impl<Out>(shifted);
    }

    // Compute sin(x) for x a Q.N turn-phase, returning Q.30. Inlined version
    // of the turn-input sin core for use by tan (which needs both sin and
    // cos before quantizing to Out).
    template <int N>
    constexpr imax sin_q30_from_phase(imax raw) noexcept
    {
      constexpr imax half_turn    = imax{1} << (N - 1);
      constexpr imax quarter_turn = imax{1} << (N - 2);
      constexpr imax half_mask    = half_turn - 1;

      bool flip = (raw & half_turn) != 0;
      raw &= half_mask;
      if (raw > quarter_turn) raw = half_turn - raw;
      imax sin_q30 = sin_q30_quadrant1(raw * kRadPerSlotQ30<N>);
      return flip ? -sin_q30 : sin_q30;
    }

    // Compute sin from a signed Q.30 turn. Reduces mod 2^30 (handling
    // negative inputs), applies the same quadrant reduction as
    // sin_q30_from_phase<N>, then converts the quadrant-local Q.30 turn
    // to Q.30 radians via `(local · kTwoPiQ30) >> 30`. This gives full
    // Q.30 precision regardless of N — sin_q30_from_phase<30> would
    // silently lose bits because kRadPerSlotQ30<30> rounds 2π to 6.
    //
    // Overflow audit: local ≤ 2^28; local · kTwoPiQ30 ≤ 2^28 · 2^33 =
    // 2^61, fits int63. Shift back by 30 keeps the result < 2^31.
    constexpr imax sin_q30_from_turn_q30(imax turn_q30) noexcept
    {
      constexpr imax one_turn     = kQ30One;
      constexpr imax half_turn    = imax{1} << 29;
      constexpr imax quarter_turn = imax{1} << 28;
      constexpr imax half_mask    = half_turn - 1;

      // Reduce to [0, 2^30). The double-mod handles signed input
      // (negative turn_q30 wraps into the positive period).
      turn_q30 = ((turn_q30 % one_turn) + one_turn) % one_turn;

      bool flip = (turn_q30 & half_turn) != 0;
      turn_q30 &= half_mask;
      if (turn_q30 > quarter_turn) turn_q30 = half_turn - turn_q30;

      imax x_q30   = (turn_q30 * kTwoPiQ30) >> 30;
      imax sin_q30 = sin_q30_quadrant1(x_q30);
      return flip ? -sin_q30 : sin_q30;
    }
  } // namespace detail

  // sin: radians-valued bound → amplitude on the auto-deduced output grid
  // (`detail::sin_auto_t<In>` = `bound<{-1, 1}, Notch<In>, … | round_nearest>`).
  // Input is interpreted as radians — the std::sin equivalent. Internally
  // converts to a Q.30 turn (via `× 1/(2π)`) and runs the same Q.30
  // polynomial as the turn-input path.
  //
  // Restrict |angle| ≤ 1024 rad (~163 cycles) so the Q.30 scaling stays
  // inside int63. Wider angles need a different normalization step
  // (deferred).
  template <boundable Out, boundable In>
  constexpr Out sin_impl(In angle) noexcept
  {
    static_assert(Lower<In> >= -1024 && Upper<In> <= 1024,
                  "bnd::math::sin: input must be in [-1024, 1024] rad");
    static_assert(Lower<Out> <= -1 && Upper<Out> >= 1,
                  "bnd::math::sin: Out must cover [-1, 1]");

    rational a     = angle;
    imax     a_q30 = detail::to_q30(a);
    imax     t_q30 = detail::scaled_mul_q30(a_q30, detail::kOneOver2PiQ30);
    imax     sin_q30 = detail::sin_q30_from_turn_q30(t_q30);
    return Out{detail::q30_to_rational(sin_q30)};
  }

  // cos: radians-valued bound → amplitude. cos(x) = sin(x + π/2); add a
  // quarter-turn in Q.30 to the converted turn before the quadrant
  // reducer, same precision as sin.
  template <boundable Out, boundable In>
  constexpr Out cos_impl(In angle) noexcept
  {
    static_assert(Lower<In> >= -1024 && Upper<In> <= 1024,
                  "bnd::math::cos: input must be in [-1024, 1024] rad");
    static_assert(Lower<Out> <= -1 && Upper<Out> >= 1,
                  "bnd::math::cos: Out must cover [-1, 1]");

    constexpr imax quarter_turn_q30 = imax{1} << 28;

    rational a     = angle;
    imax     a_q30 = detail::to_q30(a);
    imax     t_q30 = detail::scaled_mul_q30(a_q30, detail::kOneOver2PiQ30)
                   + quarter_turn_q30;
    imax     cos_q30 = detail::sin_q30_from_turn_q30(t_q30);
    return Out{detail::q30_to_rational(cos_q30)};
  }

  namespace detail
  {
    // tan (turn-input, internal). sin/cos divided in Q.30 with a pole
    // guard. Returns `unexpected(errc::division_by_zero)` when the phase
    // lands exactly on a pole (raw ±π/2 modulo π) and
    // `unexpected(errc::overflow)` when the result exceeds Out's range.
    template <boundable Out, boundable In>
    constexpr std::expected<Out, errc> tan_turn_impl(In phase) noexcept
    {
      constexpr int N = turn_bits<In>;
      static_assert(N >= 2 && N <= 30,
                    "bnd::math: N must be in [2, 30] for the Q.30 internal tier");
      constexpr imax full_mask    = (imax{1} << N) - 1;
      constexpr imax quarter_turn = imax{1} << (N - 2);

      imax sin_raw = static_cast<imax>(phase.Raw);
      imax cos_raw = (sin_raw + quarter_turn) & full_mask;

      imax sin_q30 = sin_q30_from_phase<N>(sin_raw);
      imax cos_q30 = sin_q30_from_phase<N>(cos_raw);

      if (cos_q30 == 0) return std::unexpected(errc::division_by_zero);

      imax tan_q30 = (sin_q30 << 30) / cos_q30;
      rational tan_v = detail::q30_to_rational(tan_q30);
      if (tan_v < Lower<Out> || tan_v > Upper<Out>)
        return std::unexpected(errc::overflow);

      return Out{tan_v};
    }
  } // namespace detail

  // tan: radians-valued bound → amplitude, with pole guard. sin and cos
  // are computed in Q.30 from the radians input (same conversion as
  // public sin/cos) and divided. Returns
  // `unexpected(errc::division_by_zero)` if cos rounds to exactly 0 (the
  // input landed on a pole modulo Q.30 reduction), and
  // `unexpected(errc::overflow)` when the result exceeds Out's interval.
  template <boundable Out, boundable In>
  constexpr std::expected<Out, errc> tan_impl(In angle) noexcept
  {
    static_assert(Lower<In> >= -1024 && Upper<In> <= 1024,
                  "bnd::math::tan: input must be in [-1024, 1024] rad");

    constexpr imax quarter_turn_q30 = imax{1} << 28;

    rational a       = angle;
    imax     a_q30   = detail::to_q30(a);
    imax     t_q30   = detail::scaled_mul_q30(a_q30, detail::kOneOver2PiQ30);
    imax     sin_q30 = detail::sin_q30_from_turn_q30(t_q30);
    imax     cos_q30 = detail::sin_q30_from_turn_q30(t_q30 + quarter_turn_q30);

    if (cos_q30 == 0) return std::unexpected(errc::division_by_zero);

    imax     tan_q30 = (sin_q30 << 30) / cos_q30;
    rational tan_v = detail::q30_to_rational(tan_q30);
    if (tan_v < Lower<Out> || tan_v > Upper<Out>)
      return std::unexpected(errc::overflow);

    return Out{tan_v};
  }

  namespace detail
  {
    // Newton-Raphson square root of a Q.30 value, returning Q.30. Pre: x_q30
    // ≥ 0 and ≤ 2^32 (so x_q60 = x_q30 << 30 fits in int63). The leading-bit
    // initial guess puts r within a factor of 2 of the true sqrt, after
    // which Newton doubles correct bits per iteration; 5 iterations cover
    // the full Q.30 precision from any starting point in the allowed range.
    constexpr imax sqrt_q30(imax x_q30) noexcept
    {
      if (x_q30 == 0) return 0;

      // Initial guess: x_q30 ≈ 2^leading, sqrt(x_value) ≈ 2^((leading-30)/2),
      // so sqrt-in-Q.30 ≈ 2^((leading-30)/2 + 30) = 2^((leading+30)/2).
      int leading = 63 - std::countl_zero(static_cast<umax>(x_q30));
      imax r = imax{1} << ((leading + 30) / 2);

      imax x_q60 = x_q30 << 30;
      for (int i = 0; i < 5; ++i)
        r = (r + x_q60 / r) >> 1;        // r ← (r + x/r) / 2 in Q.30
      return r;
    }
  } // namespace detail

  namespace detail
  {
    // ln(2) in Q.30: round(0.6931471805599453 · 2^30) = 744261117.95… → 744261118.
    // The literal is verified at compile time against the rational source so any
    // typo trips the build; downstream uses the integer form directly.
    inline constexpr imax kLn2Q30 = 744261118;
    // Verification rational is 10-digit (not 16) because chained
    // `mul_unchecked` overflows imax with the higher-precision source even
    // after GCD reduction. 10 digits gives 30+ bits — same Q.30 rounded int.
    static_assert(kLn2Q30 == to_q(rational{6931471806,
                                           10000000000}, 30));

    // Taylor coefficients for 2^f over f ∈ [0, 1), Q.30:
    //   2^f = Σ c_n · f^n   where c_n = (ln 2)^n / n!.
    // Chained `rational` derivation overflows imax even with modest ln2
    // precision (ln2_num^10 has 150 digits), so we derive each c_n by
    // integer recursion in Q.30 with a Q.62 intermediate per multiply.
    // Cumulative error after 10 multiplies is bounded by 10 · 2^-30 ≈ 1e-8 —
    // well below Q.30 ULP at all input magnitudes of interest.
    template <int N>
    constexpr imax exp2_coeff_q30() noexcept
    {
      if constexpr (N == 0) return kQ30One;
      else if constexpr (N == 1) return kLn2Q30;
      else
      {
        imax c = exp2_coeff_q30<N - 1>();
        c = (c * kLn2Q30) >> 30;      // c · ln2,  Q.30
        c /= N;                        // / n
        return c;
      }
    }

    inline constexpr imax kExp2CoeffsQ30[10] = {
      exp2_coeff_q30<0>(), exp2_coeff_q30<1>(), exp2_coeff_q30<2>(),
      exp2_coeff_q30<3>(), exp2_coeff_q30<4>(), exp2_coeff_q30<5>(),
      exp2_coeff_q30<6>(), exp2_coeff_q30<7>(), exp2_coeff_q30<8>(),
      exp2_coeff_q30<9>(),
    };

    // Evaluate 2^f for f in Q.30 ∈ [0, 2^30). Returns 2^f in Q.30 ∈ [2^30, 2^31).
    // 10-term Horner; the residual after this poly is bounded by the next
    // unused term (ln2)^10·f^10/10! ≤ 1.86e-7 — fits Q.22 with room to spare.
    constexpr imax exp2_q30_fractional(imax f_q30) noexcept
    {
      imax r = kExp2CoeffsQ30[9];
      for (int i = 8; i >= 0; --i)
        r = kExp2CoeffsQ30[i] + ((r * f_q30) >> 30);
      return r;
    }
  } // namespace detail

  namespace detail
  {
    // 2/ln(2) in Q.30: 2 / 0.6931471806 ≈ 2.885390081 → 3098164009.
    // Verification uses a low-denominator rational source so the
    // multiplication chain doesn't blow imax.
    inline constexpr imax kTwoOverLn2Q30 = 3098164009;
    static_assert(kTwoOverLn2Q30 == to_q(rational{2885390081,
                                                  1000000000}, 30));

    // Atanh Taylor coefficients in Q.30:
    //   atanh(v) = v + v³/3 + v⁵/5 + … = v · (c_0 + v²·c_1 + v⁴·c_2 + …)
    //   c_n = 1 / (2n + 1)
    // Each coefficient is a direct `to_q` quantization (no chained
    // arithmetic, no overflow risk).
    inline constexpr imax kAtanhCoeffsQ30[10] = {
      to_q(rational{1,  1}, 30), to_q(rational{1,  3}, 30),
      to_q(rational{1,  5}, 30), to_q(rational{1,  7}, 30),
      to_q(rational{1,  9}, 30), to_q(rational{1, 11}, 30),
      to_q(rational{1, 13}, 30), to_q(rational{1, 15}, 30),
      to_q(rational{1, 17}, 30), to_q(rational{1, 19}, 30),
    };

    // Evaluate atanh(v) for v in Q.30, intended for v ∈ [0, 1/3]. Result in
    // Q.30. 10-term Horner on the even-power series; residual at v=1/3 is
    // ~(1/3)^21 / 21 ≈ 4e-12 — comfortably below Q.30 ULP.
    constexpr imax atanh_q30(imax v_q30) noexcept
    {
      imax v_sq = (v_q30 * v_q30) >> 30;       // v², Q.30
      imax r    = kAtanhCoeffsQ30[9];
      for (int i = 8; i >= 0; --i)
        r = kAtanhCoeffsQ30[i] + ((v_sq * r) >> 30);
      return (v_q30 * r) >> 30;
    }

    // Q.30 core for log2: leading-bit reduction + atanh series. Pre: x_q30 > 0.
    // Returns log2(x) in Q.30. Shared by public log2() and by compile-time
    // derivation of log2(Base) constants for pow_base / exp.
    constexpr imax log2_q30(imax x_q30) noexcept
    {
      int leading = 63 - std::countl_zero(static_cast<umax>(x_q30));
      int k       = leading - 30;
      imax m_q30  = (leading >= 30) ? (x_q30 >> (leading - 30))
                                     : (x_q30 << (30 - leading));
      imax m_minus_1 = m_q30 - kQ30One;
      imax m_plus_1  = m_q30 + kQ30One;
      imax v_q30     = (m_minus_1 << 30) / m_plus_1;
      imax log2_m_q30 = (kTwoOverLn2Q30 * atanh_q30(v_q30)) >> 30;
      return (imax{k} << 30) + log2_m_q30;
    }

    // Q.30 core for exp2: integer/fractional split + 10-term Taylor. Returns
    // the result as a `rational` so the caller picks the bound assignment.
    constexpr rational exp2_q30_to_rational(imax x_q30) noexcept
    {
      imax k          = x_q30 >> 30;
      imax f_q30      = x_q30 - (k << 30);
      imax pow2_f_q30 = exp2_q30_fractional(f_q30);
      return rational{pow2_f_q30, imax{1} << (30 - k)};
    }

    // log2(e) in Q.30 — derived at compile time from the same log2_q30 core
    // the public log2() uses. The compile-time loop unrolls to a small set of
    // integer ops; the result is a single int64 constant.
    inline constexpr imax kLog2eQ30 =
      log2_q30(to_q(rational{2718281828, 1000000000}, 30));

    // log2(Base) in Q.30 for compile-time-known integer Base ≥ 2.
    template <imax Base>
    inline constexpr imax kLog2IntBaseQ30 = log2_q30(Base << 30);

    // (`scaled_mul_q30` is defined earlier — alongside `kOneOver2PiQ30` /
    // `kTwoPiQ30` — because the radians-input sin/cos workers consume it.)
  } // namespace detail

  // log2: positive bound → bound. Calls the shared `detail::log2_q30` core
  // (leading-bit reduction + atanh series; see core for algorithm details).
  template <boundable Out, boundable In>
  constexpr Out log2_impl(In x) noexcept
  {
    static_assert(Lower<In> > 0,
                  "bnd::math::log2: input must be strictly positive");

    rational xv    = x;
    imax     x_q30 = detail::to_q30(xv);
    return Out{detail::q30_to_rational(detail::log2_q30(x_q30))};
  }

  // exp2: bound → bound, returning 2^x. Calls the shared
  // `detail::exp2_q30_to_rational` core (integer/fractional split + 10-term
  // Taylor; see core for algorithm details).
  //
  // Restrict |x| ≤ 30 so the rational denominator 2^(30 - k) fits in int63.
  // The output `Out` must include non-negative values and cover at least
  // [2^Lower<In>, 2^Upper<In>] — anything narrower needs `clamp` to absorb
  // overflow at the assignment.
  template <boundable Out, boundable In>
  constexpr Out exp2_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -30 && Upper<In> <= 30,
                  "bnd::math::exp2: input must be in [-30, 30]");
    static_assert(Lower<Out> >= 0,
                  "bnd::math::exp2: Out must be non-negative");

    rational xv    = x;
    imax     x_q30 = detail::to_q30(xv);
    return Out{detail::exp2_q30_to_rational(x_q30)};
  }

  // exp: thin wrapper. exp(x) = exp2(x · log2(e)). The scaling factor
  // log2(e) ≈ 1.4427, so x must stay inside [-30/log2(e), 30/log2(e)] ≈
  // [-20.79, 20.79] for exp2's denominator-shift envelope. We use [-20, 20].
  template <boundable Out, boundable In>
  constexpr Out exp_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -20 && Upper<In> <= 20,
                  "bnd::math::exp: input must be in [-20, 20]");
    static_assert(Lower<Out> >= 0,
                  "bnd::math::exp: Out must be non-negative");

    rational xv    = x;
    imax     x_q30 = detail::to_q30(xv);
    imax     scaled_q30 = detail::scaled_mul_q30(x_q30, detail::kLog2eQ30);
    return Out{detail::exp2_q30_to_rational(scaled_q30)};
  }

  // log: thin wrapper. log(x) = log2(x) · ln(2). Result precision matches
  // log2 minus 1-2 ULP from the final Q.30 scaling.
  template <boundable Out, boundable In>
  constexpr Out log_impl(In x) noexcept
  {
    static_assert(Lower<In> > 0,
                  "bnd::math::log: input must be strictly positive");

    rational xv    = x;
    imax     x_q30 = detail::to_q30(xv);
    imax     log2_x_q30 = detail::log2_q30(x_q30);
    imax     log_x_q30  = (log2_x_q30 * detail::kLn2Q30) >> 30;
    return Out{detail::q30_to_rational(log_x_q30)};
  }

  // pow_base<Base>(x) = Base^x for compile-time-known integer Base ≥ 2.
  // Implemented as exp2(x · log2(Base)) with log2(Base) derived at compile
  // time via the shared log2_q30 core — no hand-typed magic constants.
  // For Base = 10, this is the building block for `db_to_linear`.
  template <imax Base, boundable Out, boundable In>
  constexpr Out pow_base_impl(In x) noexcept
  {
    static_assert(Base >= 2, "bnd::math::pow_base: Base must be ≥ 2");
    static_assert(Lower<Out> >= 0,
                  "bnd::math::pow_base: Out must be non-negative");

    rational xv    = x;
    imax     x_q30 = detail::to_q30(xv);
    imax     scaled_q30 = detail::scaled_mul_q30(x_q30,
                                                 detail::kLog2IntBaseQ30<Base>);
    return Out{detail::exp2_q30_to_rational(scaled_q30)};
  }

  namespace detail
  {
    // (`kOneOver2PiQ30` is defined earlier — used by the radians-input
    // sin/cos workers as well as by the CORDIC-table derivation below.)

    // atan(x) in Q.30 radians, via Taylor: atan(x) = x − x³/3 + x⁵/5 − …
    // Pre: |x_q30| < 2^30 (i.e. |x| < 1). For x_q30 = 2^30 (x = 1) the
    // series is the slow Leibniz series; the caller special-cases atan(1).
    constexpr imax atan_q30_rad(imax x_q30) noexcept
    {
      imax x_sq = (x_q30 * x_q30) >> 30;
      imax term = x_q30;
      imax sum  = 0;
      for (int n = 0; n < 40; ++n) {
        int  denom = 2 * n + 1;
        imax piece = term / denom;
        sum += (n % 2 == 0) ? piece : -piece;
        term = (term * x_sq) >> 30;
        if (term == 0) break;
      }
      return sum;
    }

    // atan(2^-i) in Q.30 turns. Compile-time derivation:
    //   i = 0  → atan(1) = π/4 = exactly 1/8 turn, no Taylor needed.
    //   i ≥ 1  → atan(2^-i) via Taylor in radians, converted to turns.
    template <int I>
    constexpr imax compute_atan_pow2_turn_q30() noexcept
    {
      if constexpr (I == 0) return imax{1} << 27;   // 1/8 turn
      else {
        constexpr imax x_q30   = imax{1} << (30 - I);
        imax           atan_rad = atan_q30_rad(x_q30);
        return (atan_rad * kOneOver2PiQ30) >> 30;
      }
    }

    // 30-entry CORDIC lookup table: atan(2^-i) for i = 0..29, in Q.30 turns.
    // Each entry is derived at compile time from the Taylor `atan_q30_rad`
    // plus rad-to-turn scaling — no hand-typed magic constants.
    inline constexpr imax kCordicAtanTurnQ30[30] = {
      compute_atan_pow2_turn_q30< 0>(), compute_atan_pow2_turn_q30< 1>(),
      compute_atan_pow2_turn_q30< 2>(), compute_atan_pow2_turn_q30< 3>(),
      compute_atan_pow2_turn_q30< 4>(), compute_atan_pow2_turn_q30< 5>(),
      compute_atan_pow2_turn_q30< 6>(), compute_atan_pow2_turn_q30< 7>(),
      compute_atan_pow2_turn_q30< 8>(), compute_atan_pow2_turn_q30< 9>(),
      compute_atan_pow2_turn_q30<10>(), compute_atan_pow2_turn_q30<11>(),
      compute_atan_pow2_turn_q30<12>(), compute_atan_pow2_turn_q30<13>(),
      compute_atan_pow2_turn_q30<14>(), compute_atan_pow2_turn_q30<15>(),
      compute_atan_pow2_turn_q30<16>(), compute_atan_pow2_turn_q30<17>(),
      compute_atan_pow2_turn_q30<18>(), compute_atan_pow2_turn_q30<19>(),
      compute_atan_pow2_turn_q30<20>(), compute_atan_pow2_turn_q30<21>(),
      compute_atan_pow2_turn_q30<22>(), compute_atan_pow2_turn_q30<23>(),
      compute_atan_pow2_turn_q30<24>(), compute_atan_pow2_turn_q30<25>(),
      compute_atan_pow2_turn_q30<26>(), compute_atan_pow2_turn_q30<27>(),
      compute_atan_pow2_turn_q30<28>(), compute_atan_pow2_turn_q30<29>(),
    };

    // CORDIC vectoring for atan2. Pre: x_q30 > 0 (right half-plane). The
    // iteration rotates (x, y) toward the positive x-axis; the accumulated
    // angle is atan2(y₀, x₀). 30 iterations give ~30 bits of precision.
    //
    // Magnitude growth: |x|, |y| scale by K = ∏ √(1 + 4^-i) ≈ 1.6468 over
    // all iterations. Inputs bounded to Q.30 of |·| ≤ 1 keep intermediates
    // below 2^31 — comfortably inside int63.
    constexpr imax atan2_cordic_q30_turn(imax y_q30, imax x_q30) noexcept
    {
      imax z = 0;
      for (int i = 0; i < 30; ++i) {
        if (y_q30 >= 0) {
          // Rotate clockwise: x ← x + y/2^i, y ← y − x/2^i, z += atan(2^-i).
          imax dx = y_q30 >> i;
          imax dy = x_q30 >> i;
          x_q30 += dx;
          y_q30 -= dy;
          z     += kCordicAtanTurnQ30[i];
        } else {
          imax dx = y_q30 >> i;
          imax dy = x_q30 >> i;
          x_q30 -= dx;
          y_q30 += dy;
          z     -= kCordicAtanTurnQ30[i];
        }
      }
      return z;
    }
  } // namespace detail

  // atan2: signed bound, signed bound → turn-phase in [-1/2, 1/2].
  // Implemented as CORDIC vectoring with quadrant pre-rotation. Output is
  // in turns (1/2 = π, 1/4 = π/2, 1/8 = π/4, etc.); use 2π scaling at the
  // boundary if the caller wants radians.
  //
  // Restrict inputs to [-1, 1] for this revision — CORDIC only depends on
  // the y/x ratio, so callers with wider magnitudes should normalize first.
  template <boundable Out, boundable In>
  constexpr Out atan2_impl(In y, In x) noexcept
  {
    static_assert(Lower<In> >= -1 && Upper<In> <= 1,
                  "bnd::math::atan2: inputs must be in [-1, 1]; normalize first for wider ranges");
    static_assert(Lower<Out> <= -0.5_r && Upper<Out> >= 0.5_r,
                  "bnd::math::atan2: Out must cover [-1/2, 1/2] turn");

    rational yv = y, xv = x;
    imax y_q30 = detail::to_q30(yv);
    imax x_q30 = detail::to_q30(xv);

    // atan2(0, 0) is undefined; convention is 0. Without this guard the
    // CORDIC iteration trivially leaves x, y at zero but still accumulates
    // the angle table at each step, producing garbage.
    if (x_q30 == 0 && y_q30 == 0) return Out{0};

    // Quadrant pre-rotation: CORDIC requires x > 0. For x < 0, rotate the
    // vector by ±π/2 to land in the right half-plane and add the rotation
    // back at the end.
    //   Q2 (x<0, y≥0): rotate by −π/2  →  (x',y') = (y, −x).  θ = CORDIC + 1/4.
    //   Q3 (x<0, y<0): rotate by +π/2  →  (x',y') = (−y, x). θ = CORDIC − 1/4.
    constexpr imax kQuarterTurnQ30 = imax{1} << 28;
    imax pre_rotation = 0;
    if (x_q30 < 0) {
      if (y_q30 >= 0) {
        imax new_x = y_q30;  imax new_y = -x_q30;
        x_q30 = new_x;       y_q30 = new_y;
        pre_rotation = kQuarterTurnQ30;
      } else {
        imax new_x = -y_q30; imax new_y = x_q30;
        x_q30 = new_x;       y_q30 = new_y;
        pre_rotation = -kQuarterTurnQ30;
      }
    }

    imax atan_q30  = detail::atan2_cordic_q30_turn(y_q30, x_q30);
    imax total_q30 = atan_q30 + pre_rotation;

    return Out{detail::q30_to_rational(total_q30)};
  }

  //---------------------------------------------------------------------------
  // Algebraic tier — exact, no polynomial machinery. Each function wraps the
  // corresponding `rational` operation and routes through `Out`'s assignment.
  //---------------------------------------------------------------------------

  // |x|. Output Lower must be ≥ 0 (the result is always non-negative).
  template <boundable Out, boundable In>
  constexpr Out abs_impl(In x) noexcept
  {
    static_assert(Lower<Out> <= 0,
                  "bnd::math::abs: Out must include 0");
    return Out{bnd::abs(rational{x})};
  }

  // ⌊x⌋ — largest integer ≤ x.
  template <boundable Out, boundable In>
  constexpr Out floor_impl(In x) noexcept
  {
    return Out{rational{x}.floor()};
  }

  // ⌈x⌉ — smallest integer ≥ x.
  template <boundable Out, boundable In>
  constexpr Out ceil_impl(In x) noexcept
  {
    return Out{rational{x}.ceil()};
  }

  // x rounded to nearest integer, half-away-from-zero (matches the existing
  // `rational::round()` convention used throughout the library).
  template <boundable Out, boundable In>
  constexpr Out round_impl(In x) noexcept
  {
    return Out{rational{x}.round()};
  }

  // x truncated toward zero. Distinct from floor for negative inputs:
  // trunc(-1.7) = -1 vs floor(-1.7) = -2.
  template <boundable Out, boundable In>
  constexpr Out trunc_impl(In x) noexcept
  {
    return Out{rational{x}.trunc()};
  }

  // x mod y = x − ⌊x/y⌋·y (truncated-division convention, matching std::fmod).
  // Result has the sign of x. Pre: y must not span zero (caller-enforced).
  template <boundable Out, boundable InX, boundable InY>
  constexpr Out fmod_impl(InX x, InY y) noexcept
  {
    rational xv = x;
    rational yv = y;
    rational q  = rational::div_unchecked(xv, yv);
    imax     qt = q.trunc();
    rational qy = rational::mul_unchecked(rational{qt}, yv);
    rational r  = rational::add_unchecked(xv, -qy);
    return Out{r};
  }

  //---------------------------------------------------------------------------
  // Auto-deducing forms — algebraic tier (phase 1 of step 10).
  //
  // Each function gets a second overload that derives the output bound type
  // from `In` (interval endpoints + notch + policy). The auto form delegates
  // to the explicit form; the two paths share one implementation.
  //
  // Resolution: with two overloads, `f<Out>(x)` picks the explicit form,
  // and `f(x)` (no template arg) picks the auto form — the explicit form's
  // Out can't be deduced from a parameter, so it drops out of overload
  // resolution when no template arg is given.
  //
  // Notch policy: abs / fmod inherit `Notch<In>` for natural-precision
  // arithmetic. floor / ceil / round / trunc deduce `notch<1>` since their
  // outputs are integer-valued by definition — integer-storage is the
  // smallest and most honest representation.
  //---------------------------------------------------------------------------
  namespace detail
  {
    // max(|Lower<In>|, |Upper<In>|) as a constexpr rational. Used to size
    // the auto-deduced abs output.
    template <boundable In>
    inline constexpr rational abs_auto_upper =
      (bnd::abs(Lower<In>) > bnd::abs(Upper<In>))
        ? bnd::abs(Lower<In>) : bnd::abs(Upper<In>);

    template <boundable In>
    using abs_auto_t = bound<{{0_r, abs_auto_upper<In>},
                              Notch<In>}, BoundPolicy<In>>;

    template <boundable In>
    using floor_auto_t = bound<{{rational{Lower<In>.floor()},
                                  rational{Upper<In>.floor()}},
                                 notch<1>}, BoundPolicy<In>>;

    template <boundable In>
    using ceil_auto_t = bound<{{rational{Lower<In>.ceil()},
                                 rational{Upper<In>.ceil()}},
                                notch<1>}, BoundPolicy<In>>;

    template <boundable In>
    using round_auto_t = bound<{{rational{Lower<In>.round()},
                                  rational{Upper<In>.round()}},
                                 notch<1>}, BoundPolicy<In>>;

    template <boundable In>
    using trunc_auto_t = bound<{{rational{Lower<In>.trunc()},
                                  rational{Upper<In>.trunc()}},
                                 notch<1>}, BoundPolicy<In>>;

    // (fmod has no auto form: with two boundable inputs, calls of the form
    // `fmod<X>(X_val, X_val)` are ambiguous between the explicit-Out and
    // auto-deduced overloads — C++ partial ordering can't tell them apart
    // because both deduce `X` for their first template parameter. The
    // explicit form is the canonical entry point; users who want auto
    // deduction can construct the output bound from x and y manually.)
  } // namespace detail

  template <boundable In>
  constexpr auto abs(In x) noexcept { return abs_impl<detail::abs_auto_t<In>>(x); }

  template <boundable In>
  constexpr auto floor(In x) noexcept { return floor_impl<detail::floor_auto_t<In>>(x); }

  template <boundable In>
  constexpr auto ceil(In x) noexcept { return ceil_impl<detail::ceil_auto_t<In>>(x); }

  template <boundable In>
  constexpr auto round(In x) noexcept { return round_impl<detail::round_auto_t<In>>(x); }

  template <boundable In>
  constexpr auto trunc(In x) noexcept { return trunc_impl<detail::trunc_auto_t<In>>(x); }

  // sqrt: non-negative bound → bound. Newton-Raphson on Q.30 integer math,
  // leading-bit initial guess. The input must have Lower == 0, a power-of-2
  // notch denominator (1/2^K with K ≤ 30), and Upper ≤ 4 so the x_q60
  // intermediate fits in int63. The mixed-sign overload below (`signed_impl`)
  // accepts Lower < 0 and returns `slim::optional` — nullopt when the runtime
  // value is negative.
  template <boundable Out, boundable In>
  constexpr Out sqrt_impl(In x) noexcept
  {
    static_assert(Lower<In> == 0,
                  "bnd::math::sqrt: input must start at 0 (use the mixed-sign overload)");
    static_assert(Upper<In> <= 4,
                  "bnd::math::sqrt: input must be ≤ 4 for the Q.30 internal tier");
    static_assert(Notch<In>.Numerator == 1,
                  "bnd::math::sqrt: input notch must be 1/2^K");
    constexpr imax notch_den = abs_den(Notch<In>.Denominator);
    static_assert((notch_den & (notch_den - 1)) == 0,
                  "bnd::math::sqrt: input notch denominator must be a power of 2");
    static_assert(Lower<Out> <= 0,
                  "bnd::math::sqrt: Out must include 0");

    constexpr int K = detail::log2_pow2(notch_den);
    static_assert(K <= 30,
                  "bnd::math::sqrt: input must be Q.30 or coarser (Q.62 tier planned)");

    // Convert Q.K input raw to Q.30 by integer left-shift (exact).
    imax x_q30   = static_cast<imax>(x.Raw) << (30 - K);
    imax r_q30   = detail::sqrt_q30(x_q30);

    return Out{detail::q30_to_rational(r_q30)};
  }

  // Mixed-sign sqrt: accepts inputs whose interval crosses zero. Returns
  // `slim::optional<Out>{nullopt}` if the runtime value is negative,
  // otherwise the same Q.30 result as `sqrt_impl`. The notch and
  // |Upper| / |Lower| constraints mirror the non-negative path.
  template <boundable Out, boundable In>
  constexpr slim::optional<Out> sqrt_signed_impl(In x) noexcept
  {
    static_assert(Notch<In>.Numerator == 1,
                  "bnd::math::sqrt: input notch must be 1/2^K");
    constexpr imax notch_den = abs_den(Notch<In>.Denominator);
    static_assert((notch_den & (notch_den - 1)) == 0,
                  "bnd::math::sqrt: input notch denominator must be a power of 2");
    static_assert(Lower<Out> <= 0,
                  "bnd::math::sqrt: Out must include 0");
    constexpr rational max_abs =
        (bnd::abs(Lower<In>) > bnd::abs(Upper<In>))
            ? bnd::abs(Lower<In>) : bnd::abs(Upper<In>);
    static_assert(max_abs <= 4,
                  "bnd::math::sqrt: max(|Lower|, |Upper|) must be ≤ 4 for the Q.30 internal tier");
    constexpr int K = detail::log2_pow2(notch_den);
    static_assert(K <= 30,
                  "bnd::math::sqrt: input must be Q.30 or coarser (Q.62 tier planned)");

    rational v = as_rational(x);
    if (v < rational{0})
      return slim::nullopt;

    imax v_q30 = detail::to_q30(v);
    imax r_q30 = detail::sqrt_q30(v_q30);
    return Out{detail::q30_to_rational(r_q30)};
  }

  //---------------------------------------------------------------------------
  // Auto-deducing forms — monotonic transcendental tier (phase 2 of step 10).
  //
  // Each function below takes the same constraints as its explicit-Out
  // counterpart but derives the output bound type from the input's interval
  // and notch:
  //   - Lower / Upper computed by running the existing Q.30 cores on the
  //     input endpoints at compile time.
  //   - Outward rounding to `Notch<In>` ensures the deduced bound covers
  //     the true mathematical range even when the endpoint is irrational.
  //   - Notch and policy inherited from `In`.
  //---------------------------------------------------------------------------
  namespace detail
  {
    // Round a rational down to the nearest multiple of `notch`.
    constexpr rational floor_to_notch(rational x, rational notch) noexcept
    {
      auto q  = rational::div_unchecked(x, notch);
      imax n  = q.floor();
      return rational::mul_unchecked(rational{n}, notch);
    }

    // Round a rational up to the nearest multiple of `notch`.
    constexpr rational ceil_to_notch(rational x, rational notch) noexcept
    {
      auto q = rational::div_unchecked(x, notch);
      imax n = q.ceil();
      return rational::mul_unchecked(rational{n}, notch);
    }

    // Helpers: evaluate the existing Q.30 cores on a compile-time-known
    // rational endpoint and return the result as a rational.
    constexpr rational sqrt_endpoint(rational v) noexcept
    {
      if (v == 0) return 0_r;
      imax v_q30 = detail::to_q30(v);
      return detail::q30_to_rational(sqrt_q30(v_q30));
    }

    constexpr rational exp2_endpoint(rational v) noexcept
    {
      imax v_q30 = detail::to_q30(v);
      return exp2_q30_to_rational(v_q30);
    }

    constexpr rational log2_endpoint(rational v) noexcept
    {
      imax v_q30 = detail::to_q30(v);
      return detail::q30_to_rational(log2_q30(v_q30));
    }

    constexpr rational exp_endpoint(rational v) noexcept
    {
      imax v_q30  = detail::to_q30(v);
      imax sc_q30 = scaled_mul_q30(v_q30, kLog2eQ30);
      return exp2_q30_to_rational(sc_q30);
    }

    constexpr rational log_endpoint(rational v) noexcept
    {
      imax v_q30 = detail::to_q30(v);
      imax l_q30 = log2_q30(v_q30);
      imax r_q30 = (l_q30 * kLn2Q30) >> 30;
      return detail::q30_to_rational(r_q30);
    }

    template <imax Base>
    constexpr rational pow_base_endpoint(rational v) noexcept
    {
      imax v_q30  = detail::to_q30(v);
      imax sc_q30 = scaled_mul_q30(v_q30, kLog2IntBaseQ30<Base>);
      return exp2_q30_to_rational(sc_q30);
    }

    // Deduction aliases. Each rounds endpoints outward to Notch<In> and
    // adds `round_nearest` to the policy — transcendental output values
    // come out of the Q.30 cores with sub-notch drift, so the assignment
    // into the deduced bound needs a rounding rule to land on the grid.
    template <boundable In>
    using sqrt_auto_t = bound<{{0_r,
                                ceil_to_notch(sqrt_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    // Mixed-sign sqrt: Upper of the result is sqrt of the larger absolute
    // endpoint, since the runtime value can be anywhere in [Lower, Upper].
    template <boundable In>
    inline constexpr rational sqrt_signed_upper =
        (bnd::abs(Lower<In>) > bnd::abs(Upper<In>))
            ? bnd::abs(Lower<In>) : bnd::abs(Upper<In>);

    template <boundable In>
    using sqrt_signed_auto_t = bound<{{0_r,
                                       ceil_to_notch(sqrt_endpoint(sqrt_signed_upper<In>),
                                                     Notch<In>)},
                                      Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using exp2_auto_t = bound<{{floor_to_notch(exp2_endpoint(Lower<In>), Notch<In>),
                                ceil_to_notch (exp2_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using log2_auto_t = bound<{{floor_to_notch(log2_endpoint(Lower<In>), Notch<In>),
                                ceil_to_notch (log2_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using exp_auto_t = bound<{{floor_to_notch(exp_endpoint(Lower<In>), Notch<In>),
                               ceil_to_notch (exp_endpoint(Upper<In>), Notch<In>)},
                              Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using log_auto_t = bound<{{floor_to_notch(log_endpoint(Lower<In>), Notch<In>),
                               ceil_to_notch (log_endpoint(Upper<In>), Notch<In>)},
                              Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <imax Base, boundable In>
    using pow_base_auto_t = bound<{{floor_to_notch(pow_base_endpoint<Base>(Lower<In>), Notch<In>),
                                    ceil_to_notch (pow_base_endpoint<Base>(Upper<In>), Notch<In>)},
                                   Notch<In>}, BoundPolicy<In> | round_nearest>;
  } // namespace detail

  template <boundable In>
    requires (Lower<In> == rational{0})
  constexpr auto sqrt(In x) noexcept { return sqrt_impl<detail::sqrt_auto_t<In>>(x); }

  // Mixed-sign overload: dispatches to `sqrt_signed_impl`, returning
  // `slim::optional<bound>` so a negative runtime value surfaces as nullopt
  // instead of UB.
  template <boundable In>
    requires (Lower<In> < rational{0})
  constexpr auto sqrt(In x) noexcept
  { return sqrt_signed_impl<detail::sqrt_signed_auto_t<In>>(x); }

  template <boundable In>
  constexpr auto exp2(In x) noexcept { return exp2_impl<detail::exp2_auto_t<In>>(x); }

  template <boundable In>
  constexpr auto log2(In x) noexcept { return log2_impl<detail::log2_auto_t<In>>(x); }

  template <boundable In>
  constexpr auto exp(In x) noexcept { return exp_impl<detail::exp_auto_t<In>>(x); }

  template <boundable In>
  constexpr auto log(In x) noexcept { return log_impl<detail::log_auto_t<In>>(x); }

  template <imax Base, boundable In>
  constexpr auto pow_base(In x) noexcept
  { return pow_base_impl<Base, detail::pow_base_auto_t<Base, In>>(x); }

  //---------------------------------------------------------------------------
  // Auto-deducing forms — trig + atan2 + tan + fmod.
  //
  // sin / cos default to the full amplitude range [-1, 1]; atan2 defaults to
  // the full turn-phase range [-1/2, 1/2]. tan defaults to [-1024, 1024]
  // (covers all phases >1 slot from a pole; closer-to-pole phases trip the
  // overflow branch of the returned `expected`). fmod inherits sign from x.
  // Notch is inherited from input throughout.
  //---------------------------------------------------------------------------
  namespace detail
  {
    template <boundable In>
    using sin_auto_t = bound<{{-1_r, 1_r},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using cos_auto_t = sin_auto_t<In>;

    template <boundable In>
    using atan2_auto_t = bound<{{rational{-1, 2}, rational{1, 2}},
                                 Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using tan_auto_t = bound<{{-1024_r, 1024_r},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable InX, boundable InY>
    using fmod_auto_t = bound<{{-bnd::abs(Upper<InY>), bnd::abs(Upper<InY>)},
                                Notch<InX>}, BoundPolicy<InX> | round_nearest>;
  } // namespace detail

  // Public auto-form trig — radians input, std::sin-shaped. The only
  // public trig entry points; the turn-input workers live in `detail::`
  // (`sin_turn_impl`, `cos_turn_impl`, `tan_turn_impl`) for internal use.
  template <boundable In>
  constexpr auto sin(In angle) noexcept
  { return sin_impl<detail::sin_auto_t<In>>(angle); }

  template <boundable In>
  constexpr auto cos(In angle) noexcept
  { return cos_impl<detail::cos_auto_t<In>>(angle); }

  template <boundable In>
  constexpr auto atan2(In y, In x) noexcept
  { return atan2_impl<detail::atan2_auto_t<In>>(y, x); }

  template <boundable In>
  constexpr auto tan(In angle) noexcept
  { return tan_impl<detail::tan_auto_t<In>>(angle); }

  template <boundable InX, boundable InY>
  constexpr auto fmod(InX x, InY y) noexcept
  { return fmod_impl<detail::fmod_auto_t<InX, InY>>(x, y); }
}

#endif
