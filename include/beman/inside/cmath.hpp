// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_CMATH_HPP
#define BEMAN_INSIDE_CMATH_HPP

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath_double.hpp>   // the double (binary64) math engine
#include <beman/inside/cmath_float.hpp>    // the float  (binary32) math engine

#include <expected>   // std::expected, std::unexpected

#include <array>
#include <bit>

// The public beman::inside::math::* functions dispatch to the double engine (default) or
// the integer/CORDIC engine (`-DBEMAN_INSIDE_MATH_FIXED`). The integer engine is
// always `constexpr`; the double engine becomes `constexpr` automatically on
// C++26 toolchains where <cmath> is constexpr (P1383 — std::fma / std::sqrt /
// std::nearbyint; feature macro __cpp_lib_constexpr_cmath). That branch is
// inert (and untested) until such a toolchain exists. Decision 2026-06-12:
// no compile-time softfloat emulation — wait for the standard.
// BEMAN_INSIDE_MATH_NO_FP (resolved in cmath_double.hpp, included above) selects the
// integer/CORDIC engine and is implied by BEMAN_INSIDE_MATH_FIXED — so the integer engine
// is constexpr here. The double engine becomes constexpr only on a C++26 toolchain
// with constexpr <cmath> (P1383); that branch is inert until such a toolchain.
#if defined(BEMAN_INSIDE_MATH_NO_FP) \
    || (defined(__cpp_lib_constexpr_cmath) && __cpp_lib_constexpr_cmath >= 202202L)
#  define BEMAN_INSIDE_MATH_FN constexpr
#else
#  define BEMAN_INSIDE_MATH_FN
#endif

//---------------------------------------------------------------------------
// beman::inside::math — one transcendental API, two interchangeable engines selected by
// the `BEMAN_INSIDE_MATH_FIXED` macro. Both are feature-equivalent (same functions,
// signatures, domains):
//
//   * DEFAULT — double engine (`cmath_double.hpp`): hardware `double`
//     polynomials on `real` bounds. Bit-identical on any IEEE-754 binary64
//     platform built without `-ffast-math`. Fast (~ns); needs an FPU; runtime.
//   * `BEMAN_INSIDE_MATH_FIXED` — integer/CORDIC engine (this file): FPU-free, constexpr,
//     UNCONDITIONALLY bit-identical (any platform/flags). For embedded/portability.
//   * `BEMAN_INSIDE_MATH_FLOAT` — float (binary32) engine (`cmath_float.hpp`): like the
//     double engine but single precision, for single-precision-only FPUs.
//
// The macro picks only which engine the UNQUALIFIED `beman::inside::math::fn` uses; all
// engines are always reachable by namespace (`cordic::`/`dbl::`/`flt::`). Default
// selection (the dispatch below): `BEMAN_INSIDE_MATH_NO_FP`→cordic, else
// `BEMAN_INSIDE_MATH_FLOAT`→flt, else dbl.
//
// `BEMAN_INSIDE_MATH_NO_FP` (implied by `BEMAN_INSIDE_MATH_FIXED`, auto-enabled when
// `__STDC_HOSTED__ == 0`) compiles the double AND float engines and their
// `<cmath>` out entirely, leaving the integer engine — so the library, including
// the single header, builds with no hardware floating point.
//
// =====================================================================
//   INTEGER (CORDIC) ENGINE — BIT-EXACT REPRODUCIBILITY CONTRACT
// =====================================================================
// Every function below produces bit-identical output for the same input across
// compiler, platform, optimisation level, and FP flags — relied on for fuzzing
// corpora, record-and-replay, deterministic simulation, regression testing.
// (The double engine's contract lives atop cmath_double.hpp.) Requirements:
//   1. NO `<cmath>`/FPU/intrinsics; hot paths are integer-only over int64.
//   2. NO runtime-derived tables — coefficients derived at compile time from
//      `rational` literals, quantized to integer Q-format via constexpr rounding.
//   3. NO external code generators; derivation is constexpr C++.
//   4. C++20+ (well-defined signed right-shift semantics).
//   5. Each transcendental ships checked-in `static_assert` vectors pinning its
//      bit-exact output.
//
// Pattern per function: pick a working scale 2^W from the output grid
// (`working_bits`), range-reduce with integer ops, evaluate via the shared
// shift-add CORDIC (or Newton for sqrt/cbrt) using the portable wide `fmul`,
// then quantize onto `Out`'s grid through its assignment policy.
//---------------------------------------------------------------------------
namespace beman::inside::math
{
  using beman::inside::detail::rational;

  namespace detail
  {
    using namespace beman::inside::detail;

    // Exact rational source for the irrational constants — the fixed-point cores
    // need the exact form; bit-identical across platforms.
    inline constexpr rational pi_r{1068966896, 340262731};
    inline constexpr rational two_pi_r = 2 * pi_r;

    // Policy of an auto-deduced output: the input's, minus any fixed-width
    // storage flag (i8 … u64) — the output range differs, as for arithmetic.
    template <insidable In>
    inline constexpr policy_flag out_policy = InsidePolicy<In> & ~raw_width_mask;

    // Out's interval endpoints as F (via double, like the runtime value), folded
    // at compile time for the FP engines' range checks.
    template <typename F, insidable Out>
    inline constexpr F lower_fp = static_cast<F>(static_cast<double>(Lower<Out>));
    template <typename F, insidable Out>
    inline constexpr F upper_fp = static_cast<F>(static_cast<double>(Upper<Out>));

    // Every transcendental operand must carry the `real` policy flag: under the
    // default engine it selects double-backed dyadic storage, under BEMAN_INSIDE_MATH_FIXED
    // integer round_nearest. Requiring it keeps both engines' call sites identical
    // and avoids the slow integer-I/O path. Pure grid ops (abs/floor/ceil/round/
    // trunc/fmod) have no engine and don't require it.
    template <insidable In>
    consteval bool require_snap() noexcept
    {
      static_assert(has_flag(InsidePolicy<In>, snap),
          "beman::inside::math: a transcendental result is rounded onto the grid — its "
          "operand must permit rounding. Declare it with `round_nearest` (or "
          "`snap` / a `round_*` mode / `real`).");
      return true;
    }
  }

  // Public irrational constants as POINT-BOUNDS, so they compose directly in
  // inside-space (`angle * math::pi`) with no rational on the surface.
  inline constexpr auto pi     = just<detail::pi_r>;
  inline constexpr auto two_pi = just<detail::two_pi_r>;

  namespace detail
  {
    using namespace beman::inside::detail;

    // Internal turn-phase shape: Q.N turns, period implicit in the unsigned raw's
    // modular wrap. Public sin/cos/tan take radians and route through this shape;
    // callers don't construct it directly (see examples/oscillator.cpp).
    template <int N>
    using turns_t = inside<{0, rational{(imax{1} << N) - 1, imax{1} << N},
                           notch<1, (imax{1} << N)>}>;


    // log2(d) for a power-of-2 imax d. Constexpr loop; cheap at compile time.
    constexpr int log2_pow2(imax d) noexcept
    {
      int n = 0;
      while (d > 1) { d >>= 1; ++n; }
      return n;
    }

    // Extract N from a turns-shaped input inside (notch denominator is 2^N).
    // Alias templates can't be reverse-deduced, so sin/cos take a `insidable In`
    // and derive N from its grid.
    template <insidable In>
    inline constexpr int turn_bits = []{
      static_assert(Lower<In> == 0,
                    "beman::inside::math: turn-phase input must have Lower == 0");
      static_assert(Notch<In>.Numerator == 1,
                    "beman::inside::math: turn-phase input must have notch 1/2^N");
      return log2_pow2(abs_den(Notch<In>.Denominator));
    }();

    // Forward declarations of the CORDIC engine pieces the turn-input workers
    // rely on (the engine is defined below, after the radians sin/cos).
    template <insidable Out> constexpr int working_bits() noexcept;
    template <int W, int N> constexpr rational sin_from_turn_fixed(imax turn_w) noexcept;
    template <int W, int N> constexpr rational cos_from_turn_fixed(imax turn_w) noexcept;
    template <insidable Out> constexpr Out store_grid(rational r);

    // sin (turn-input, internal). Q.N turn-phase → amplitude on `Out`'s grid via
    // the CORDIC engine: rescale the phase to the working scale 2^W and run the
    // shared `sin_from_turn_fixed` reducer.
    template <insidable Out, insidable In>
    [[nodiscard]] constexpr Out sin_turn_impl(In phase)
    {
      constexpr int N = turn_bits<In>;
      static_assert(N >= 2 && N <= 30, "beman::inside::math: turn-phase N must be in [2, 30]");
      static_assert(Lower<Out> <= -1 && Upper<Out> >= 1,
                    "beman::inside::math: Out must cover [-1, 1]");

      constexpr int W = working_bits<Out>();
      imax raw    = raw_imax(phase);                       // Q.N turn
      imax turn_w = (W >= N) ? (raw << (W - N)) : (raw >> (N - W));       // → Q.W
      return store_grid<Out>(sin_from_turn_fixed<W, W>(turn_w));
    }

    // cos (turn-input, internal). cos(x) = sin(x + π/2) — shift the phase
    // by one quarter-turn (modular wrap on the raw) and reuse sin. The
    // shift is integer-exact, no precision cost at this tier.
    template <insidable Out, insidable In>
    [[nodiscard]] constexpr Out cos_turn_impl(In phase)
    {
      constexpr int  N            = turn_bits<In>;
      constexpr imax full_mask    = (imax{1} << N) - 1;
      constexpr imax quarter_turn = imax{1} << (N - 2);

      In shifted = In::from_raw(raw_cast<In>(
          (raw_imax(phase) + quarter_turn) & full_mask));
      return sin_turn_impl<Out>(shifted);
    }

    //=========================================================================
    // Grid-scaled CORDIC engine. Values cross the API as `rational`; internally
    // we work in fixed-point at a scale 2^W chosen from the output grid
    // (`working_bits`), so precision follows the grid. The iteration is pure
    // shift-add (overflow-free); the only multiplies (table/gain derivation,
    // input scaling) use the wide `fmul`, bit-identical on every toolchain.
    //=========================================================================

    // (a·b) >> W via the full 128-bit product, magnitude-truncating (toward zero).
    // The two paths below are bit-for-bit equal by construction.
    constexpr imax fmul(imax a, imax b, int W) noexcept
    {
      bool neg = (a < 0) ^ (b < 0);
      umax ua = (a < 0) ? umax{0} - static_cast<umax>(a) : static_cast<umax>(a);
      umax ub = (b < 0) ? umax{0} - static_cast<umax>(b) : static_cast<umax>(b);
#if defined(__SIZEOF_INT128__)
      // gcc/clang: native 128-bit, constexpr-friendly.
      umax r = static_cast<umax>((static_cast<unsigned __int128>(ua) * ub) >> W);
#else
      // portable (no __int128): 32-bit split → 128-bit (hi:lo) → shift.
      umax al = ua & 0xffffffffu, ah = ua >> 32;
      umax bl = ub & 0xffffffffu, bh = ub >> 32;
      umax ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
      umax mid = (ll >> 32) + (lh & 0xffffffffu) + (hl & 0xffffffffu);
      umax lo  = (ll & 0xffffffffu) | (mid << 32);
      umax hi  = hh + (lh >> 32) + (hl >> 32) + (mid >> 32);
      umax r   = (W == 0) ? lo
               : (W < 64) ? ((lo >> W) | (hi << (64 - W)))
               :            (hi >> (W - 64));
#endif
      return neg ? -static_cast<imax>(r) : static_cast<imax>(r);
    }

    // round(v · 2^W) — scale-W marshalling (parametric Q.W). The product num·2^W
    // is formed at 128 bits, so a reduced numerator near 2^63 cannot wrap.
    // Rounding is half-away-from-zero, matching rational::round().
    constexpr imax to_fixed(rational v, int W) noexcept
    {
      const umax n = v.Numerator;
      const umax d = abs_den(v.Denominator);
      const auto sign = [&](umax q) { return (v.Denominator < 0) ? -static_cast<imax>(q) : static_cast<imax>(q); };
      // Power-of-two denominator (every core result and fixed_to_rational):
      // a shift, with round-half-up as the last shifted-out bit.
      if (std::has_single_bit(d))
      {
        const int D = std::countr_zero(d);
        if (W >= D) return sign(n << (W - D));
        const int sh = D - W;
        return sign((n >> sh) + ((n >> (sh - 1)) & 1u));
      }
      // n·2^W + d/2 below 2^64: one 64-bit divide (d < 2^63).
      if (n < (umax{1} << (63 - W)))
        return sign(((n << W) + d / 2) / d);
#if defined(__SIZEOF_INT128__)
      using u128 = unsigned __int128;
      const u128 t = (u128{n} << W) + d / 2;
      const umax q = static_cast<umax>(t / d);
#else
      // portable: 128-bit (hi:lo) dividend, restoring shift-subtract divide.
      // d < 2^63 (imax denominator), so the partial remainder fits umax.
      umax hi = (W == 0) ? 0 : (n >> (64 - W));
      umax lo = n << W;
      const umax half = d / 2;
      lo += half;
      hi += (lo < half);
      umax q = 0, r = 0;
      for (int i = 127; i >= 0; --i)
      {
        r = (r << 1) | ((i >= 64 ? (hi >> (i - 64)) : (lo >> i)) & 1u);
        q <<= 1;
        if (r >= d) { r -= d; q |= 1; }
      }
#endif
      return sign(q);
    }
    constexpr rational fixed_to_rational(imax x, int W) noexcept
    { return rational{x, imax{1} << W}; }

    // Grids eligible for the GCD-free store: unit-numerator notch, non-rational
    // storage, raw fits imax, assigned with round_nearest. (Unlike the Q-format
    // fast path this does NOT require integer Lower — Lower·K is an exact
    // integer by the grid invariant regardless.) The math results all carry a
    // power-of-two denominator, so the value index is formed with integer
    // shifts, rounded half away from zero — the same rule as the rational
    // assignment path (round_quotient), minus `(value−Lower)/Notch`'s GCDs.
    template <insidable Out>
    inline constexpr bool grid_fast_store =
        Notch<Out>.Numerator == 1
        && !rational_raw<Out>
        // `real` storage holds the VALUE, not an offset index, so route it
        // through the rational fallback `Out{r}` (same guard as fmod_int_fast).
        && !fp_raw<Out>
        && rounding_of(InsidePolicy<Out>) == round_mode::nearest
        && (std::signed_integral<raw_t<Out>>
            || NotchCount<Out>
                 <= static_cast<umax>(std::numeric_limits<imax>::max()));

    // Store a power-of-two-denominator result (the shape every core returns)
    // onto Out's grid. Fast path: pure integer. Fallback: the general rational
    // assignment (handles non-fast grids, clamp/wrap on out-of-range, etc).
    template <insidable Out>
    constexpr Out store_grid(rational r)
    {
      if constexpr (grid_fast_store<Out>)
      {
        umax den = abs_den(r.Denominator);
        if ((den & (den - 1)) == 0)                       // power-of-two denom
        {
          int  D   = std::countr_zero(den);
          imax num = signed_numerator(r);
          constexpr imax K = abs_den(Notch<Out>.Denominator);  // 1/notch
          constexpr imax m = trunc((Lower<Out> * rational{K}).value()); // Lower·K (exact int)
          // K·num + half must fit imax (a wide-denominator r, e.g. hypot's
          // 2^46, would wrap K·num and silently store `value mod 2^k`).
          constexpr imax lim = std::numeric_limits<imax>::max() / 2 / K;
          // Range-check the exact value first, like assignment: x = value·K·2^D
          // must lie in [m, m + max index]·2^D (floor / ceil via arithmetic shifts).
          if (-lim <= num && num <= lim
              && ((K * num) >> D) >= m
              && -((-(K * num)) >> D) <= m + static_cast<imax>(NotchCount<Out>))
          {
            // value index round(value·K), ties half away from zero like the
            // assignment path: round the magnitude, then restore the sign.
            const imax half = (D > 0) ? (imax{1} << (D - 1)) : 0;
            const imax x    = K * num;
            const imax idx  = x >= 0 ? (x + half) >> D : -((-x + half) >> D);
            const imax off  = idx - m;
            return Out::from_raw(raw_from_offset<Out>(static_cast<umax>(off)));
          }
        }
      }
      return Out{r};
    }

    // Working scale for an output grid: fractional bits to resolve the notch,
    // plus integer bits of the largest output magnitude (error ~V·2^-W, so large
    // outputs like pow's 10^k need headroom), plus CORDIC guard bits. Capped at 31.
    template <insidable Out>
    constexpr int working_bits() noexcept
    {
      constexpr int GUARD = 6;
      umax den        = abs_den(Notch<Out>.Denominator);   // 1/notch
      int  notch_bits = (den <= 1) ? 0 : std::bit_width(den - 1);
      imax hi  = ceil(abs(Upper<Out>));
      imax lo  = ceil(abs(Lower<Out>));
      imax mag = (hi > lo) ? hi : lo;
      int  int_bits = (mag <= 1) ? 0 : std::bit_width(static_cast<umax>(mag));
      int  W = notch_bits + int_bits + GUARD;
      return (W < 12) ? 12 : (W > 31) ? 31 : W;
    }

    // Working scale for the composed endpoint functions (asin, tanh, log10,
    // cbrt, ...): several fixed-point stages each add error, so they need more
    // guard bits than one CORDIC pass; capped at the reference scale.
    inline constexpr int kEndpointGuard = 4;
    template <insidable Out>
    constexpr int endpoint_bits() noexcept
    {
      constexpr int W = working_bits<Out>() + kEndpointGuard;
      return W < 30 ? W : 30;
    }

    // atan(2^-i) in RADIANS at scale 2^W. i=0 is π/4 (exact, from pi_r); i≥1
    // uses the fast-converging series atan(z)=z−z³/3+z⁵/5−… for tiny z=2^-i.
    constexpr imax atan_pow2_fixed(int i, int W) noexcept
    {
      if (i == 0)
        return to_fixed(pi_r / 4, W);
      if (W - i < 1) return 0;
      imax z = imax{1} << (W - i);
      imax z2 = fmul(z, z, W), term = z, acc = 0;
      for (int k = 0; k < 64; ++k) {
        imax t = term / (2 * k + 1);
        acc += (k & 1) ? -t : t;
        if (z2 == 0) break;
        term = fmul(term, z2, W);
        if (term == 0) break;
      }
      return acc;
    }

    // 1/sqrt(a) at scale 2^W for a ∈ [1,2], division-free Newton (y←y(3−ay²)/2)
    // from y = 1, `iters` steps. Compile-time use (CORDIC gains, the seed table);
    // the runtime sqrt path seeds from a table instead (rsqrt_seeded).
    constexpr imax rsqrt_fixed(imax a, int W, int iters) noexcept
    {
      imax one = imax{1} << W, three = 3 * one, y = one;
      for (int k = 0; k < iters; ++k) {
        imax ay2 = fmul(a, fmul(y, y, W), W);
        y = fmul(y, three - ay2, W) >> 1;
      }
      return y;
    }

    // 1/√m for m ∈ [1, 2) at scale 2^30, sampled at the midpoints of 16 cells
    // (top 4 fraction bits of m): ≤ 1.1% error, a ~6.6-bit Newton start.
    inline constexpr auto rsqrt_seed_tbl = []{
      std::array<imax, 16> t{};
      for (int i = 0; i < 16; ++i)
      {
        const imax m_mid = (imax{1} << 30) + (((2 * imax{i} + 1)) << 25);   // 1 + (i+½)/16
        t[static_cast<std::size_t>(i)] = rsqrt_fixed(m_mid, 30, 12);
      }
      return t;
    }();

    // 1/√m at scale 2^W for m·2^W ∈ [2^W, 2^(W+1)): table seed, then Newton
    // (y ← y(3−my²)/2, error squares each step): 2 steps reach ~24 bits, 3 ~47.
    template <int W>
    constexpr imax rsqrt_seeded(imax m_w) noexcept
    {
      static_assert(W >= 4 && W <= 31);
      constexpr int iters = (W <= 20) ? 2 : 3;
      const imax seed = rsqrt_seed_tbl[static_cast<std::size_t>((m_w >> (W - 4)) & 15)];
      imax y = (W <= 30) ? (seed >> (30 - W)) : (seed << (W - 30));
      constexpr imax three = imax{3} << W;
      for (int k = 0; k < iters; ++k)
        y = fmul(y, three - fmul(m_w, fmul(y, y, W), W), W) >> 1;
      return y;
    }

    // √2 as a rational (literal source, like pi_r / ln2_r), for sqrt's odd-
    // exponent step.
    inline constexpr rational sqrt2_r{1414213562, 1000000000};

    // √a at scale 2^W, a_w = a·2^W ≥ 0. Reduce a = m·2^e, m ∈ [1,2);
    // √a = √m · 2^(e/2), √m = m·(1/√m) via rsqrt; odd e multiplies in √2.
    // Templated on W so the √2 constant and iteration count fold at compile time.
    template <int W>
    constexpr imax sqrt_fixed(imax a_w) noexcept
    {
      if (a_w <= 0) return 0;
      int  lead = 63 - std::countl_zero(static_cast<umax>(a_w));
      int  e    = lead - W;
      imax m_w  = (e >= 0) ? (a_w >> e) : (a_w << (-e));    // m·2^W ∈ [2^W, 2^(W+1))
      imax sm   = fmul(m_w, rsqrt_seeded<W>(m_w), W);                     // √m · 2^W
      if (e & 1) { constexpr imax sqrt2_w = to_fixed(sqrt2_r, W); sm = fmul(sm, sqrt2_w, W); }
      int h = e >> 1;                                       // floor(e/2)
      return (h >= 0) ? (sm << h) : (sm >> (-h));
    }

    // CORDIC circular gain 1/K = ∏ 1/√(1+4^-i) at scale 2^W.
    constexpr imax cordic_invgain(int W, int N) noexcept
    {
      imax one = imax{1} << W, invK = one;
      for (int i = 0; i < N; ++i) {
        if (2 * i > W - 1) break;
        invK = fmul(invK, rsqrt_fixed(one + (one >> (2 * i)), W, 12), W);
      }
      return invK;
    }

    // Per-<W,N> compile-time atan table + prescaled gain (one per instantiation).
    template <int W, int N>
    inline constexpr auto cordic_atan_tbl = []{
      std::array<imax, static_cast<std::size_t>(N)> t{};
      for (int i = 0; i < N; ++i) t[i] = atan_pow2_fixed(i, W);
      return t;
    }();
    template <int W, int N>
    inline constexpr imax cordic_invgain_v = cordic_invgain(W, N);

    // Circular rotation: sin/cos of z (radians at scale 2^W, |z| ≤ ~π/2).
    template <int W, int N>
    constexpr void cordic_sincos(imax z, imax& sin_out, imax& cos_out) noexcept
    {
      imax x = cordic_invgain_v<W, N>, y = 0;
      for (int i = 0; i < N; ++i) {
        imax d  = (z >= 0) ? 1 : -1;
        imax xn = x - d * (y >> i);
        imax yn = y + d * (x >> i);
        z -= d * cordic_atan_tbl<W, N>[i];
        x = xn; y = yn;
      }
      sin_out = y; cos_out = x;
    }

    // sin(x) as a rational, x given as a Q.W turn-phase (one turn = 2^W). Reduces
    // to the first quadrant in turns (exact powers of two), then CORDICs the
    // residual converted to radians. cos = sin(+¼ turn).
    template <int W, int N>
    constexpr rational sin_from_turn_fixed(imax turn_w) noexcept
    {
      imax one_turn = imax{1} << W, half = imax{1} << (W - 1), quarter = imax{1} << (W - 2);
      turn_w &= (one_turn - 1);                      // wrap into [0,1) turn
      bool flip = (turn_w & half) != 0;
      turn_w &= (half - 1);
      if (turn_w > quarter) turn_w = half - turn_w;  // reflect about π/4
      // Exact zero at multiples of a half-turn: CORDIC leaves a ~1-ULP residual
      // at angle 0, but sin(kπ) must be exactly 0 (pole detection in tan relies
      // on it). Quadrant peaks (turn_w == quarter) round to ±1 on the grid.
      if (turn_w == 0) return rational{0};
      // Bound as constexpr so the 128-bit divide inside to_fixed is guaranteed
      // compile-time (same pattern as sqrt2_w) — args are all constants.
      constexpr imax two_pi_w = to_fixed(two_pi_r, W);
      imax rad = fmul(turn_w, two_pi_w, W);
      imax s, c;
      cordic_sincos<W, N>(rad, s, c);
      return fixed_to_rational(flip ? -s : s, W);
    }
    template <int W, int N>
    constexpr rational cos_from_turn_fixed(imax turn_w) noexcept
    { return sin_from_turn_fixed<W, N>(turn_w + (imax{1} << (W - 2))); }

    // tan(x) for a Q.W turn-phase as a Q.W fixed-point value, from ONE rotation:
    // reduce to quadrant q and r ∈ [0, ¼ turn], take sin r / cos r from a single
    // cordic_sincos pass, and divide in fixed point. Exact zeros at r == 0 and
    // r == ¼ turn keep the poles exact. Returns false on a pole.
    template <int W, int N>
    constexpr bool tan_from_turn_fixed(imax turn_w, imax& tan_w) noexcept
    {
      constexpr imax one_turn = imax{1} << W, quarter = imax{1} << (W - 2);
      turn_w &= (one_turn - 1);                        // wrap into [0, 1) turn
      const imax q = turn_w / quarter;                 // quadrant 0..3
      const imax r = turn_w - q * quarter;             // [0, ¼ turn)
      imax s = 0, c = imax{1} << W;                    // r == 0: sin 0, cos 1
      if (r != 0)
      {
        constexpr imax two_pi_w = to_fixed(two_pi_r, W);
        cordic_sincos<W, N>(fmul(r, two_pi_w, W), s, c);
      }
      // tan = sin/cos per quadrant: q0 s/c, q1 −c/s, q2 s/c, q3 −c/s.
      const imax num = (q & 1) ? -c : s;
      const imax den = (q & 1) ?  s : c;
      if (den == 0) return false;                      // pole (q odd, r == 0)
      // round-half-away((num << W) / den); |num| ≤ 2^W so num·2^W fits imax.
      const imax n = num * (imax{1} << W);
      const imax ad = den < 0 ? -den : den;
      const imax an = n < 0 ? -n : n;
      const imax mag = (an + ad / 2) / ad;
      tan_w = ((n < 0) != (den < 0)) ? -mag : mag;
      return true;
    }

    // 1/(2π) as a rational, for radians→turn reduction at any scale.
    inline constexpr rational inv_two_pi =
      (rational{1} / two_pi_r).value();

    // radians → turn at scale 2^W. The single-term product's error (~2^-(W+1))
    // scales with |a|, capping the envelope at ±1024 rad — grids within it keep
    // that expression verbatim (bit-identical). Wider grids (up to ±2^20 rad) use
    // a two-term hi+lo split of 1/2π (lo carried at scale 2^(W+24)) to recover
    // ~2^-W turn accuracy, combined through the 128-bit fmul.
    template <int W, insidable In>
    constexpr imax rad_to_turn_w(rational a) noexcept
    {
      const imax a_w = to_fixed(a, W);
      if constexpr (Lower<In> >= -1024 && Upper<In> <= 1024)
      {
        constexpr imax inv_two_pi_w = to_fixed(inv_two_pi, W);
        return fmul(a_w, inv_two_pi_w, W);
      }
      else
      {
        constexpr int  S    = W + 24;                 // ≤ 55 for W ≤ 31
        constexpr imax hi_w = to_fixed(inv_two_pi, W);
        constexpr rational lo = inv_two_pi - fixed_to_rational(hi_w, W);
        constexpr imax lo_s = to_fixed(lo, S);
        return fmul(a_w, hi_w, W) + fmul(a_w, lo_s, S);
      }
    }

    // CORDIC circular vectoring: atan2(y, x) in RADIANS at scale 2^W. Pre: x > 0
    // (caller pre-rotates other quadrants). Rotates (x, y) toward the +x axis,
    // accumulating the atan table; the gain cancels in y/x, so no prescale.
    template <int W, int N>
    constexpr imax cordic_atan2_rad(imax y, imax x) noexcept
    {
      imax z = 0;
      for (int i = 0; i < N; ++i) {
        imax dx = y >> i, dy = x >> i;
        if (y >= 0) { x += dx; y -= dy; z += cordic_atan_tbl<W, N>[i]; }
        else        { x -= dx; y += dy; z -= cordic_atan_tbl<W, N>[i]; }
      }
      return z;
    }

    //----- hyperbolic CORDIC (exp via sinh+cosh, ln via atanh-vectoring) ------

    // Reference precision for compile-time interval derivation and composed
    // endpoints (sinh/cosh/tanh/log10/cbrt/pow). Runtime impls use working_bits<Out>
    // so the value still follows the grid; this only bounds the derived intervals.
    inline constexpr int kRefBits = 30;

    // ln 2 as a rational (10-digit literal), plus its reciprocal — for exp/log
    // range reduction and base changes.
    inline constexpr rational ln2_r{6931471806, 10000000000};
    inline constexpr rational inv_ln2_r = (rational{1} / ln2_r).value();

    // atanh(2^-i) at scale 2^W (series; 2^-i ≤ ½ ⇒ converges). i ≥ 1 only.
    constexpr imax atanh_pow2_fixed(int i, int W) noexcept
    {
      if (W - i < 1) return 0;
      imax z = imax{1} << (W - i);
      imax z2 = fmul(z, z, W), term = z, acc = 0;
      for (int k = 0; k < 64; ++k) {
        acc += term / (2 * k + 1);
        if (z2 == 0) break;
        term = fmul(term, z2, W);
        if (term == 0) break;
      }
      return acc;
    }

    // Hyperbolic CORDIC shift schedule with the convergence repeats at
    // i = 4, 13, 40, … (each 3·prev+1). Length L covers W + guard distinct bits.
    template <int L>
    constexpr std::array<int, static_cast<std::size_t>(L)> hyp_seq() noexcept
    {
      std::array<int, static_cast<std::size_t>(L)> s{};
      int idx = 0, i = 1, rep = 4;
      while (idx < L) {
        s[idx++] = i;
        if (i == rep && idx < L) { s[idx++] = i; rep = 3 * rep + 1; }
        ++i;
      }
      return s;
    }

    constexpr int hyp_len(int W) noexcept { return W + 6; }

    template <int W, int L>
    inline constexpr auto cordic_atanh_tbl = []{
      constexpr auto seq = hyp_seq<L>();
      std::array<imax, static_cast<std::size_t>(L)> t{};
      for (int j = 0; j < L; ++j)
        t[j] = atanh_pow2_fixed(seq[j], W);
      return t;
    }();

    // Hyperbolic gain 1/Kh = ∏ 1/√(1−4^-i) over the schedule, at scale 2^W.
    template <int W, int L>
    inline constexpr imax cordic_hyp_invgain_v = []{
      constexpr auto seq = hyp_seq<L>();
      imax one = imax{1} << W, invK = one;
      for (int j = 0; j < L; ++j) {
        int i = seq[j];
        if (2 * i > W - 1) continue;
        invK = fmul(invK, rsqrt_fixed(one - (one >> (2 * i)), W, 12), W);   // ×1/√(1−4^-i)
      }
      return invK;
    }();

    // Rotation: sinh/cosh of z (scale 2^W, |z| ≤ ~1.11). exp(z) = sinh+cosh.
    template <int W, int L>
    constexpr void cordic_sinhcosh(imax z, imax& sh, imax& ch) noexcept
    {
      constexpr auto seq = hyp_seq<L>();
      imax x = cordic_hyp_invgain_v<W, L>, y = 0;
      for (int j = 0; j < L; ++j) {
        int i = seq[j];
        imax d  = (z >= 0) ? 1 : -1;
        imax xn = x + d * (y >> i);
        imax yn = y + d * (x >> i);
        z -= d * cordic_atanh_tbl<W, L>[j];
        x = xn; y = yn;
      }
      sh = y; ch = x;
    }

    // Vectoring: atanh(y/x) at scale 2^W (drives y → 0). ln(m) = 2·atanh((m−1)/(m+1)).
    template <int W, int L>
    constexpr imax cordic_atanh_vec(imax x, imax y) noexcept
    {
      constexpr auto seq = hyp_seq<L>();
      imax z = 0;
      for (int j = 0; j < L; ++j) {
        int i = seq[j];
        imax d  = (y < 0) ? 1 : -1;
        imax xn = x + d * (y >> i);
        imax yn = y + d * (x >> i);
        z -= d * cordic_atanh_tbl<W, L>[j];
        x = xn; y = yn;
      }
      return z;
    }

    // 2^(x_w / 2^W) as a rational, x_w a fixed-point exponent at scale 2^W.
    // Split x = k + f (k integer, f ∈ [−½,½]); 2^x = 2^k · e^(f·ln2). The 2^k
    // lives in the rational's power-of-two num/den so large |x| never overflows.
    // Pure fixed-point — composing this from a log result (pow/cbrt) never
    // stacks rational denominators.
    template <int W>
    constexpr rational exp2_from_fixed(imax x_w) noexcept
    {
      imax k   = (x_w + (imax{1} << (W - 1))) >> W;        // round to nearest int
      imax f_w = x_w - (k << W);                            // ∈ [−2^(W−1), 2^(W−1)]
      constexpr imax ln2_w = to_fixed(ln2_r, W);            // compile-time constant
      imax fr_w = fmul(f_w, ln2_w, W);                      // f·ln2 (natural)
      imax er_w;
      if (fr_w == 0) er_w = imax{1} << W;                   // 2^k exactly
      else { imax sh, ch; cordic_sinhcosh<W, hyp_len(W)>(fr_w, sh, ch); er_w = sh + ch; }
      if (k <= W) return rational{static_cast<umax>(er_w), imax{1} << (W - k)};
      return rational{static_cast<umax>(er_w) << (k - W), 1};
    }

    // ln(w) at scale 2^W as a fixed-point imax. Leading-bit reduce w = 2^e·m,
    // m ∈ [1,2); ln(w) = e·ln2 + 2·atanh((m−1)/(m+1)). Pre: w > 0.
    template <int W>
    constexpr imax ln_to_fixed(rational w) noexcept
    {
      imax w_w  = to_fixed(w, W);
      int  lead = 63 - std::countl_zero(static_cast<umax>(w_w));
      int  e    = lead - W;
      imax one  = imax{1} << W;
      imax m_w  = (e >= 0) ? (w_w >> e) : (w_w << (-e));   // m·2^W ∈ [2^W, 2^(W+1))
      imax z    = cordic_atanh_vec<W, hyp_len(W)>(m_w + one, m_w - one);
      constexpr imax ln2_w = to_fixed(ln2_r, W);           // compile-time constant
      return e * ln2_w + 2 * z;
    }

    // log2(x) at scale 2^W as imax: ln(x)·log2(e).
    template <int W>
    constexpr imax log2_to_fixed(rational x) noexcept
    {
      constexpr imax inv_ln2_w = to_fixed(inv_ln2_r, W);   // compile-time constant
      return fmul(ln_to_fixed<W>(x), inv_ln2_w, W);
    }

    // e^(v_w / 2^W) as a rational: 2^(v·log2 e).
    template <int W>
    constexpr rational exp_from_fixed(imax v_w) noexcept
    {
      constexpr imax inv_ln2_w = to_fixed(inv_ln2_r, W);   // compile-time constant
      return exp2_from_fixed<W>(fmul(v_w, inv_ln2_w, W));
    }

    // Rational-input wrappers (inputs are small-denominator values; fine to
    // marshal through to_fixed). pow/cbrt compose via the *_fixed primitives
    // above instead, to avoid rational-denominator blow-up.
    template <int W>
    constexpr rational exp_fixed(rational v) noexcept
    { return exp_from_fixed<W>(to_fixed(v, W)); }
    template <int W>
    constexpr rational ln_fixed(rational w) noexcept
    { return fixed_to_rational(ln_to_fixed<W>(w), W); }
    template <int W>
    constexpr rational exp2_fixed(rational x) noexcept
    { return exp2_from_fixed<W>(to_fixed(x, W)); }
    template <int W>
    constexpr rational log2_fixed(rational x) noexcept
    { return fixed_to_rational(log2_to_fixed<W>(x), W); }

    // 1/ln10 at scale 2^W — for log10 = ln·(1/ln10), composed in fixed-point.
    template <int W>
    constexpr imax inv_ln10_fixed() noexcept
    {
      // ln10 ≈ 2.302585 (small, no overflow): derive the rational once, marshal.
      return to_fixed((rational{1} /
                       fixed_to_rational(ln_to_fixed<W>(rational{10}), W)).value(), W);
    }
  } // namespace detail

  // sin: radians-valued inside → amplitude on the auto-deduced output grid.
  // Converts to a turn (× 1/(2π)) at the grid-derived working scale, then runs
  // the circular-CORDIC reducer. Inputs up to |angle| ≤ 2^20 rad (see
  // rad_to_turn_w for the reduction split beyond ±1024).
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out sin_impl(In angle)
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "beman::inside::math::sin: input magnitudes must be \u2264 2^20 rad");
    static_assert(Lower<Out> <= -1 && Upper<Out> >= 1,
                  "beman::inside::math::sin: Out must cover [-1, 1]");

    constexpr int W = detail::working_bits<Out>();
    imax turn_w = detail::rad_to_turn_w<W, In>(angle);
    return detail::store_grid<Out>(detail::sin_from_turn_fixed<W, W>(turn_w));
  }

  // cos: radians-valued inside → amplitude. cos(x) = sin(x + π/2) — add a
  // quarter-turn before the quadrant reducer, same precision as sin.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out cos_impl(In angle)
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "beman::inside::math::cos: input magnitudes must be \u2264 2^20 rad");
    static_assert(Lower<Out> <= -1 && Upper<Out> >= 1,
                  "beman::inside::math::cos: Out must cover [-1, 1]");

    constexpr int W = detail::working_bits<Out>();
    imax turn_w = detail::rad_to_turn_w<W, In>(angle);
    return detail::store_grid<Out>(detail::cos_from_turn_fixed<W, W>(turn_w));
  }

  namespace detail
  {
    using namespace beman::inside::detail;

    // Shared tail of tan_impl / tan_turn_impl: pole → division_by_zero; outside
    // Out → overflow (a clamp Out saturates in the store instead).
    template <insidable Out, int W>
    constexpr std::expected<Out, errc> tan_store(imax turn_w)
    {
      imax t_w;
      if (!tan_from_turn_fixed<W, W>(turn_w, t_w))
        return std::unexpected(errc::division_by_zero);
      if constexpr (!has_flag(InsidePolicy<Out>, clamp))
      {
        // t_w/2^W ∈ [lo, hi]  ⇔  ⌈lo·2^W⌉ ≤ t_w ≤ ⌊hi·2^W⌋
        constexpr imax lo_w = ceil ((Lower<Out> * rational{imax{1} << W}).value());
        constexpr imax hi_w = floor((Upper<Out> * rational{imax{1} << W}).value());
        if (t_w < lo_w || t_w > hi_w)
          return std::unexpected(errc::overflow);
      }
      return store_grid<Out>(fixed_to_rational(t_w, W));
    }

    // tan (turn-input, internal). sin/cos from the grid-scaled engine, divided
    // with a pole guard. Returns `unexpected(errc::division_by_zero)` when the
    // phase lands on a pole (cos == 0) and `unexpected(errc::overflow)` when the
    // result exceeds Out's range.
    template <insidable Out, insidable In>
    [[nodiscard]] constexpr std::expected<Out, errc> tan_turn_impl(In phase)
    {
      constexpr int N = turn_bits<In>;
      static_assert(N >= 2 && N <= 30, "beman::inside::math: turn-phase N must be in [2, 30]");

      constexpr int W = working_bits<Out>();
      imax raw    = raw_imax(phase);
      imax turn_w = (W >= N) ? (raw << (W - N)) : (raw >> (N - W));

      return tan_store<Out, W>(turn_w);
    }
  } // namespace detail

  // tan: radians-valued inside → amplitude, with pole guard. sin/cos from the
  // radians input, divided. Returns `unexpected(division_by_zero)` if cos rounds
  // to 0 (input on a pole), `unexpected(overflow)` if the result exceeds Out.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr std::expected<Out, errc> tan_impl(In angle)
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "beman::inside::math::tan: input magnitudes must be \u2264 2^20 rad");

    constexpr int W = detail::working_bits<Out>();
    imax turn_w = detail::rad_to_turn_w<W, In>(angle);

    return detail::tan_store<Out, W>(turn_w);
  }


  // log2: positive inside → inside. log2(x) = ln(x)·log2(e) via the grid-scaled
  // hyperbolic-CORDIC `log2_fixed` core (leading-bit reduction + atanh vectoring).
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out log2_impl(In x)
  {
    static_assert(Lower<In> > 0,
                  "beman::inside::math::log2: input must be strictly positive");

    return detail::store_grid<Out>(detail::log2_fixed<detail::working_bits<Out>()>(rational{x}));
  }

  // exp2: inside → inside, returning 2^x. 2^x = e^(x·ln2) via the grid-scaled
  // hyperbolic-CORDIC `exp2_fixed` core (integer/fractional split + sinh/cosh).
  //
  // Restrict |x| ≤ 30 so the rational denominator 2^(30 - k) fits in int63.
  // The output `Out` must include non-negative values and cover at least
  // [2^Lower<In>, 2^Upper<In>] — anything narrower needs `clamp` to absorb
  // overflow at the assignment.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out exp2_impl(In x)
  {
    static_assert(Lower<In> >= -30 && Upper<In> <= 30,
                  "beman::inside::math::exp2: input must be in [-30, 30]");
    static_assert(Lower<Out> >= 0,
                  "beman::inside::math::exp2: Out must be non-negative");

    return detail::store_grid<Out>(detail::exp2_fixed<detail::working_bits<Out>()>(rational{x}));
  }

  // exp: thin wrapper. exp(x) = exp2(x · log2(e)). The scaling factor
  // log2(e) ≈ 1.4427, so x must stay inside [-30/log2(e), 30/log2(e)] ≈
  // [-20.79, 20.79] for exp2's denominator-shift envelope. We use [-20, 20].
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out exp_impl(In x)
  {
    static_assert(Lower<In> >= -20 && Upper<In> <= 20,
                  "beman::inside::math::exp: input must be in [-20, 20]");
    static_assert(Lower<Out> >= 0,
                  "beman::inside::math::exp: Out must be non-negative");

    return detail::store_grid<Out>(detail::exp_fixed<detail::working_bits<Out>()>(rational{x}));
  }

  // log: thin wrapper. log(x) = log2(x) · ln(2). Result precision matches
  // log2 minus 1-2 ULP from the final fixed-point scaling.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out log_impl(In x)
  {
    static_assert(Lower<In> > 0,
                  "beman::inside::math::log: input must be strictly positive");

    return detail::store_grid<Out>(detail::ln_fixed<detail::working_bits<Out>()>(rational{x}));
  }

  // pow_base<Base>(x) = Base^x for compile-time-known integer Base ≥ 2.
  // Implemented as exp2(x · log2(Base)) with log2(Base) from the grid-scaled
  // `log2_to_fixed` core — no hand-typed magic constants.
  // For Base = 10, this is the building block for `db_to_linear`.
  template <imax Base, insidable Out, insidable In>
  [[nodiscard]] constexpr Out pow_base_impl(In x)
  {
    static_assert(Base >= 2, "beman::inside::math::pow_base: Base must be ≥ 2");
    static_assert(Lower<Out> >= 0,
                  "beman::inside::math::pow_base: Out must be non-negative");

    constexpr int W = detail::working_bits<Out>();
    constexpr imax lb_w = detail::log2_to_fixed<W>(rational{Base});   // log2(Base)·2^W
    imax sc_w = detail::fmul(detail::to_fixed(rational{x}, W), lb_w, W);
    return detail::store_grid<Out>(detail::exp2_from_fixed<W>(sc_w));
  }


  // atan2: signed inside, signed inside → radians ∈ [-π, π], via CORDIC vectoring
  // with quadrant pre-rotation. CORDIC depends only on y/x, so inputs beyond
  // magnitude 1 are normalized by the larger magnitude (exact rational division);
  // inputs already in [-1, 1] skip it.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out atan2_impl(In y, In x)
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "beman::inside::math::atan2: input magnitudes must be \u2264 2^20 for the working-scale envelope");
    static_assert(Lower<Out> <= -detail::pi_r && Upper<Out> >= detail::pi_r,
                  "beman::inside::math::atan2: Out must cover [-π, π]");

    constexpr int W = detail::working_bits<Out>();
    rational yv = y, xv = x;
    {
      rational ay = beman::inside::detail::abs(yv);
      rational ax = beman::inside::detail::abs(xv);
      rational m  = (ax > ay) ? ax : ay;
      if (m > rational{1})
      {
        yv = yv / m;
        xv = xv / m;
      }
    }
    imax y_w = detail::to_fixed(yv, W);
    imax x_w = detail::to_fixed(xv, W);

    // atan2(0, 0) is undefined; convention is 0. Without the guard CORDIC
    // accumulates the angle table on zero x,y and produces garbage.
    if (x_w == 0 && y_w == 0) return Out{0};

    // Quadrant pre-rotation: CORDIC requires x > 0. For x < 0, rotate the
    // vector by ±π/2 (in radians, at scale W) to land in the right half-plane
    // and add the rotation back at the end.
    //   Q2 (x<0, y≥0): (x',y') = (y, −x),  θ = CORDIC + π/2.
    //   Q3 (x<0, y<0): (x',y') = (−y, x),  θ = CORDIC − π/2.
    constexpr imax half_pi_w = detail::to_fixed(detail::pi_r / 2, W);
    imax pre_rotation = 0;
    if (x_w < 0) {
      if (y_w >= 0) { imax nx = y_w;  imax ny = -x_w; x_w = nx; y_w = ny; pre_rotation =  half_pi_w; }
      else          { imax nx = -y_w; imax ny =  x_w; x_w = nx; y_w = ny; pre_rotation = -half_pi_w; }
    }

    imax rad = detail::cordic_atan2_rad<W, W>(y_w, x_w) + pre_rotation;   // radians, scale W
    return detail::store_grid<Out>(detail::fixed_to_rational(rad, W));
  }

  namespace detail
  {
    // max(|Lower<In>|, |Upper<In>|) as a constexpr rational. Used to size
    // the auto-deduced abs output.
    template <insidable In>
    inline constexpr rational abs_auto_upper =
      (abs(Lower<In>) > abs(Upper<In>))
        ? abs(Lower<In>) : abs(Upper<In>);

    template <insidable In>
    using abs_auto_t = inside<{{rational{0}, abs_auto_upper<In>},
                              Notch<In>}, out_policy<In>>;

    template <insidable In>
    using floor_auto_t = inside<{{rational{floor(Lower<In>)},
                                  rational{floor(Upper<In>)}},
                                 notch<1>}, out_policy<In>>;

    template <insidable In>
    using ceil_auto_t = inside<{{rational{ceil(Lower<In>)},
                                 rational{ceil(Upper<In>)}},
                                notch<1>}, out_policy<In>>;

    template <insidable In>
    using round_auto_t = inside<{{rational{round(Lower<In>)},
                                  rational{round(Upper<In>)}},
                                 notch<1>}, out_policy<In>>;

    template <insidable In>
    using trunc_auto_t = inside<{{rational{trunc(Lower<In>)},
                                  rational{trunc(Upper<In>)}},
                                 notch<1>}, out_policy<In>>;

    // Double-backed fast path for the algebraic tier. |x| and the integer
    // roundings of a grid value are exact in double (|x| < 2^53 on a
    // double_exact grid, so the imax cast cannot overflow), and the
    // auto-deduced Out holds every result by construction, so the result is
    // stored as the raw without the rational round-trip.
    template <insidable Out, insidable AutoOut, insidable In>
    inline constexpr bool fp_direct =
        std::same_as<Out, AutoOut> && fp_raw<In> && fp_raw<Out>;

    template <insidable Out, insidable In, typename F>
    constexpr Out fp_direct_store(In x, F f) noexcept
    { return Out::from_raw(raw_cast<Out>(f(static_cast<double>(x.raw())))); }

    constexpr double fp_trunc(double v) noexcept { return static_cast<double>(static_cast<imax>(v)); }
    constexpr double fp_floor(double v) noexcept { const double t = fp_trunc(v); return t > v ? t - 1 : t; }
    constexpr double fp_ceil (double v) noexcept { const double t = fp_trunc(v); return t < v ? t + 1 : t; }
    constexpr double fp_round(double v) noexcept   // half away from zero, like rational round()
    {
      const double t = fp_trunc(v), f = v - t;     // exact: v and t share the grid
      return f >= 0.5 ? t + 1 : f <= -0.5 ? t - 1 : t;
    }
  }

  //---------------------------------------------------------------------------
  // Algebraic tier — exact, no polynomial machinery. Each function wraps the
  // corresponding `rational` operation and routes through `Out`'s assignment.
  //---------------------------------------------------------------------------

  // |x|. Output Lower must be ≥ 0 (the result is always non-negative).
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out abs_impl(In x)
  {
    static_assert(Lower<Out> <= 0,
                  "beman::inside::math::abs: Out must include 0");
    if constexpr (detail::fp_direct<Out, detail::abs_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, [](double v) { return v < 0 ? -v : v; });
    else
      return detail::store_grid<Out>(beman::inside::detail::abs(rational{x}));
  }

  // ⌊x⌋ — largest integer ≤ x.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out floor_impl(In x)
  {
    if constexpr (detail::fp_direct<Out, detail::floor_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, detail::fp_floor);
    else
      return detail::store_grid<Out>(floor(rational{x}));
  }

  // ⌈x⌉ — smallest integer ≥ x.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out ceil_impl(In x)
  {
    if constexpr (detail::fp_direct<Out, detail::ceil_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, detail::fp_ceil);
    else
      return detail::store_grid<Out>(ceil(rational{x}));
  }

  // x rounded to nearest integer, half-away-from-zero (matches the existing
  // `rational::round()` convention used throughout the library).
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out round_impl(In x)
  {
    if constexpr (detail::fp_direct<Out, detail::round_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, detail::fp_round);
    else
      return detail::store_grid<Out>(round(rational{x}));
  }

  // x truncated toward zero. Distinct from floor for negative inputs:
  // trunc(-1.7) = -1 vs floor(-1.7) = -2.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out trunc_impl(In x)
  {
    if constexpr (detail::fp_direct<Out, detail::trunc_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, detail::fp_trunc);
    else
      return detail::store_grid<Out>(trunc(rational{x}));
  }

  namespace detail
  {
    using namespace beman::inside::detail;

    // Gate for fmod's integer fast path. When both operands and Out are
    // integer-backed on commensurable notches, fmod collapses to ONE integer
    // remainder in units of g = gcd(Notch<InX>, Notch<InY>): with x = a·g and
    // y = b·g, x − trunc(x/y)·y = (a − (a/b)·b)·g = (a % b)·g exactly (C++ %
    // is truncated division, the same convention). Conditions:
    //   * integer raws only (rational/double raws keep the rational path),
    //   * non-zero notches, g on Out's grid (g / Notch<Out> integer),
    //   * divisor grid excludes zero (no runtime zero check needed),
    //   * Out's interval covers ±max|y| (result magnitude is < |y|),
    //   * all unit counts fit comfortably in imax (headroom 4).
    template <insidable Out, insidable InX, insidable InY>
    inline constexpr bool fmod_int_fast = []{
      if (rational_raw<InX> || fp_raw<InX>
       || rational_raw<InY> || fp_raw<InY>
       || rational_raw<Out> || fp_raw<Out>)
        return false;
      if (Notch<InX> == 0 || Notch<InY> == 0 || Notch<Out> == 0)
        return false;
      if (!DivisorExcludesZero<InY>)
        return false;
      auto go = gcd(Notch<InX>, Notch<InY>);
      if (!go.has_value()) return false;
      rational g = *go;
      auto qo = g / Notch<Out>;
      if (!qo.has_value() || abs_den(qo->Denominator) != 1)
        return false;
      rational maxx =
          abs(Lower<InX>) > abs(Upper<InX>)
            ? abs(Lower<InX>) : abs(Upper<InX>);
      rational maxy =
          abs(Lower<InY>) > abs(Upper<InY>)
            ? abs(Lower<InY>) : abs(Upper<InY>);
      if (Lower<Out> > -maxy || Upper<Out> < maxy)
        return false;
      constexpr umax lim = static_cast<umax>(std::numeric_limits<imax>::max() / 4);
      auto ux = maxx / g;  auto uy = maxy / g;  auto uo = maxy / Notch<Out>;
      return ux.has_value() && uy.has_value() && uo.has_value()
          && ux->Numerator <= lim && uy->Numerator <= lim && uo->Numerator <= lim;
    }();
  }

  // x mod y = x − ⌊x/y⌋·y (truncated-division convention, matching std::fmod).
  // Result has the sign of x. Pre: y != 0 (fmod_impl checks it).
  template <insidable Out, insidable InX, insidable InY>
  [[nodiscard]] constexpr Out fmod_nonzero(InX x, InY y)
  {
    if constexpr (detail::fmod_int_fast<Out, InX, InY>)
    {
      // One integer remainder in g-units; bit-identical to the rational path.
      constexpr rational g = *beman::inside::detail::gcd(Notch<InX>, Notch<InY>);
      constexpr imax wx  = trunc((Notch<InX> / g).value());
      constexpr imax wy  = trunc((Notch<InY> / g).value());
      constexpr imax wo  = trunc((g / Notch<Out>).value());
      constexpr imax lox = trunc((Lower<InX> / g).value());   // exact: grid invariant
      constexpr imax loy = trunc((Lower<InY> / g).value());
      constexpr imax loo = trunc((Lower<Out> / Notch<Out>).value());
      const imax a = beman::inside::detail::raw_imax(x) * wx
                   + (beman::inside::detail::index_raw<InX> ? lox : 0);
      const imax b = beman::inside::detail::raw_imax(y) * wy
                   + (beman::inside::detail::index_raw<InY> ? loy : 0);
      const imax r = a % b;                                    // |r| < |b|, in Out's range
      return Out::from_raw(beman::inside::detail::raw_from_offset<Out>(r * wo - loo));
    }
    else
    {
      rational xv = x;
      rational yv = y;
      rational q  = xv / yv;
      imax     qt = trunc(q);
      rational qy = qt * yv;
      rational r  = xv - qy;
      return detail::store_grid<Out>(r);
    }
  }

  // Like `/`: a plain Out when y's grid excludes 0, else expected<Out, errc>
  // with division_by_zero for y == 0.
  template <insidable Out, insidable InX, insidable InY>
  [[nodiscard]] constexpr auto fmod_impl(InX x, InY y)
  {
    if constexpr (beman::inside::detail::DivisorExcludesZero<InY>)
      return fmod_nonzero<Out>(x, y);
    else
    {
      if (y == 0)
        return std::expected<Out, errc>{std::unexpected(errc::division_by_zero)};
      return std::expected<Out, errc>{fmod_nonzero<Out>(x, y)};
    }
  }

  //---------------------------------------------------------------------------
  // Auto-deducing forms — algebraic tier.
  //
  // Each function gets a second overload that derives `Out` from `In` and
  // delegates to the explicit form. `f<Out>(x)` picks the explicit form, `f(x)`
  // the auto form (explicit Out can't be deduced from a parameter, so it drops
  // out). Notch policy: abs/fmod inherit `Notch<In>`; floor/ceil/round/trunc
  // deduce `notch<1>` since their outputs are integer-valued.
  //---------------------------------------------------------------------------

  //---------------------------------------------------------------------------
  // pown<E> — compile-time integer powers, pure grid arithmetic
  //---------------------------------------------------------------------------
  // Repeated squaring in inside-space: every multiply widens the result grid
  // corner-correctly, so the result is exact for exact inputs and negative
  // bases are fine. No engine, no `real` requirement — works on any inside
  // (like abs/floor/fmod). Checked rational raws may return
  // std::expected<inside, errc> per the usual arithmetic vocabulary. Negative
  // exponents are deferred (they need the division error story).
  template <imax E, insidable In>
    requires (E >= 0)
  [[nodiscard]] constexpr auto pown(In x) noexcept
  {
    if constexpr (E == 0)      { (void)x; return just<1>; }
    else if constexpr (E == 1) return x;
    else if constexpr (E % 2)  return x * pown<E - 1>(x);
    else                       { auto h = pown<E / 2>(x); return h * h; }
  }

  namespace detail
  {
    using namespace beman::inside::detail;

    // (fmod has no auto form: with two insidable inputs, `fmod<X>(x, y)` is
    // ambiguous between the explicit-Out and auto overloads — partial ordering
    // can't tell them apart. The explicit form is the canonical entry point.)
  } // namespace detail

  template <insidable In>
  [[nodiscard]] constexpr auto abs(In x) { return abs_impl<detail::abs_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto floor(In x) { return floor_impl<detail::floor_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto ceil(In x) { return ceil_impl<detail::ceil_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto round(In x) { return round_impl<detail::round_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto trunc(In x) { return trunc_impl<detail::trunc_auto_t<In>>(x); }

  // sqrt: non-negative inside → inside. Newton-Raphson on grid-scaled integer math
  // with a leading-bit initial guess; input must have Lower == 0. The mixed-sign
  // overload below accepts Lower < 0 and errors on a negative runtime value.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out sqrt_impl(In x)
  {
    static_assert(Lower<In> == 0,
                  "beman::inside::math::sqrt: input must start at 0 (use the mixed-sign overload)");
    static_assert(Lower<Out> <= 0,
                  "beman::inside::math::sqrt: Out must include 0");

    constexpr int W = detail::working_bits<Out>();
    imax a_w = detail::to_fixed(rational{x}, W);
    return detail::store_grid<Out>(detail::fixed_to_rational(detail::sqrt_fixed<W>(a_w), W));
  }

  // Mixed-sign sqrt: accepts inputs whose interval crosses zero. Returns
  // `unexpected(errc::domain_error)` on a negative runtime value, else same as
  // sqrt_impl.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr std::expected<Out, errc> sqrt_signed_impl(In x)
  {
    static_assert(Lower<Out> <= 0,
                  "beman::inside::math::sqrt: Out must include 0");

    rational v = beman::inside::detail::as_rational(x);
    if (v < rational{0})
      return std::unexpected(errc::domain_error);

    constexpr int W = detail::working_bits<Out>();
    imax a_w = detail::to_fixed(v, W);
    return detail::store_grid<Out>(detail::fixed_to_rational(detail::sqrt_fixed<W>(a_w), W));
  }

  //---------------------------------------------------------------------------
  // Auto-deducing forms — monotonic transcendental tier. Each derives Out from
  // In: Lower/Upper from running the engine cores on the input endpoints at
  // compile time, rounded outward to Notch<In> so the deduced inside covers the
  // true range even for irrational endpoints; notch and policy inherited from In.
  //---------------------------------------------------------------------------
  namespace detail
  {
    using namespace beman::inside::detail;

    // Round a rational down to the nearest multiple of `notch`.
    constexpr rational floor_to_notch(rational x, rational notch) noexcept
    {
      rational q = x / notch;
      imax n  = floor(q);
      return n * notch;
    }

    // Round a rational up to the nearest multiple of `notch`.
    constexpr rational ceil_to_notch(rational x, rational notch) noexcept
    {
      rational q = x / notch;
      imax n = ceil(q);
      return n * notch;
    }

    // Helpers: evaluate the engine cores on a compile-time-known
    // rational endpoint and return the result as a rational.
    constexpr rational sqrt_endpoint(rational v) noexcept
    {
      if (v == 0) return rational{0};
      return fixed_to_rational(sqrt_fixed<kRefBits>(to_fixed(v, kRefBits)), kRefBits);
    }

    constexpr rational exp2_endpoint(rational v) noexcept
    { return exp2_fixed<kRefBits>(v); }

    constexpr rational log2_endpoint(rational v) noexcept
    { return log2_fixed<kRefBits>(v); }

    constexpr rational exp_endpoint(rational v) noexcept
    { return exp_fixed<kRefBits>(v); }

    constexpr rational log_endpoint(rational v) noexcept
    { return ln_fixed<kRefBits>(v); }

    template <imax Base>
    constexpr rational pow_base_endpoint(rational v) noexcept
    {
      imax sc_w = fmul(to_fixed(v, kRefBits),
                       log2_to_fixed<kRefBits>(rational{Base}), kRefBits);
      return exp2_from_fixed<kRefBits>(sc_w);
    }

    // Deduction aliases. Each rounds endpoints outward to Notch<In> and adds
    // `round_nearest` — the cores emit sub-notch drift, so the assignment needs
    // a rounding rule to land on the grid.
    template <insidable In>
    using sqrt_auto_t = inside<{{rational{0},
                                ceil_to_notch(sqrt_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, out_policy<In> | round_nearest>;

    // Mixed-sign sqrt: Upper of the result is sqrt of the larger absolute
    // endpoint, since the runtime value can be anywhere in [Lower, Upper].
    template <insidable In>
    inline constexpr rational sqrt_signed_upper =
        (abs(Lower<In>) > abs(Upper<In>))
            ? abs(Lower<In>) : abs(Upper<In>);

    template <insidable In>
    using sqrt_signed_auto_t = inside<{{rational{0},
                                       ceil_to_notch(sqrt_endpoint(sqrt_signed_upper<In>),
                                                     Notch<In>)},
                                      Notch<In>}, out_policy<In> | round_nearest>;

    // Auto output grid for results in [lo, hi]: the endpoints rounded outward
    // to In's notch, with In's notch and policy (plus round_nearest).
    template <insidable In, rational Lo, rational Hi>
    using outward_t = inside<{{floor_to_notch(Lo, Notch<In>), ceil_to_notch(Hi, Notch<In>)},
                              Notch<In>}, out_policy<In> | round_nearest>;

    template <insidable In>
    using exp2_auto_t = outward_t<In, exp2_endpoint(Lower<In>), exp2_endpoint(Upper<In>)>;

    template <insidable In>
    using log2_auto_t = outward_t<In, log2_endpoint(Lower<In>), log2_endpoint(Upper<In>)>;

    template <insidable In>
    using exp_auto_t = outward_t<In, exp_endpoint(Lower<In>), exp_endpoint(Upper<In>)>;

    template <insidable In>
    using log_auto_t = outward_t<In, log_endpoint(Lower<In>), log_endpoint(Upper<In>)>;

    template <imax Base, insidable In>
    using pow_base_auto_t = outward_t<In, pow_base_endpoint<Base>(Lower<In>), pow_base_endpoint<Base>(Upper<In>)>;
  } // namespace detail

  //---------------------------------------------------------------------------
  // Auto-deducing forms — trig + atan2 + tan + fmod.
  //
  // sin / cos default to the full amplitude range [-1, 1]; atan2 defaults to
  // the full angle range [-π, π] radians. tan defaults to [-1024, 1024]
  // (covers all phases >1 slot from a pole; closer-to-pole phases trip the
  // overflow branch of the returned `expected`). fmod inherits sign from x.
  // Notch is inherited from input throughout.
  //---------------------------------------------------------------------------
  namespace detail
  {
    using namespace beman::inside::detail;

    template <insidable In>
    using sin_auto_t = inside<{{-rational{1}, rational{1}},
                               Notch<In>}, out_policy<In> | round_nearest>;

    template <insidable In>
    using cos_auto_t = sin_auto_t<In>;

    // Output covers [-π, π] rounded outward to notch multiples — the exact
    // ±π endpoints are irrational and would violate the grid's divides-evenly
    // invariant against a rational notch.
    template <insidable In>
    using atan2_auto_t = outward_t<In, -pi_r, pi_r>;

    template <insidable In>
    using tan_auto_t = inside<{{-rational{1024}, rational{1024}},
                               Notch<In>}, out_policy<In> | round_nearest>;

    // fmod's result: |r| < |y| and |r| ≤ |x|, with the sign of x, on the gcd of
    // both notches (x − k·y lies on that lattice, so the result is exact).
    template <insidable B>
    inline constexpr rational max_abs = abs(Lower<B>) > abs(Upper<B>) ? abs(Lower<B>) : abs(Upper<B>);

    template <insidable InX, insidable InY>
    inline constexpr rational fmod_bound =
        max_abs<InX> < max_abs<InY> ? max_abs<InX> : max_abs<InY>;

    template <insidable InX, insidable InY>
    inline constexpr rational fmod_notch =
        (Notch<InX> == 0 || Notch<InY> == 0) ? rational{0} : *gcd(Notch<InX>, Notch<InY>);

    template <insidable InX, insidable InY>
    using fmod_auto_t = inside<{{(Lower<InX> < 0 ? -fmod_bound<InX, InY> : rational{0}),
                                 (Upper<InX> > 0 ?  fmod_bound<InX, InY> : rational{0})},
                                fmod_notch<InX, InY>}, out_policy<InX> | round_nearest>;
  } // namespace detail

  template <insidable InX, insidable InY>
  [[nodiscard]] constexpr auto fmod(InX x, InY y)
  { return fmod_impl<detail::fmod_auto_t<InX, InY>>(x, y); }

  //===========================================================================
  // Grid-native periodic trig: circle<M> angle + caller-owned amplitude.
  //
  // A `circle<M>` is one revolution split into M equal slots, valued in DEGREES.
  // Degrees have an integer period (360), so a notch divides the circle exactly
  // and `wrap` is drift-free — unlike radians, whose 2π period no rational notch
  // divides. The raw is just the slot index 0..M-1.
  //
  // `sin`/`cos` write into a caller-supplied amplitude inside whose grid fixes the
  // output precision. The runtime path is a table lookup (no ×1/(2π), no
  // polynomial) into a first-quadrant table built at compile time by CORDIC.
  // Power-of-two M is optimal (reflection is a bitmask); any M%4==0 works.
  //===========================================================================
  template <std::uint64_t M>
  using circle = inside<{{rational{0},
                         rational{std::uint64_t{360} * (M - 1),
                                               static_cast<imax>(M)}},
                        notch<360, static_cast<imax>(M)>}, real | wrap>;

  // Amplitude output grid: [-1, 1] at 1/K resolution. The natural target for
  // `sin(circle<M>, amp<K>&)` — angle precision (M) and amplitude precision (K)
  // are chosen independently.
  template <std::uint64_t K>
  using amp = inside<{{rational{-1}, rational{1}},
                     notch<1, static_cast<imax>(K)>}, real>;

  namespace detail
  {
    using namespace beman::inside::detail;

    // First-quadrant sine table for an M-slot circle at working scale 2^W:
    // entry j = sin(j/M turn) for j ∈ [0, M/4], as a rational. Filled at
    // compile time by the grid-scaled CORDIC rotation (`sin_from_turn_fixed`),
    // never evaluated at runtime. Keyed by <M, W> so the entry precision tracks
    // the amplitude grid that selected W.
    template <imax M, int W>
    inline constexpr auto sin_quarter_table = []{
      std::array<rational, static_cast<std::size_t>(M / 4) + 1> t{};
      for (imax j = 0; j <= M / 4; ++j)
      {
        imax turn_w = ((j << W) + M / 2) / M;             // round(j/M · 2^W)
        t[j] = sin_from_turn_fixed<W, W>(turn_w);
      }
      return t;
    }();

    // sin(i/M turn) as a rational for any integer slot i (wraps mod M), by
    // quadrant reduction: sign from the half-turn, reflect about M/4. The table
    // holds first-quadrant magnitudes; this applies the sign. Power-of-two M
    // makes the half/quarter compares a bitmask.
    template <imax M, int W>
    constexpr rational sin_slot(imax i) noexcept
    {
      constexpr imax half = M / 2, quarter = M / 4;
      i = euclid_mod(i, M);                   // wrap into [0, M)
      bool flip = i >= half;
      if (flip) i -= half;                    // sin(π + x) = -sin(x)
      if (i > quarter) i = half - i;          // sin(π - x) =  sin(x)
      rational mag = sin_quarter_table<M, W>[i];
      return flip ? -mag : mag;
    }

    // Recover the slot count M from a circle-shaped angle: the degree period
    // 360 divided by the notch. The public entry points validate the shape.
    template <insidable DEG>
    inline constexpr imax circle_slots =
      round((rational{360} / Notch<DEG>).value());

    // Shared shape check for the circle-input entry points.
    template <insidable DEG>
    constexpr bool valid_circle() noexcept
    {
      static_assert(Lower<DEG> == 0,
                    "beman::inside::math: circle angle must have Lower 0 (degrees)");
      static_assert(has_flag(InsidePolicy<DEG>, wrap),
                    "beman::inside::math: circle angle must carry the wrap policy");
      static_assert(has_flag(InsidePolicy<DEG>, real),
                    "beman::inside::math: circle angle must carry the `real` policy "
                    "(circle<M> already does; custom angle bounds must add `| real`)");
      static_assert(circle_slots<DEG> % 4 == 0,
                    "beman::inside::math: circle slot count M must be divisible by 4");
      return true;
    }
  } // namespace detail

  // sin(angle) → out, on out's amplitude grid. angle is a circle<M>. Reference
  // output (not a return) lets AMP be deduced from the caller's object and reuses
  // its assignment policy for the final rounding.
  template <insidable DEG, insidable AMP>
  BEMAN_INSIDE_MATH_FN void sin(DEG angle, AMP& out)
  {
    static_assert(detail::valid_circle<DEG>());
#if defined(BEMAN_INSIDE_MATH_NO_FP)
    constexpr imax M = detail::circle_slots<DEG>;
    constexpr int  W = detail::working_bits<AMP>();
    out = detail::sin_slot<M, W>(beman::inside::detail::raw_imax(angle));
#elif defined(BEMAN_INSIDE_MATH_FLOAT)
    out = flt::detail::d_sin(flt::to_float(angle) * (flt::detail::kPi / 180.0f));
#else
    out = dbl::detail::d_sin(static_cast<double>(angle) * (dbl::detail::kPi / 180.0));
#endif
  }

  // cos(angle) → out. cos(x) = sin(x + ¼ turn): shift the slot by M/4.
  template <insidable DEG, insidable AMP>
  BEMAN_INSIDE_MATH_FN void cos(DEG angle, AMP& out)
  {
    static_assert(detail::valid_circle<DEG>());
#if defined(BEMAN_INSIDE_MATH_NO_FP)
    constexpr imax M = detail::circle_slots<DEG>;
    constexpr int  W = detail::working_bits<AMP>();
    out = detail::sin_slot<M, W>(beman::inside::detail::raw_imax(angle) + M / 4);
#elif defined(BEMAN_INSIDE_MATH_FLOAT)
    out = flt::detail::d_cos(flt::to_float(angle) * (flt::detail::kPi / 180.0f));
#else
    out = dbl::detail::d_cos(static_cast<double>(angle) * (dbl::detail::kPi / 180.0));
#endif
  }

  // tan(angle) → out = sin/cos. Returns false (and leaves out untouched) when
  // the angle lands exactly on a pole (cos == 0); overflow of the amplitude
  // grid is handled by out's own policy (e.g. clamp).
  template <insidable DEG, insidable AMP>
  [[nodiscard]] BEMAN_INSIDE_MATH_FN bool tan(DEG angle, AMP& out)
  {
    static_assert(detail::valid_circle<DEG>());
#if defined(BEMAN_INSIDE_MATH_NO_FP)
    constexpr imax M = detail::circle_slots<DEG>;
    constexpr int  W = detail::working_bits<AMP>();
    imax i = beman::inside::detail::raw_imax(angle);
    rational c = detail::sin_slot<M, W>(i + M / 4);
    if (c == 0) return false;                                  // pole
    out = (detail::sin_slot<M, W>(i) / c).value();             // sin / cos
    return true;
#elif defined(BEMAN_INSIDE_MATH_FLOAT)
    float t;
    if (!flt::detail::d_tan(flt::to_float(angle) * (flt::detail::kPi / 180.0f), t)) return false;   // pole
    out = t;
    return true;
#else
    double t;
    if (!dbl::detail::d_tan(static_cast<double>(angle) * (dbl::detail::kPi / 180.0), t)) return false;   // pole
    out = t;
    return true;
#endif
  }

  //===========================================================================
  // Extended transcendentals — inverse trig, hyperbolic, log10, pow, cbrt,
  // hypot. Each composes the CORDIC / Newton cores defined above; no new
  // polynomial machinery. Outputs follow the beman::inside::math conventions: angles in
  // radians, runtime-conditional failures via `std::expected<Out, errc>`,
  // statically-knowable domain limits via `static_assert`.
  //===========================================================================
  namespace detail
  {
    using namespace beman::inside::detail;

    // --- inverse trig (radians) -------------------------------------------
    // atan(v) in radians at scale 2^W: atan2(v, 1) — x = 1 > 0, so the
    // vectoring CORDIC runs with no pre-rotation. Grid-scaled (no Q.30).
    // Full domain: |v| > 1 reduces via atan(v) = sign(v)·(π/2 − atan(1/|v|)),
    // keeping the CORDIC argument inside its [-1, 1] window. Inputs with
    // |v| ≤ 1 take the original branch unchanged (bit-identical results).
    template <int W>
    constexpr rational atan_fixed(rational v) noexcept
    {
      rational av = abs(v);
      if (av <= rational{1})
      {
        imax rad = cordic_atan2_rad<W, W>(to_fixed(v, W), imax{1} << W);
        return fixed_to_rational(rad, W);
      }
      rational inv = 1 / av;
      imax rad = cordic_atan2_rad<W, W>(to_fixed(inv, W), imax{1} << W);
      rational mag = pi_r / 2 - fixed_to_rational(rad, W);
      return (v < rational{0}) ? -mag : mag;
    }

    // asin(v) = atan2(v, sqrt(1 − v²)); v ∈ [−1, 1] → result ∈ [−π/2, π/2].
    template <int W = kRefBits>
    constexpr rational asin_endpoint(rational v) noexcept
    {
      imax one = imax{1} << W;
      imax v_w = to_fixed(v, W);
      imax c_w = sqrt_fixed<W>(one - fmul(v_w, v_w, W));   // √(1−v²) ≥ 0
      if (c_w == 0) {                                                    // v = ±1 → ±π/2
        rational half_pi = pi_r / 2;
        return (v < rational{0}) ? -half_pi : half_pi;
      }
      imax rad = cordic_atan2_rad<W, W>(v_w, c_w);        // x = c_w > 0
      return fixed_to_rational(rad, W);
    }

    // acos(v) = π/2 − asin(v); v ∈ [−1, 1] → result ∈ [0, π].
    template <int W = kRefBits>
    constexpr rational acos_endpoint(rational v) noexcept
    {
      rational half_pi = pi_r / 2;
      return half_pi - asin_endpoint<W>(v);
    }

    // --- hyperbolic (from e^x via the exp core) ---------------------------
    // sinh/cosh = (e^v ∓ e^-v)/2, combined in fixed-point at W, not as
    // rationals: e^v and e^-v have wildly different denominators and the rational
    // cross-multiply overflows imax. At scale kRefBits each term is one scaled
    // integer (|v| ≤ 10 ⇒ e^|v|·2^30 ≤ 2.4e13, well inside int63).
    // e^|v| and e^-|v| at scale W from ONE exponential: the small term is the
    // rounded fixed-point reciprocal of the large one (2^2W ≤ 2^60 fits imax).
    template <int W>
    constexpr void exp_pair(rational v, imax& big, imax& small) noexcept
    {
      big   = to_fixed(exp_fixed<W>(abs(v)), W);
      small = ((imax{1} << (2 * W)) + big / 2) / big;
    }

    template <int W = kRefBits>
    constexpr rational sinh_endpoint(rational v) noexcept
    {
      imax big, small;
      exp_pair<W>(v, big, small);
      const imax h = (big - small) / 2;
      return fixed_to_rational(v < rational{0} ? -h : h, W);
    }

    template <int W = kRefBits>
    constexpr rational cosh_endpoint(rational v) noexcept
    {
      imax big, small;
      exp_pair<W>(v, big, small);
      return fixed_to_rational((big + small) / 2, W);
    }

    // tanh via the overflow-safe form tanh(x) = (1 − e^-2|x|)/(1 + e^-2|x|),
    // odd-extended for x < 0. With u = e^-2|x| ∈ (0, 1] at scale W, the
    // quotient `((1−u)·2^W)/(1+u)` keeps the dividend bounded.
    template <int W = kRefBits>
    constexpr rational tanh_endpoint(rational v) noexcept
    {
      constexpr imax one = imax{1} << W;
      rational av = abs(v);
      imax u = to_fixed(exp_fixed<W>(av * -2), W);
      imax t = ((one - u) << W) / (one + u);
      return (v < rational{0}) ? fixed_to_rational(-t, W)
                                            : fixed_to_rational(t, W);
    }

    // --- log10, cbrt ------------------------------------------------------
    template <int W>
    inline constexpr imax inv_ln10_w = inv_ln10_fixed<W>();

    template <int W = kRefBits>
    constexpr rational log10_endpoint(rational v) noexcept
    { return fixed_to_rational(fmul(ln_to_fixed<W>(v), inv_ln10_w<W>, W), W); }

    // cbrt(v) = sign(v)·e^(ln|v|/3); cbrt(0) = 0.
    template <int W = kRefBits>
    constexpr rational cbrt_endpoint(rational v) noexcept
    {
      if (v == rational{0}) return rational{0};
      rational av = abs(v);
      rational mag = exp_from_fixed<W>(ln_to_fixed<W>(av) / 3);
      return (v < rational{0}) ? -mag : mag;
    }

    // hypot(x, y) = sqrt(x²+y²), computed as m·sqrt((x/m)²+(y/m)²) with
    // m = max(|x|,|y|) so the radicand stays in [1, 2]. Exact rational scaling;
    // reuses the grid-scaled sqrt_fixed.
    template <int W = kRefBits>
    constexpr rational hypot_endpoint(rational x,
                                                   rational y) noexcept
    {
      rational ax = abs(x);
      rational ay = abs(y);
      rational m  = (ax > ay) ? ax : ay;
      if (m == rational{0}) return rational{0};
      // Form the radicand (x/m)²+(y/m)² ∈ [1, 2] at scale kRefBits — keeping it
      // a rational overflows imax (the squared numerators cross-multiply to
      // ~1e24). Each ratio is ≤ 1, so its fixed-point square fits comfortably.
      imax rx = to_fixed(x / m, W);
      imax ry = to_fixed(y / m, W);
      imax s_w = fmul(rx, rx, W) + fmul(ry, ry, W);
      const imax root_w = sqrt_fixed<W>(s_w);
      // Exact rational product when it fits; a wide-denominator m can push m·root
      // past imax, so fall back to the fixed-point form (|m| ≤ 2^20 envelope).
      if (auto exact = m * fixed_to_rational(root_w, W))
        return *exact;
      return fixed_to_rational(fmul(to_fixed(m, W), root_w, W),
                               W);
    }

    // pow(b, e) = 2^(e·log2(b)), b > 0. The exponent e·log2(b) is saturated into
    // exp2's [−30, 30] envelope so the power-of-two denominator never UB-shifts;
    // the runtime impl reports envelope overflow via `expected`.
    constexpr rational pow_endpoint(rational b,
                                                 rational e) noexcept
    {
      imax sc_w = fmul(to_fixed(e, kRefBits), log2_to_fixed<kRefBits>(b), kRefBits);
      constexpr imax lim = imax{30} << kRefBits;
      sc_w = (sc_w > lim) ? lim : (sc_w < -lim) ? -lim : sc_w;
      return exp2_from_fixed<kRefBits>(sc_w);
    }

    // --- deduction aliases ------------------------------------------------
    // Monotonic-increasing functions round endpoints outward like the exp/log
    // family. acos is decreasing; cosh is even (min at 0 if the interval
    // spans it). round_nearest lands sub-notch drift onto the grid.
    template <insidable In>
    using atan_auto_t = outward_t<In, atan_fixed<working_bits<In>()>(Lower<In>), atan_fixed<working_bits<In>()>(Upper<In>)>;

    template <insidable In>
    using asin_auto_t = outward_t<In, asin_endpoint(Lower<In>), asin_endpoint(Upper<In>)>;

    template <insidable In>
    using acos_auto_t = outward_t<In, acos_endpoint(Upper<In>), acos_endpoint(Lower<In>)>;

    template <insidable In>
    using sinh_auto_t = outward_t<In, sinh_endpoint(Lower<In>), sinh_endpoint(Upper<In>)>;

    template <insidable In>
    inline constexpr rational cosh_auto_lo =
      (Lower<In> <= rational{0} && Upper<In> >= rational{0})
        ? rational{1}
        : (cosh_endpoint(Lower<In>) < cosh_endpoint(Upper<In>)
             ? cosh_endpoint(Lower<In>) : cosh_endpoint(Upper<In>));

    template <insidable In>
    inline constexpr rational cosh_auto_hi =
      (cosh_endpoint(Lower<In>) > cosh_endpoint(Upper<In>))
        ? cosh_endpoint(Lower<In>) : cosh_endpoint(Upper<In>);

    template <insidable In>
    using cosh_auto_t = outward_t<In, cosh_auto_lo<In>, cosh_auto_hi<In>>;

    template <insidable In>
    using tanh_auto_t = outward_t<In, tanh_endpoint(Lower<In>), tanh_endpoint(Upper<In>)>;

    template <insidable In>
    using log10_auto_t = outward_t<In, log10_endpoint(Lower<In>), log10_endpoint(Upper<In>)>;

    template <insidable In>
    using cbrt_auto_t = outward_t<In, cbrt_endpoint(Lower<In>), cbrt_endpoint(Upper<In>)>;

    // hypot output: non-negative, Upper at the largest-magnitude corner.
    template <insidable InX, insidable InY>
    inline constexpr rational hypot_auto_hi =
      hypot_endpoint(
        (abs(Lower<InX>) > abs(Upper<InX>)
           ? abs(Lower<InX>) : abs(Upper<InX>)),
        (abs(Lower<InY>) > abs(Upper<InY>)
           ? abs(Lower<InY>) : abs(Upper<InY>)));

    template <insidable InX, insidable InY>
    using hypot_auto_t = inside<{{rational{0},
                                 ceil_to_notch(hypot_auto_hi<InX, InY>, Notch<InX>)},
                                Notch<InX>}, out_policy<InX> | round_nearest>;

    // pow output: extrema of b^e over the input rectangle occur at corners
    // (monotone in each argument for b > 0). Min and max of the 4 corners.
    template <insidable InB, insidable InE>
    inline constexpr rational pow_corner[4] = {
      pow_endpoint(Lower<InB>, Lower<InE>), pow_endpoint(Lower<InB>, Upper<InE>),
      pow_endpoint(Upper<InB>, Lower<InE>), pow_endpoint(Upper<InB>, Upper<InE>),
    };

    template <insidable InB, insidable InE>
    inline constexpr rational pow_auto_lo = []{
      rational m = pow_corner<InB, InE>[0];
      for (int i = 1; i < 4; ++i)
        if (pow_corner<InB, InE>[i] < m) m = pow_corner<InB, InE>[i];
      return m;
    }();

    template <insidable InB, insidable InE>
    inline constexpr rational pow_auto_hi = []{
      rational m = pow_corner<InB, InE>[0];
      for (int i = 1; i < 4; ++i)
        if (pow_corner<InB, InE>[i] > m) m = pow_corner<InB, InE>[i];
      return m;
    }();

    template <insidable InB, insidable InE>
    using pow_auto_t = inside<{{floor_to_notch(pow_auto_lo<InB, InE>, Notch<InB>),
                               ceil_to_notch (pow_auto_hi<InB, InE>, Notch<InB>)},
                              Notch<InB>}, out_policy<InB> | round_nearest>;
  } // namespace detail

  // --- explicit-Out impls -------------------------------------------------
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out atan_impl(In x)
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "beman::inside::math::atan: input magnitudes must be \u2264 2^20 for the working-scale envelope");
    return detail::store_grid<Out>(detail::atan_fixed<detail::working_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out asin_impl(In x)
  {
    static_assert(Lower<In> >= -1 && Upper<In> <= 1,
                  "beman::inside::math::asin: input must be in [-1, 1]");
    return detail::store_grid<Out>(detail::asin_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out acos_impl(In x)
  {
    static_assert(Lower<In> >= -1 && Upper<In> <= 1,
                  "beman::inside::math::acos: input must be in [-1, 1]");
    return detail::store_grid<Out>(detail::acos_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out sinh_impl(In x)
  {
    static_assert(Lower<In> >= -10 && Upper<In> <= 10,
                  "beman::inside::math::sinh: input must be in [-10, 10]");
    return detail::store_grid<Out>(detail::sinh_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out cosh_impl(In x)
  {
    static_assert(Lower<In> >= -10 && Upper<In> <= 10,
                  "beman::inside::math::cosh: input must be in [-10, 10]");
    static_assert(Lower<Out> <= rational{1},
                  "beman::inside::math::cosh: Out must include 1 (cosh ≥ 1)");
    return detail::store_grid<Out>(detail::cosh_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out tanh_impl(In x)
  {
    static_assert(Lower<In> >= -10 && Upper<In> <= 10,
                  "beman::inside::math::tanh: input must be in [-10, 10]");
    return detail::store_grid<Out>(detail::tanh_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out log10_impl(In x)
  {
    static_assert(Lower<In> > 0,
                  "beman::inside::math::log10: input must be strictly positive");
    return detail::store_grid<Out>(detail::log10_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out cbrt_impl(In x)
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "beman::inside::math::cbrt: input magnitude must be ≤ 2^20 for the working-scale envelope");
    return detail::store_grid<Out>(detail::cbrt_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable InX, insidable InY>
  [[nodiscard]] constexpr Out hypot_impl(InX x, InY y)
  {
    static_assert(Lower<InX> >= -(imax{1} << 20) && Upper<InX> <= (imax{1} << 20)
               && Lower<InY> >= -(imax{1} << 20) && Upper<InY> <= (imax{1} << 20),
                  "beman::inside::math::hypot: input magnitudes must be ≤ 2^20 for the working-scale envelope");
    static_assert(Lower<Out> <= 0, "beman::inside::math::hypot: Out must include 0");
    return detail::store_grid<Out>(detail::hypot_endpoint<detail::endpoint_bits<Out>()>(rational{x}, rational{y}));
  }

  // pow: b^e for runtime base b > 0. Returns `expected` — `overflow` when
  // e·log2(b) leaves exp2's [-30, 30] envelope or the result leaves Out's
  // interval. The auto form requires Lower<InB> > 0 (so b > 0 is guaranteed
  // and the output range is bounded for deduction).
  template <insidable Out, insidable InB, insidable InE>
  [[nodiscard]] constexpr std::expected<Out, errc> pow_impl(InB base, InE exp)
  {
    rational bv = base;
    if (bv <= rational{0})
      return std::unexpected(errc::domain_error);

    constexpr int W = detail::working_bits<Out>();
    imax sc_w = detail::fmul(detail::to_fixed(rational{exp}, W),
                             detail::log2_to_fixed<W>(bv), W);     // e·log2(b), scale 2^W
    constexpr imax lim = imax{30} << W;
    if (sc_w > lim || sc_w < -lim)
      return std::unexpected(errc::overflow);

    rational r = detail::exp2_from_fixed<W>(sc_w);
    if constexpr (!has_flag(InsidePolicy<Out>, clamp))   // clamp Out: saturate below
      if (r < Lower<Out> || r > Upper<Out>)
        return std::unexpected(errc::overflow);
    return detail::store_grid<Out>(r);
  }

  //===========================================================================
  // Explicit engine namespaces — call a chosen engine regardless of the build
  // default. `cordic::fn` (integer/CORDIC, ALWAYS present) and `dbl::fn` (the
  // double engine, present unless BEMAN_INSIDE_MATH_NO_FP) expose the SAME public-shaped
  // API as the top-level `beman::inside::math::fn` — same signatures, domains, auto-deduced
  // output grids, and domain static_asserts. The unqualified `beman::inside::math::fn` is
  // an alias for whichever engine the build selects (see the #ifdef dispatch
  // above); these let a single binary mix both — e.g. `cordic::sin` on a
  // determinism-critical path and `dbl::sin` on a hot one.
  //
  // The engines are independent approximations: a grid-snapped value can differ
  // between them by up to one notch on rounding ties (table-maker's dilemma).
  // Don't compare outputs across engines — see determinism.md.
  //===========================================================================
  namespace cordic
  {
    template <insidable In>
      requires (Lower<In> == rational{0})
    [[nodiscard]] constexpr auto sqrt(In x)
    { static_assert(detail::require_snap<In>()); return sqrt_impl<detail::sqrt_auto_t<In>>(x); }

    template <insidable In>
      requires (Lower<In> < rational{0})
    [[nodiscard]] constexpr auto sqrt(In x)
    { static_assert(detail::require_snap<In>()); return sqrt_signed_impl<detail::sqrt_signed_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto exp2(In x)
    { static_assert(detail::require_snap<In>()); return exp2_impl<detail::exp2_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto log2(In x)
    {
      static_assert(detail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::cordic::log2: input must be strictly positive");
      return log2_impl<detail::log2_auto_t<In>>(x);
    }

    template <insidable In>
    [[nodiscard]] constexpr auto exp(In x)
    { static_assert(detail::require_snap<In>()); return exp_impl<detail::exp_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto log(In x)
    {
      static_assert(detail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::cordic::log: input must be strictly positive");
      return log_impl<detail::log_auto_t<In>>(x);
    }

    template <imax Base, insidable In>
    [[nodiscard]] constexpr auto pow_base(In x)
    { static_assert(detail::require_snap<In>()); return pow_base_impl<Base, detail::pow_base_auto_t<Base, In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto sin(In angle)
    { static_assert(detail::require_snap<In>()); return sin_impl<detail::sin_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] constexpr auto cos(In angle)
    { static_assert(detail::require_snap<In>()); return cos_impl<detail::cos_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] constexpr auto tan(In angle)
    { static_assert(detail::require_snap<In>()); return tan_impl<detail::tan_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] constexpr auto atan2(In y, In x)
    { static_assert(detail::require_snap<In>()); return atan2_impl<detail::atan2_auto_t<In>>(y, x); }

    template <insidable In>
    [[nodiscard]] constexpr auto atan(In x)
    { static_assert(detail::require_snap<In>()); return atan_impl<detail::atan_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto asin(In x)
    { static_assert(detail::require_snap<In>()); return asin_impl<detail::asin_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto acos(In x)
    { static_assert(detail::require_snap<In>()); return acos_impl<detail::acos_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto sinh(In x)
    { static_assert(detail::require_snap<In>()); return sinh_impl<detail::sinh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto cosh(In x)
    { static_assert(detail::require_snap<In>()); return cosh_impl<detail::cosh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto tanh(In x)
    { static_assert(detail::require_snap<In>()); return tanh_impl<detail::tanh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto log10(In x)
    {
      static_assert(detail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::cordic::log10: input must be strictly positive");
      return log10_impl<detail::log10_auto_t<In>>(x);
    }

    template <insidable In>
    [[nodiscard]] constexpr auto cbrt(In x)
    { static_assert(detail::require_snap<In>()); return cbrt_impl<detail::cbrt_auto_t<In>>(x); }

    template <insidable InX, insidable InY>
    [[nodiscard]] constexpr auto hypot(InX x, InY y)
    {
      static_assert(detail::require_snap<InX>() && detail::require_snap<InY>());
      return hypot_impl<detail::hypot_auto_t<InX, InY>>(x, y);
    }

    template <insidable InB, insidable InE>
      requires (Lower<InB> > rational{0})
    [[nodiscard]] constexpr auto pow(InB base, InE exp)
    {
      static_assert(detail::require_snap<InB>() && detail::require_snap<InE>());
      return pow_impl<detail::pow_auto_t<InB, InE>>(base, exp);
    }
  } // namespace cordic

  // The shared deduction/helpers, as seen from the engine namespaces (where a
  // bare `detail::` names the engine's own cores).
  namespace mdetail = beman::inside::math::detail;

#ifndef BEMAN_INSIDE_MATH_NO_FP
  namespace dbl
  {
    // Public-shaped double-engine entry points. `detail::` here would resolve to
    // beman::inside::math::dbl::detail (the engine cores), so the shared deduction/helpers
    // are qualified as `mdetail::`; the `*_core`/`store`/`d_*` names are
    // this namespace's own. Absent under BEMAN_INSIDE_MATH_NO_FP (no FP, no <cmath>).
    template <insidable In>
      requires (Lower<In> == rational{0})
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sqrt(In x)
    { static_assert(mdetail::require_snap<In>()); return sqrt_core<mdetail::sqrt_auto_t<In>>(x); }

    template <insidable In>
      requires (Lower<In> < rational{0})
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sqrt(In x)
    {
      static_assert(mdetail::require_snap<In>());
      using Out = mdetail::sqrt_signed_auto_t<In>;
      double v = static_cast<double>(x);
      if (v < 0.0)
        return std::expected<Out, errc>{std::unexpected(errc::domain_error)};
      return std::expected<Out, errc>{store<Out>(detail::d_sqrt(v))};
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto exp2(In x)
    { static_assert(mdetail::require_snap<In>()); return exp2_core<mdetail::exp2_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto log2(In x)
    {
      static_assert(mdetail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::dbl::log2: input must be strictly positive");
      return log2_core<mdetail::log2_auto_t<In>>(x);
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto exp(In x)
    { static_assert(mdetail::require_snap<In>()); return exp_core<mdetail::exp_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto log(In x)
    {
      static_assert(mdetail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::dbl::log: input must be strictly positive");
      return log_core<mdetail::log_auto_t<In>>(x);
    }

    template <imax Base, insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto pow_base(In x)
    {
      static_assert(mdetail::require_snap<In>());
      using Out = mdetail::pow_base_auto_t<Base, In>;
      return store<Out>(detail::d_pow(static_cast<double>(Base), static_cast<double>(x)));
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sin(In angle)
    { static_assert(mdetail::require_snap<In>()); return sin_core<mdetail::sin_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto cos(In angle)
    { static_assert(mdetail::require_snap<In>()); return cos_core<mdetail::cos_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto tan(In angle)
    {
      static_assert(mdetail::require_snap<In>());
      using Out = mdetail::tan_auto_t<In>;
      double t;
      if (!detail::d_tan(static_cast<double>(angle), t))
        return std::expected<Out, errc>{std::unexpected(errc::division_by_zero)};
      if constexpr (!has_flag(InsidePolicy<Out>, clamp))   // clamp Out: saturate below
        if (t < mdetail::lower_fp<double, Out> || t > mdetail::upper_fp<double, Out>)
          return std::expected<Out, errc>{std::unexpected(errc::overflow)};
      return std::expected<Out, errc>{store<Out>(t)};
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto atan2(In y, In x)
    { static_assert(mdetail::require_snap<In>()); return atan2_core<mdetail::atan2_auto_t<In>>(y, x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto atan(In x)
    { static_assert(mdetail::require_snap<In>()); return atan_core<mdetail::atan_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto asin(In x)
    { static_assert(mdetail::require_snap<In>()); return asin_core<mdetail::asin_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto acos(In x)
    { static_assert(mdetail::require_snap<In>()); return acos_core<mdetail::acos_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sinh(In x)
    { static_assert(mdetail::require_snap<In>()); return sinh_core<mdetail::sinh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto cosh(In x)
    { static_assert(mdetail::require_snap<In>()); return cosh_core<mdetail::cosh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto tanh(In x)
    { static_assert(mdetail::require_snap<In>()); return tanh_core<mdetail::tanh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto log10(In x)
    {
      static_assert(mdetail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::dbl::log10: input must be strictly positive");
      return log10_core<mdetail::log10_auto_t<In>>(x);
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto cbrt(In x)
    { static_assert(mdetail::require_snap<In>()); return cbrt_core<mdetail::cbrt_auto_t<In>>(x); }

    template <insidable InX, insidable InY>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto hypot(InX x, InY y)
    {
      static_assert(mdetail::require_snap<InX>() && mdetail::require_snap<InY>());
      return hypot_core<mdetail::hypot_auto_t<InX, InY>>(x, y);
    }

    template <insidable InB, insidable InE>
      requires (Lower<InB> > rational{0})
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto pow(InB base, InE exp)
    {
      static_assert(mdetail::require_snap<InB>() && mdetail::require_snap<InE>());
      using Out = mdetail::pow_auto_t<InB, InE>;
      double b = static_cast<double>(base);
      if (b <= 0.0)
        return std::expected<Out, errc>{std::unexpected(errc::domain_error)};
      double r = detail::d_pow(b, static_cast<double>(exp));
      if constexpr (!has_flag(InsidePolicy<Out>, clamp))   // clamp Out: saturate below
        if (r < mdetail::lower_fp<double, Out> || r > mdetail::upper_fp<double, Out>)
          return std::expected<Out, errc>{std::unexpected(errc::overflow)};
      return std::expected<Out, errc>{store<Out>(r)};
    }
  } // namespace dbl

  namespace flt
  {
    // Public-shaped float-engine entry points — binary32 compute, same shapes as
    // dbl:: (qualify shared helpers as mdetail::; *_core/store/d_* are
    // this namespace's own). A third value set (float ≠ double ≠ cordic).
    template <insidable In>
      requires (Lower<In> == rational{0})
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sqrt(In x)
    { static_assert(mdetail::require_snap<In>()); return sqrt_core<mdetail::sqrt_auto_t<In>>(x); }

    template <insidable In>
      requires (Lower<In> < rational{0})
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sqrt(In x)
    {
      static_assert(mdetail::require_snap<In>());
      using Out = mdetail::sqrt_signed_auto_t<In>;
      float v = flt::to_float(x);
      if (v < 0.0f)
        return std::expected<Out, errc>{std::unexpected(errc::domain_error)};
      return std::expected<Out, errc>{store<Out>(detail::d_sqrt(v))};
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto exp2(In x)
    { static_assert(mdetail::require_snap<In>()); return exp2_core<mdetail::exp2_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto log2(In x)
    {
      static_assert(mdetail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::flt::log2: input must be strictly positive");
      return log2_core<mdetail::log2_auto_t<In>>(x);
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto exp(In x)
    { static_assert(mdetail::require_snap<In>()); return exp_core<mdetail::exp_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto log(In x)
    {
      static_assert(mdetail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::flt::log: input must be strictly positive");
      return log_core<mdetail::log_auto_t<In>>(x);
    }

    template <imax Base, insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto pow_base(In x)
    {
      static_assert(mdetail::require_snap<In>());
      using Out = mdetail::pow_base_auto_t<Base, In>;
      return store<Out>(detail::d_pow(static_cast<float>(Base), flt::to_float(x)));
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sin(In angle)
    { static_assert(mdetail::require_snap<In>()); return sin_core<mdetail::sin_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto cos(In angle)
    { static_assert(mdetail::require_snap<In>()); return cos_core<mdetail::cos_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto tan(In angle)
    {
      static_assert(mdetail::require_snap<In>());
      using Out = mdetail::tan_auto_t<In>;
      float t;
      if (!detail::d_tan(flt::to_float(angle), t))
        return std::expected<Out, errc>{std::unexpected(errc::division_by_zero)};
      if constexpr (!has_flag(InsidePolicy<Out>, clamp))   // clamp Out: saturate below
        if (t < mdetail::lower_fp<float, Out> || t > mdetail::upper_fp<float, Out>)
          return std::expected<Out, errc>{std::unexpected(errc::overflow)};
      return std::expected<Out, errc>{store<Out>(t)};
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto atan2(In y, In x)
    { static_assert(mdetail::require_snap<In>()); return atan2_core<mdetail::atan2_auto_t<In>>(y, x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto atan(In x)
    { static_assert(mdetail::require_snap<In>()); return atan_core<mdetail::atan_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto asin(In x)
    { static_assert(mdetail::require_snap<In>()); return asin_core<mdetail::asin_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto acos(In x)
    { static_assert(mdetail::require_snap<In>()); return acos_core<mdetail::acos_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sinh(In x)
    { static_assert(mdetail::require_snap<In>()); return sinh_core<mdetail::sinh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto cosh(In x)
    { static_assert(mdetail::require_snap<In>()); return cosh_core<mdetail::cosh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto tanh(In x)
    { static_assert(mdetail::require_snap<In>()); return tanh_core<mdetail::tanh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto log10(In x)
    {
      static_assert(mdetail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::flt::log10: input must be strictly positive");
      return log10_core<mdetail::log10_auto_t<In>>(x);
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto cbrt(In x)
    { static_assert(mdetail::require_snap<In>()); return cbrt_core<mdetail::cbrt_auto_t<In>>(x); }

    template <insidable InX, insidable InY>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto hypot(InX x, InY y)
    {
      static_assert(mdetail::require_snap<InX>() && mdetail::require_snap<InY>());
      return hypot_core<mdetail::hypot_auto_t<InX, InY>>(x, y);
    }

    template <insidable InB, insidable InE>
      requires (Lower<InB> > rational{0})
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto pow(InB base, InE exp)
    {
      static_assert(mdetail::require_snap<InB>() && mdetail::require_snap<InE>());
      using Out = mdetail::pow_auto_t<InB, InE>;
      float b = flt::to_float(base);
      if (b <= 0.0f)
        return std::expected<Out, errc>{std::unexpected(errc::domain_error)};
      float r = detail::d_pow(b, flt::to_float(exp));
      if constexpr (!has_flag(InsidePolicy<Out>, clamp))   // clamp Out: saturate below
        if (r < mdetail::lower_fp<float, Out> || r > mdetail::upper_fp<float, Out>)
          return std::expected<Out, errc>{std::unexpected(errc::overflow)};
      return std::expected<Out, errc>{store<Out>(r)};
    }
  } // namespace flt
#endif // !BEMAN_INSIDE_MATH_NO_FP

  //---------------------------------------------------------------------------
  // The unqualified API (`beman::inside::math::sin` etc.) is the build's default
  // engine: CORDIC under BEMAN_INSIDE_MATH_NO_FP, float under
  // BEMAN_INSIDE_MATH_FLOAT, else double. Every engine stays reachable by name.
  // Trig takes radians (std::sin-shaped; the turn-input workers are internal).
  // sqrt of a mixed-sign input, tan and pow return std::expected<inside, errc>
  // (domain_error / division_by_zero / overflow) instead of UB.
  //---------------------------------------------------------------------------
#if defined(BEMAN_INSIDE_MATH_NO_FP)
  namespace default_engine = cordic;
#elif defined(BEMAN_INSIDE_MATH_FLOAT)
  namespace default_engine = flt;
#else
  namespace default_engine = dbl;
#endif
  using default_engine::sqrt;
  using default_engine::exp2;
  using default_engine::log2;
  using default_engine::exp;
  using default_engine::log;
  using default_engine::pow_base;
  using default_engine::sin;
  using default_engine::cos;
  using default_engine::tan;
  using default_engine::atan2;
  using default_engine::atan;
  using default_engine::asin;
  using default_engine::acos;
  using default_engine::sinh;
  using default_engine::cosh;
  using default_engine::tanh;
  using default_engine::log10;
  using default_engine::cbrt;
  using default_engine::hypot;
  using default_engine::pow;
}

#endif
