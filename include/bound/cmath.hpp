//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDcmathHPP
#define BNDcmathHPP

#include "bound/bound.hpp"
#include "bound/cmath_double.hpp"   // the default (double) math engine

#include "slim/expected.hpp"     // slim::expected, slim::unexpected

#include <array>
#include <bit>

// The public bnd::math::* functions dispatch to the double engine (default) or
// the integer/CORDIC engine (`-DBOUND_MATH_FIXED`). Only the integer engine is
// `constexpr` (the double engine uses std::fma, not constexpr before C++23), so
// the public functions are `constexpr` only in the integer build.
#ifdef BND_MATH_FIXED
#  define BND_MATH_FN constexpr
#else
#  define BND_MATH_FN
#endif

//---------------------------------------------------------------------------
// bnd::math — one transcendental API, two interchangeable engines.
//
// =====================================================================
//   TWO ENGINES, SELECTED AT BUILD TIME
// =====================================================================
// `bnd::math::sin/cos/exp/...` present a single API; the engine is chosen by
// the `BND_MATH_FIXED` macro (CMake `-DBOUND_MATH_FIXED=ON`):
//
//   * DEFAULT — the double engine (`cmath_double.hpp`). Hardware `double`
//     polynomials (`std::fma`, hex-float coeffs, Cody-Waite reduction) on
//     double-backed (`real`) bounds. Reproducible *under the IEEE-754 contract*:
//     bit-identical on any IEEE-754 binary64 platform built WITHOUT
//     `-ffast-math`. Fast (~ns); requires an FPU. The math functions are
//     runtime here (std::fma isn't constexpr before C++23).
//
//   * `BND_MATH_FIXED` — the integer/CORDIC engine (this file's cores). The
//     contract below: UNCONDITIONALLY bit-identical (any platform/flags, no
//     FPU). FPU-free and `constexpr`. For embedded / maximum portability.
//
// The two are feature-equivalent: same function set, signatures, and domains.
// The integer-engine bit-exact contract that follows applies to the
// `BND_MATH_FIXED` build (tested by test_cmath.cpp); the double engine's
// contract lives at the top of cmath_double.hpp (tested by test_cmath_double).
//
// =====================================================================
//   INTEGER (CORDIC) ENGINE — BIT-EXACT REPRODUCIBILITY CONTRACT
// =====================================================================
// Every function below must produce bit-identical output for the same input
// across compiler (gcc/clang/msvc), platform (x86/ARM/RISC-V/WASM),
// optimisation level, and build flags (`-ffast-math`, FMA on/off, strict vs.
// fast FP, etc.). Downstream code relies on this for fuzzing corpora,
// record-and-replay debugging, networked deterministic simulation, and
// cross-platform regression testing.
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
//   - Pick a working scale 2^W from the OUTPUT grid (`detail::working_bits`),
//     so precision follows the grid rather than a fixed tier.
//   - Range-reduce the input to a small interval using integer ops.
//   - Evaluate with the shared shift-add CORDIC engine (circular → sin/cos/atan,
//     hyperbolic → exp/log) or Newton (sqrt/cbrt). The only multiplies are in
//     the compile-time tables and input scaling, via the portable wide `fmul`.
//   - Quantize the result to `Out`'s grid through the existing `bound`
//     assignment policy (round / clamp / etc. — caller's choice).
//
// =====================================================================
//---------------------------------------------------------------------------
namespace bnd::math
{
  namespace detail
  {
    // High-precision rational source for the irrational constants — internal,
    // because the fixed-point cores need the exact rational form. Bit-identical across
    // platforms via constexpr rational arithmetic.
    inline constexpr bnd::detail::rational pi_r{1068966896, 340262731};
    inline constexpr bnd::detail::rational two_pi_r =
        bnd::detail::rational::mul_unchecked(pi_r, bnd::detail::rational{2});

    // Every transcendental operand must carry the `real` policy flag. `real`
    // marks a bound as a math operand: under the default engine it selects
    // double-backed storage (fast, no integer-index I/O) on a dyadic grid;
    // under BND_MATH_FIXED it is integer round_nearest. Requiring it on the
    // public surface keeps the two engines' call sites identical and prevents
    // silently routing math through the slow integer-I/O path. Pure grid ops
    // (abs/floor/ceil/round/trunc/fmod) do NOT require it — they have no
    // engine and act on any bound.
    template <boundable In>
    consteval bool require_real() noexcept
    {
      static_assert((BoundPolicy<In> & real) == real,
          "bnd::math: transcendental operand must carry the `real` policy — "
          "declare it as bound<G, real> (or add `| real` to its policy).");
      return true;
    }
  }

  // Public irrational constants as POINT-BOUNDS, so they compose directly in
  // bound-space (`angle * math::pi`) with no rational on the surface.
  inline constexpr auto pi     = just<detail::pi_r>;
  inline constexpr auto two_pi = just<detail::two_pi_r>;

  namespace detail
  {
    // Internal turn-phase shape: Q.N turns, period
    // implicit in the unsigned raw's modular wrap. The public sin/cos/tan
    // accept radians and route through this shape internally; callers do
    // not construct it directly. Phase-accumulator patterns (e.g.
    // `examples/oscillator.cpp`) use a plain integer counter for the same
    // free-modular-wrap property and convert to radians at the sin call.
    template <int N>
    using turns_t = bound<{{bnd::detail::rational{0},
                            bnd::detail::rational{(imax{1} << N) - 1, imax{1} << N}},
                           notch<1, (imax{1} << N)>}>;


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
      return log2_pow2(bnd::detail::abs_den(Notch<In>.Denominator));
    }();

    // Forward declarations of the grid-scaled CORDIC engine pieces the
    // turn-input workers below rely on (the engine itself is defined further
    // down, after the radians sin/cos). Templates, so a declaration here is
    // enough for the instantiation points later in the TU.
    template <boundable Out> constexpr int working_bits() noexcept;
    template <int W, int N> constexpr bnd::detail::rational sin_from_turn_fixed(imax turn_w) noexcept;
    template <int W, int N> constexpr bnd::detail::rational cos_from_turn_fixed(imax turn_w) noexcept;
    template <boundable Out> constexpr Out store_grid(bnd::detail::rational r) noexcept;

    // sin (turn-input, internal). Q.N turn-phase → amplitude on `Out`'s grid,
    // via the grid-scaled CORDIC engine. The input is `detail::turns_t<N>`-
    // shaped; the raw value already plays the role of `phase mod 1` via the
    // underlying unsigned wrap. Rescales the Q.N phase to the engine's working
    // scale and runs the shared `sin_from_turn_fixed` reducer.
    template <boundable Out, boundable In>
    [[nodiscard]] constexpr Out sin_turn_impl(In phase) noexcept
    {
      constexpr int N = turn_bits<In>;
      static_assert(N >= 2 && N <= 30, "bnd::math: turn-phase N must be in [2, 30]");
      static_assert(Lower<Out> <= -1 && Upper<Out> >= 1,
                    "bnd::math: Out must cover [-1, 1]");

      constexpr int W = working_bits<Out>();
      imax raw    = bnd::detail::raw_imax(phase);                       // Q.N turn
      imax turn_w = (W >= N) ? (raw << (W - N)) : (raw >> (N - W));       // → Q.W
      return store_grid<Out>(sin_from_turn_fixed<W, W>(turn_w));
    }

    // cos (turn-input, internal). cos(x) = sin(x + π/2) — shift the phase
    // by one quarter-turn (modular wrap on the raw) and reuse sin. The
    // shift is integer-exact, no precision cost at this tier.
    template <boundable Out, boundable In>
    [[nodiscard]] constexpr Out cos_turn_impl(In phase) noexcept
    {
      constexpr int  N            = turn_bits<In>;
      constexpr imax full_mask    = (imax{1} << N) - 1;
      constexpr imax quarter_turn = imax{1} << (N - 2);

      In shifted = In::from_raw(bnd::detail::raw_cast<In>(
          (bnd::detail::raw_imax(phase) + quarter_turn) & full_mask));
      return sin_turn_impl<Out>(shifted);
    }

    //=========================================================================
    // Grid-scaled CORDIC engine (replacement for the fixed Q.30 cores).
    //
    // Values cross the API as `rational`; internally we work in binary
    // fixed-point at a scale 2^W chosen from the OUTPUT grid (`working_bits`),
    // so precision follows the grid instead of a hardcoded 30 bits. The CORDIC
    // iteration is pure shift-add (bounded, overflow-free). The only multiplies
    // live in the compile-time table/gain derivation and the input scaling, and
    // use the wide multiply `fmul`. Where a 128-bit integer type is available
    // it is used directly; the portable 32-bit-split fallback computes the
    // identical 128-bit product, so the result is bit-identical on every
    // toolchain (MSVC included).
    //=========================================================================

    // (a·b) >> W, exact for full 64-bit operands (the full 128-bit unsigned
    // product is formed, shifted, then re-signed). Magnitude-truncating (toward
    // zero). The two paths below are bit-for-bit equal by construction.
    constexpr imax fmul(imax a, imax b, int W) noexcept
    {
      bool neg = (a < 0) ^ (b < 0);
      umax ua = (a < 0) ? -static_cast<umax>(a) : static_cast<umax>(a);
      umax ub = (b < 0) ? -static_cast<umax>(b) : static_cast<umax>(b);
#if defined(__SIZEOF_INT128__)
      // gcc/clang: native 128-bit, constexpr-friendly.
      umax r = static_cast<umax>((static_cast<unsigned __int128>(ua) * ub) >> W);
#else
      // portable: 32-bit split → 128-bit (hi:lo) → shift. Also the MSVC path.
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

    // round(v · 2^W)  and  x / 2^W — the scale-W marshalling (parametric Q.W).
    // The product num·2^W is formed at 128 bits (native or shift-subtract
    // portable, bit-for-bit equal), so a reduced numerator near 2^63 cannot
    // wrap — the previous rational multiply silently overflowed for
    // numerators ≥ 2^(63−W) (e.g. hypot_endpoint fed full-precision-double
    // ratios). Rounding is half-away-from-zero, matching rational::round().
    constexpr imax to_fixed(bnd::detail::rational v, int W) noexcept
    {
      const umax n = v.Numerator;
      const umax d = bnd::detail::abs_den(v.Denominator);
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
      return (v.Denominator < 0) ? -static_cast<imax>(q) : static_cast<imax>(q);
    }
    constexpr bnd::detail::rational fixed_to_rational(imax x, int W) noexcept
    { return bnd::detail::rational{x, imax{1} << W}; }

    // Grids eligible for the GCD-free store: unit-numerator notch, non-rational
    // storage, raw fits imax, assigned with round_nearest. (Unlike the Q-format
    // fast path this does NOT require integer Lower — Lower·K is an exact
    // integer by the grid invariant regardless.) The math results all carry a
    // power-of-two denominator, so the offset index is formed with integer
    // shifts + round-half-up — identical to the rational assignment path
    // (round_quotient round_nearest is `(num+den/2)/den`, invariant under
    // fraction reduction), but skipping `(value−Lower)/Notch`'s GCD reductions.
    template <boundable Out>
    inline constexpr bool grid_fast_store =
        Notch<Out>.Numerator == 1
        && !bnd::detail::rational_raw<Out>
        && (BoundPolicy<Out> & round_nearest) == round_nearest
        && (std::signed_integral<bnd::detail::raw_t<Out>>
            || bnd::detail::NotchCount<Out>
                 <= static_cast<umax>(std::numeric_limits<imax>::max()));

    // Store a power-of-two-denominator result (the shape every core returns)
    // onto Out's grid. Fast path: pure integer. Fallback: the general rational
    // assignment (handles non-fast grids, clamp/wrap on out-of-range, etc).
    template <boundable Out>
    constexpr Out store_grid(bnd::detail::rational r) noexcept
    {
      if constexpr (grid_fast_store<Out>)
      {
        umax den = bnd::detail::abs_den(r.Denominator);
        if ((den & (den - 1)) == 0)                       // power-of-two denom
        {
          int  D   = std::countr_zero(den);
          imax num = (r.Denominator < 0) ? -r.Numerator
                                         :  r.Numerator;
          constexpr imax K = bnd::detail::abs_den(Notch<Out>.Denominator);  // 1/notch
          constexpr imax m = (Lower<Out> * bnd::detail::rational{K}).value().trunc(); // Lower·K (exact int)
          // K·num + half must fit imax (a wide-denominator r, e.g. hypot's
          // 2^46, would wrap K·num and silently store `value mod 2^k`).
          constexpr imax lim = std::numeric_limits<imax>::max() / 2 / K;
          if (-lim <= num && num <= lim)
          {
            imax half = (D > 0) ? (imax{1} << (D - 1)) : 0;
            imax off  = ((K * num + half) >> D) - m;      // round-half-up((value−Lower)·K)
            if (off >= 0 && static_cast<umax>(off) <= bnd::detail::NotchCount<Out>)
              return Out::from_raw(bnd::detail::raw_from_offset<Out>(static_cast<umax>(off)));
          }
        }
      }
      return Out{r};
    }

    // Working scale for an output grid: enough fractional bits to resolve the
    // notch, PLUS the integer bits of the largest output magnitude (a result of
    // value V at fixed scale 2^W carries absolute error ~V·2^-W, so large
    // outputs — e.g. pow's 10^k — need the extra headroom), plus guard bits for
    // the CORDIC residual (~16 ULP in the prototype). Capped at 31 (int64).
    template <boundable Out>
    constexpr int working_bits() noexcept
    {
      constexpr int GUARD = 6;
      umax den        = bnd::detail::abs_den(Notch<Out>.Denominator);   // 1/notch
      int  notch_bits = (den <= 1) ? 0 : std::bit_width(den - 1);
      imax hi  = bnd::detail::abs(Upper<Out>).ceil();
      imax lo  = bnd::detail::abs(Lower<Out>).ceil();
      imax mag = (hi > lo) ? hi : lo;
      int  int_bits = (mag <= 1) ? 0 : std::bit_width(static_cast<umax>(mag));
      int  W = notch_bits + int_bits + GUARD;
      return (W < 12) ? 12 : (W > 31) ? 31 : W;
    }

    // atan(2^-i) in RADIANS at scale 2^W. i=0 is π/4 (exact, from pi_r); i≥1
    // uses the fast-converging series atan(z)=z−z³/3+z⁵/5−… for tiny z=2^-i.
    constexpr imax atan_pow2_fixed(int i, int W) noexcept
    {
      if (i == 0)
        return to_fixed(bnd::detail::rational::mul_unchecked(
                          pi_r, bnd::detail::rational{1, 4}), W);
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

    // Newton iterations to converge rsqrt to W bits from the y=1 seed (a∈[1,2],
    // ~2-bit start, quadratic): ≈ ceil(log2 W) + 1.
    constexpr int rsqrt_iters(int W) noexcept
    { int n = 3; while ((1 << n) < W) ++n; return n + 1; }

    // 1/sqrt(a) at scale 2^W for a ∈ [1,2], division-free Newton (y←y(3−ay²)/2).
    // `iters` lets the runtime sqrt path scale work with W while the
    // compile-time CORDIC gains stay at a fixed high count.
    constexpr imax rsqrt_fixed(imax a, int W, int iters) noexcept
    {
      imax one = imax{1} << W, three = 3 * one, y = one;
      for (int k = 0; k < iters; ++k) {
        imax ay2 = fmul(a, fmul(y, y, W), W);
        y = fmul(y, three - ay2, W) >> 1;
      }
      return y;
    }

    // √2 as a rational (literal source, like pi_r / ln2_r), for sqrt's odd-
    // exponent step.
    inline constexpr bnd::detail::rational sqrt2_r{1414213562, 1000000000};

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
      imax sm   = fmul(m_w, rsqrt_fixed(m_w, W, rsqrt_iters(W)), W);        // √m · 2^W
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
    constexpr bnd::detail::rational sin_from_turn_fixed(imax turn_w) noexcept
    {
      imax one_turn = imax{1} << W, half = imax{1} << (W - 1), quarter = imax{1} << (W - 2);
      turn_w &= (one_turn - 1);                      // wrap into [0,1) turn
      bool flip = (turn_w & half) != 0;
      turn_w &= (half - 1);
      if (turn_w > quarter) turn_w = half - turn_w;  // reflect about π/4
      // Exact zero at multiples of a half-turn: CORDIC leaves a ~1-ULP residual
      // at angle 0, but sin(kπ) must be exactly 0 (pole detection in tan relies
      // on it). Quadrant peaks (turn_w == quarter) round to ±1 on the grid.
      if (turn_w == 0) return bnd::detail::rational{0};
      imax rad = fmul(turn_w, to_fixed(two_pi_r, W), W);
      imax s, c;
      cordic_sincos<W, N>(rad, s, c);
      return fixed_to_rational(flip ? -s : s, W);
    }
    template <int W, int N>
    constexpr bnd::detail::rational cos_from_turn_fixed(imax turn_w) noexcept
    { return sin_from_turn_fixed<W, N>(turn_w + (imax{1} << (W - 2))); }

    // 1/(2π) as a rational, for radians→turn reduction at any scale.
    inline constexpr bnd::detail::rational inv_two_pi =
      (bnd::detail::rational{1} / two_pi_r).value();

    // CORDIC circular vectoring: atan2(y, x) in RADIANS at scale 2^W. Pre: x > 0
    // (caller pre-rotates the other quadrants). Rotates (x, y) toward the +x
    // axis, accumulating the radians atan table. Generalizes the former Q.30
    // vectoring; the gain cancels in the y/x ratio so no prescale needed.
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

    // Reference precision for compile-time interval derivation and for the
    // composed endpoints (sinh/cosh/tanh/log10/cbrt/pow). The runtime impls use
    // working_bits<Out> so the *value* still follows the grid; this only bounds
    // the derived intervals and the intermediate composition.
    inline constexpr int kRefBits = 30;

    // ln 2 as a rational (10-digit literal, like pi_r), plus
    // its reciprocal — for the exp/log range reduction and base changes.
    inline constexpr bnd::detail::rational ln2_r{6931471806, 10000000000};
    inline constexpr bnd::detail::rational inv_ln2_r = (bnd::detail::rational{1} / ln2_r).value();

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
    constexpr bnd::detail::rational exp2_from_fixed(imax x_w) noexcept
    {
      imax k   = (x_w + (imax{1} << (W - 1))) >> W;        // round to nearest int
      imax f_w = x_w - (k << W);                            // ∈ [−2^(W−1), 2^(W−1)]
      imax fr_w = fmul(f_w, to_fixed(ln2_r, W), W);         // f·ln2 (natural)
      imax er_w;
      if (fr_w == 0) er_w = imax{1} << W;                   // 2^k exactly
      else { imax sh, ch; cordic_sinhcosh<W, hyp_len(W)>(fr_w, sh, ch); er_w = sh + ch; }
      if (k <= W) return bnd::detail::rational{static_cast<umax>(er_w), imax{1} << (W - k)};
      return bnd::detail::rational{static_cast<umax>(er_w) << (k - W), 1};
    }

    // ln(w) at scale 2^W as a fixed-point imax. Leading-bit reduce w = 2^e·m,
    // m ∈ [1,2); ln(w) = e·ln2 + 2·atanh((m−1)/(m+1)). Pre: w > 0.
    template <int W>
    constexpr imax ln_to_fixed(bnd::detail::rational w) noexcept
    {
      imax w_w  = to_fixed(w, W);
      int  lead = 63 - std::countl_zero(static_cast<umax>(w_w));
      int  e    = lead - W;
      imax one  = imax{1} << W;
      imax m_w  = (e >= 0) ? (w_w >> e) : (w_w << (-e));   // m·2^W ∈ [2^W, 2^(W+1))
      imax z    = cordic_atanh_vec<W, hyp_len(W)>(m_w + one, m_w - one);
      return e * to_fixed(ln2_r, W) + 2 * z;
    }

    // log2(x) at scale 2^W as imax: ln(x)·log2(e).
    template <int W>
    constexpr imax log2_to_fixed(bnd::detail::rational x) noexcept
    { return fmul(ln_to_fixed<W>(x), to_fixed(inv_ln2_r, W), W); }

    // e^(v_w / 2^W) as a rational: 2^(v·log2 e).
    template <int W>
    constexpr bnd::detail::rational exp_from_fixed(imax v_w) noexcept
    { return exp2_from_fixed<W>(fmul(v_w, to_fixed(inv_ln2_r, W), W)); }

    // Rational-input wrappers (inputs are small-denominator values; fine to
    // marshal through to_fixed). pow/cbrt compose via the *_fixed primitives
    // above instead, to avoid rational-denominator blow-up.
    template <int W>
    constexpr bnd::detail::rational exp_fixed(bnd::detail::rational v) noexcept
    { return exp_from_fixed<W>(to_fixed(v, W)); }
    template <int W>
    constexpr bnd::detail::rational ln_fixed(bnd::detail::rational w) noexcept
    { return fixed_to_rational(ln_to_fixed<W>(w), W); }
    template <int W>
    constexpr bnd::detail::rational exp2_fixed(bnd::detail::rational x) noexcept
    { return exp2_from_fixed<W>(to_fixed(x, W)); }
    template <int W>
    constexpr bnd::detail::rational log2_fixed(bnd::detail::rational x) noexcept
    { return fixed_to_rational(log2_to_fixed<W>(x), W); }

    // 1/ln10 at scale 2^W — for log10 = ln·(1/ln10), composed in fixed-point.
    template <int W>
    constexpr imax inv_ln10_fixed() noexcept
    {
      // ln10 at scale 2^W, then reciprocal via division-free… simplest: derive
      // the rational once and marshal. ln10 ≈ 2.302585; small, no overflow.
      return to_fixed((bnd::detail::rational{1} /
                       fixed_to_rational(ln_to_fixed<W>(bnd::detail::rational{10}), W)).value(), W);
    }
  } // namespace detail

  // sin: radians-valued bound → amplitude on the auto-deduced output grid
  // (`detail::sin_auto_t<In>` = `bound<{-1, 1}, Notch<In>, … | round_nearest>`).
  // Input is interpreted as radians — the std::sin equivalent. Internally
  // converts to a turn (via `× 1/(2π)`) at the grid-derived working scale and
  // runs the same circular-CORDIC reducer as the turn-input path.
  //
  // Restrict |angle| ≤ 1024 rad (~163 cycles) so the fixed-point scaling stays
  // inside int63. Wider angles need a different normalization step
  // (deferred).
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out sin_impl(In angle) noexcept
  {
    static_assert(Lower<In> >= -1024 && Upper<In> <= 1024,
                  "bnd::math::sin: input must be in [-1024, 1024] rad");
    static_assert(Lower<Out> <= -1 && Upper<Out> >= 1,
                  "bnd::math::sin: Out must cover [-1, 1]");

    constexpr int W = detail::working_bits<Out>();
    bnd::detail::rational a = angle;
    imax a_w    = detail::to_fixed(a, W);                          // radians · 2^W
    imax turn_w = detail::fmul(a_w, detail::to_fixed(detail::inv_two_pi, W), W);
    return detail::store_grid<Out>(detail::sin_from_turn_fixed<W, W>(turn_w));
  }

  // cos: radians-valued bound → amplitude. cos(x) = sin(x + π/2); add a
  // quarter-turn to the converted turn before the quadrant
  // reducer, same precision as sin.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out cos_impl(In angle) noexcept
  {
    static_assert(Lower<In> >= -1024 && Upper<In> <= 1024,
                  "bnd::math::cos: input must be in [-1024, 1024] rad");
    static_assert(Lower<Out> <= -1 && Upper<Out> >= 1,
                  "bnd::math::cos: Out must cover [-1, 1]");

    constexpr int W = detail::working_bits<Out>();
    bnd::detail::rational a = angle;
    imax a_w    = detail::to_fixed(a, W);                          // radians · 2^W
    imax turn_w = detail::fmul(a_w, detail::to_fixed(detail::inv_two_pi, W), W);
    return detail::store_grid<Out>(detail::cos_from_turn_fixed<W, W>(turn_w));
  }

  namespace detail
  {
    // tan (turn-input, internal). sin/cos from the grid-scaled engine, divided
    // with a pole guard. Returns `unexpected(errc::division_by_zero)` when the
    // phase lands on a pole (cos == 0) and `unexpected(errc::overflow)` when the
    // result exceeds Out's range.
    template <boundable Out, boundable In>
    [[nodiscard]] constexpr slim::expected<Out, errc> tan_turn_impl(In phase) noexcept
    {
      constexpr int N = turn_bits<In>;
      static_assert(N >= 2 && N <= 30, "bnd::math: turn-phase N must be in [2, 30]");

      constexpr int W = working_bits<Out>();
      imax raw    = bnd::detail::raw_imax(phase);
      imax turn_w = (W >= N) ? (raw << (W - N)) : (raw >> (N - W));

      bnd::detail::rational sin_v = sin_from_turn_fixed<W, W>(turn_w);
      bnd::detail::rational cos_v = cos_from_turn_fixed<W, W>(turn_w);
      if (cos_v == 0) return slim::unexpected(errc::division_by_zero);

      bnd::detail::rational tan_v = (sin_v / cos_v).value();
      if (tan_v < Lower<Out> || tan_v > Upper<Out>)
        return slim::unexpected(errc::overflow);

      return detail::store_grid<Out>(tan_v);
    }
  } // namespace detail

  // tan: radians-valued bound → amplitude, with pole guard. sin and cos
  // are computed from the radians input (same conversion as
  // public sin/cos) and divided. Returns
  // `unexpected(errc::division_by_zero)` if cos rounds to exactly 0 (the
  // input landed on a pole modulo the reduction), and
  // `unexpected(errc::overflow)` when the result exceeds Out's interval.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr slim::expected<Out, errc> tan_impl(In angle) noexcept
  {
    static_assert(Lower<In> >= -1024 && Upper<In> <= 1024,
                  "bnd::math::tan: input must be in [-1024, 1024] rad");

    constexpr int W = detail::working_bits<Out>();
    bnd::detail::rational a = angle;
    imax a_w    = detail::to_fixed(a, W);
    imax turn_w = detail::fmul(a_w, detail::to_fixed(detail::inv_two_pi, W), W);

    bnd::detail::rational sin_v = detail::sin_from_turn_fixed<W, W>(turn_w);
    bnd::detail::rational cos_v = detail::cos_from_turn_fixed<W, W>(turn_w);

    if (cos_v == 0) return slim::unexpected(errc::division_by_zero);

    bnd::detail::rational tan_v = (sin_v / cos_v).value();
    if (tan_v < Lower<Out> || tan_v > Upper<Out>)
      return slim::unexpected(errc::overflow);

    return detail::store_grid<Out>(tan_v);
  }


  // log2: positive bound → bound. log2(x) = ln(x)·log2(e) via the grid-scaled
  // hyperbolic-CORDIC `log2_fixed` core (leading-bit reduction + atanh vectoring).
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out log2_impl(In x) noexcept
  {
    static_assert(Lower<In> > 0,
                  "bnd::math::log2: input must be strictly positive");

    return detail::store_grid<Out>(detail::log2_fixed<detail::working_bits<Out>()>(bnd::detail::rational{x}));
  }

  // exp2: bound → bound, returning 2^x. 2^x = e^(x·ln2) via the grid-scaled
  // hyperbolic-CORDIC `exp2_fixed` core (integer/fractional split + sinh/cosh).
  //
  // Restrict |x| ≤ 30 so the rational denominator 2^(30 - k) fits in int63.
  // The output `Out` must include non-negative values and cover at least
  // [2^Lower<In>, 2^Upper<In>] — anything narrower needs `clamp` to absorb
  // overflow at the assignment.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out exp2_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -30 && Upper<In> <= 30,
                  "bnd::math::exp2: input must be in [-30, 30]");
    static_assert(Lower<Out> >= 0,
                  "bnd::math::exp2: Out must be non-negative");

    return detail::store_grid<Out>(detail::exp2_fixed<detail::working_bits<Out>()>(bnd::detail::rational{x}));
  }

  // exp: thin wrapper. exp(x) = exp2(x · log2(e)). The scaling factor
  // log2(e) ≈ 1.4427, so x must stay inside [-30/log2(e), 30/log2(e)] ≈
  // [-20.79, 20.79] for exp2's denominator-shift envelope. We use [-20, 20].
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out exp_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -20 && Upper<In> <= 20,
                  "bnd::math::exp: input must be in [-20, 20]");
    static_assert(Lower<Out> >= 0,
                  "bnd::math::exp: Out must be non-negative");

    return detail::store_grid<Out>(detail::exp_fixed<detail::working_bits<Out>()>(bnd::detail::rational{x}));
  }

  // log: thin wrapper. log(x) = log2(x) · ln(2). Result precision matches
  // log2 minus 1-2 ULP from the final fixed-point scaling.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out log_impl(In x) noexcept
  {
    static_assert(Lower<In> > 0,
                  "bnd::math::log: input must be strictly positive");

    return detail::store_grid<Out>(detail::ln_fixed<detail::working_bits<Out>()>(bnd::detail::rational{x}));
  }

  // pow_base<Base>(x) = Base^x for compile-time-known integer Base ≥ 2.
  // Implemented as exp2(x · log2(Base)) with log2(Base) from the grid-scaled
  // `log2_to_fixed` core — no hand-typed magic constants.
  // For Base = 10, this is the building block for `db_to_linear`.
  template <imax Base, boundable Out, boundable In>
  [[nodiscard]] constexpr Out pow_base_impl(In x) noexcept
  {
    static_assert(Base >= 2, "bnd::math::pow_base: Base must be ≥ 2");
    static_assert(Lower<Out> >= 0,
                  "bnd::math::pow_base: Out must be non-negative");

    constexpr int W = detail::working_bits<Out>();
    imax lb_w = detail::log2_to_fixed<W>(bnd::detail::rational{Base});   // log2(Base)·2^W
    imax sc_w = detail::fmul(detail::to_fixed(bnd::detail::rational{x}, W), lb_w, W);
    return detail::store_grid<Out>(detail::exp2_from_fixed<W>(sc_w));
  }


  // atan2: signed bound, signed bound → angle in radians ∈ [-π, π].
  // Implemented as CORDIC vectoring with quadrant pre-rotation. The CORDIC
  // core accumulates a turn-phase internally; the public boundary scales by
  // 2π so the output is radians, consistent with sin/cos/tan and the rest
  // of bnd::math.
  //
  // CORDIC only depends on the y/x ratio: inputs with magnitudes beyond 1
  // are normalized by the larger magnitude (exact rational division) before
  // entering the fixed-point window, so the full plane is accepted. Inputs
  // already inside [-1, 1] skip the division (bit-identical to before).
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out atan2_impl(In y, In x) noexcept
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "bnd::math::atan2: input magnitudes must be \u2264 2^20 for the working-scale envelope");
    static_assert(Lower<Out> <= -detail::pi_r && Upper<Out> >= detail::pi_r,
                  "bnd::math::atan2: Out must cover [-π, π]");

    constexpr int W = detail::working_bits<Out>();
    bnd::detail::rational yv = y, xv = x;
    {
      bnd::detail::rational ay = bnd::detail::abs(yv);
      bnd::detail::rational ax = bnd::detail::abs(xv);
      bnd::detail::rational m  = (ax > ay) ? ax : ay;
      if (m > bnd::detail::rational{1})
      {
        yv = bnd::detail::rational::div_unchecked(yv, m);
        xv = bnd::detail::rational::div_unchecked(xv, m);
      }
    }
    imax y_w = detail::to_fixed(yv, W);
    imax x_w = detail::to_fixed(xv, W);

    // atan2(0, 0) is undefined; convention is 0. Without this guard the
    // CORDIC iteration trivially leaves x, y at zero but still accumulates
    // the angle table at each step, producing garbage.
    if (x_w == 0 && y_w == 0) return Out{0};

    // Quadrant pre-rotation: CORDIC requires x > 0. For x < 0, rotate the
    // vector by ±π/2 (in radians, at scale W) to land in the right half-plane
    // and add the rotation back at the end.
    //   Q2 (x<0, y≥0): (x',y') = (y, −x),  θ = CORDIC + π/2.
    //   Q3 (x<0, y<0): (x',y') = (−y, x),  θ = CORDIC − π/2.
    imax half_pi_w = detail::to_fixed(
        bnd::detail::rational::mul_unchecked(detail::pi_r, bnd::detail::rational{1, 2}), W);
    imax pre_rotation = 0;
    if (x_w < 0) {
      if (y_w >= 0) { imax nx = y_w;  imax ny = -x_w; x_w = nx; y_w = ny; pre_rotation =  half_pi_w; }
      else          { imax nx = -y_w; imax ny =  x_w; x_w = nx; y_w = ny; pre_rotation = -half_pi_w; }
    }

    imax rad = detail::cordic_atan2_rad<W, W>(y_w, x_w) + pre_rotation;   // radians, scale W
    return detail::store_grid<Out>(detail::fixed_to_rational(rad, W));
  }

  //---------------------------------------------------------------------------
  // Algebraic tier — exact, no polynomial machinery. Each function wraps the
  // corresponding `rational` operation and routes through `Out`'s assignment.
  //---------------------------------------------------------------------------

  // |x|. Output Lower must be ≥ 0 (the result is always non-negative).
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out abs_impl(In x) noexcept
  {
    static_assert(Lower<Out> <= 0,
                  "bnd::math::abs: Out must include 0");
    return detail::store_grid<Out>(bnd::detail::abs(bnd::detail::rational{x}));
  }

  // ⌊x⌋ — largest integer ≤ x.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out floor_impl(In x) noexcept
  {
    return detail::store_grid<Out>(bnd::detail::rational{x}.floor());
  }

  // ⌈x⌉ — smallest integer ≥ x.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out ceil_impl(In x) noexcept
  {
    return detail::store_grid<Out>(bnd::detail::rational{x}.ceil());
  }

  // x rounded to nearest integer, half-away-from-zero (matches the existing
  // `rational::round()` convention used throughout the library).
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out round_impl(In x) noexcept
  {
    return detail::store_grid<Out>(bnd::detail::rational{x}.round());
  }

  // x truncated toward zero. Distinct from floor for negative inputs:
  // trunc(-1.7) = -1 vs floor(-1.7) = -2.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out trunc_impl(In x) noexcept
  {
    return detail::store_grid<Out>(bnd::detail::rational{x}.trunc());
  }

  namespace detail
  {
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
    template <boundable Out, boundable InX, boundable InY>
    inline constexpr bool fmod_int_fast = []{
      if (bnd::detail::rational_raw<InX> || bnd::detail::real_raw<InX>
       || bnd::detail::rational_raw<InY> || bnd::detail::real_raw<InY>
       || bnd::detail::rational_raw<Out> || bnd::detail::real_raw<Out>)
        return false;
      if (Notch<InX> == 0 || Notch<InY> == 0 || Notch<Out> == 0)
        return false;
      if (!bnd::detail::DivisorExcludesZero<InY>)
        return false;
      auto go = bnd::detail::gcd(Notch<InX>, Notch<InY>);
      if (!go.has_value()) return false;
      bnd::detail::rational g = *go;
      auto qo = g / Notch<Out>;
      if (!qo.has_value() || bnd::detail::abs_den(qo->Denominator) != 1)
        return false;
      bnd::detail::rational maxx =
          bnd::detail::abs(Lower<InX>) > bnd::detail::abs(Upper<InX>)
            ? bnd::detail::abs(Lower<InX>) : bnd::detail::abs(Upper<InX>);
      bnd::detail::rational maxy =
          bnd::detail::abs(Lower<InY>) > bnd::detail::abs(Upper<InY>)
            ? bnd::detail::abs(Lower<InY>) : bnd::detail::abs(Upper<InY>);
      if (Lower<Out> > -maxy || Upper<Out> < maxy)
        return false;
      constexpr umax lim = static_cast<umax>(std::numeric_limits<imax>::max() / 4);
      auto ux = maxx / g;  auto uy = maxy / g;  auto uo = maxy / Notch<Out>;
      return ux.has_value() && uy.has_value() && uo.has_value()
          && ux->Numerator <= lim && uy->Numerator <= lim && uo->Numerator <= lim;
    }();
  }

  // x mod y = x − ⌊x/y⌋·y (truncated-division convention, matching std::fmod).
  // Result has the sign of x. Pre: y must not span zero (caller-enforced).
  template <boundable Out, boundable InX, boundable InY>
  [[nodiscard]] constexpr Out fmod_impl(InX x, InY y) noexcept
  {
    if constexpr (detail::fmod_int_fast<Out, InX, InY>)
    {
      // One integer remainder in g-units; bit-identical to the rational path.
      constexpr bnd::detail::rational g = *bnd::detail::gcd(Notch<InX>, Notch<InY>);
      constexpr imax wx  = (Notch<InX> / g).value().trunc();
      constexpr imax wy  = (Notch<InY> / g).value().trunc();
      constexpr imax wo  = (g / Notch<Out>).value().trunc();
      constexpr imax lox = (Lower<InX> / g).value().trunc();   // exact: grid invariant
      constexpr imax loy = (Lower<InY> / g).value().trunc();
      constexpr imax loo = (Lower<Out> / Notch<Out>).value().trunc();
      const imax a = bnd::detail::raw_imax(x) * wx
                   + (bnd::detail::index_raw<InX> ? lox : 0);
      const imax b = bnd::detail::raw_imax(y) * wy
                   + (bnd::detail::index_raw<InY> ? loy : 0);
      const imax r = a % b;                                    // |r| < |b|, in Out's range
      return Out::from_raw(bnd::detail::raw_from_offset<Out>(r * wo - loo));
    }
    else
    {
      bnd::detail::rational xv = x;
      bnd::detail::rational yv = y;
      bnd::detail::rational q  = bnd::detail::rational::div_unchecked(xv, yv);
      imax     qt = q.trunc();
      bnd::detail::rational qy = bnd::detail::rational::mul_unchecked(bnd::detail::rational{qt}, yv);
      bnd::detail::rational r  = bnd::detail::rational::add_unchecked(xv, -qy);
      return detail::store_grid<Out>(r);
    }
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
    inline constexpr bnd::detail::rational abs_auto_upper =
      (bnd::detail::abs(Lower<In>) > bnd::detail::abs(Upper<In>))
        ? bnd::detail::abs(Lower<In>) : bnd::detail::abs(Upper<In>);

    template <boundable In>
    using abs_auto_t = bound<{{bnd::detail::rational{0}, abs_auto_upper<In>},
                              Notch<In>}, BoundPolicy<In>>;

    template <boundable In>
    using floor_auto_t = bound<{{bnd::detail::rational{Lower<In>.floor()},
                                  bnd::detail::rational{Upper<In>.floor()}},
                                 notch<1>}, BoundPolicy<In>>;

    template <boundable In>
    using ceil_auto_t = bound<{{bnd::detail::rational{Lower<In>.ceil()},
                                 bnd::detail::rational{Upper<In>.ceil()}},
                                notch<1>}, BoundPolicy<In>>;

    template <boundable In>
    using round_auto_t = bound<{{bnd::detail::rational{Lower<In>.round()},
                                  bnd::detail::rational{Upper<In>.round()}},
                                 notch<1>}, BoundPolicy<In>>;

    template <boundable In>
    using trunc_auto_t = bound<{{bnd::detail::rational{Lower<In>.trunc()},
                                  bnd::detail::rational{Upper<In>.trunc()}},
                                 notch<1>}, BoundPolicy<In>>;

    // (fmod has no auto form: with two boundable inputs, calls of the form
    // `fmod<X>(X_val, X_val)` are ambiguous between the explicit-Out and
    // auto-deduced overloads — C++ partial ordering can't tell them apart
    // because both deduce `X` for their first template parameter. The
    // explicit form is the canonical entry point; users who want auto
    // deduction can construct the output bound from x and y manually.)
  } // namespace detail

  template <boundable In>
  [[nodiscard]] constexpr auto abs(In x) noexcept { return abs_impl<detail::abs_auto_t<In>>(x); }

  template <boundable In>
  [[nodiscard]] constexpr auto floor(In x) noexcept { return floor_impl<detail::floor_auto_t<In>>(x); }

  template <boundable In>
  [[nodiscard]] constexpr auto ceil(In x) noexcept { return ceil_impl<detail::ceil_auto_t<In>>(x); }

  template <boundable In>
  [[nodiscard]] constexpr auto round(In x) noexcept { return round_impl<detail::round_auto_t<In>>(x); }

  template <boundable In>
  [[nodiscard]] constexpr auto trunc(In x) noexcept { return trunc_impl<detail::trunc_auto_t<In>>(x); }

  // sqrt: non-negative bound → bound. Newton-Raphson on grid-scaled integer math,
  // leading-bit initial guess. The input must have Lower == 0, a power-of-2
  // notch denominator (1/2^K with K ≤ 30), and Upper ≤ 4 so the x_q60
  // intermediate fits in int63. The mixed-sign overload below (`signed_impl`)
  // accepts Lower < 0 and returns `slim::optional` — nullopt when the runtime
  // value is negative.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out sqrt_impl(In x) noexcept
  {
    static_assert(Lower<In> == 0,
                  "bnd::math::sqrt: input must start at 0 (use the mixed-sign overload)");
    static_assert(Lower<Out> <= 0,
                  "bnd::math::sqrt: Out must include 0");

    constexpr int W = detail::working_bits<Out>();
    imax a_w = detail::to_fixed(bnd::detail::rational{x}, W);
    return detail::store_grid<Out>(detail::fixed_to_rational(detail::sqrt_fixed<W>(a_w), W));
  }

  // Mixed-sign sqrt: accepts inputs whose interval crosses zero. Returns
  // `unexpected(errc::domain_error)` if the runtime value is negative,
  // otherwise the same result as `sqrt_impl`. The notch and
  // |Upper| / |Lower| constraints mirror the non-negative path.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr slim::expected<Out, errc> sqrt_signed_impl(In x) noexcept
  {
    static_assert(Lower<Out> <= 0,
                  "bnd::math::sqrt: Out must include 0");

    bnd::detail::rational v = bnd::detail::as_rational(x);
    if (v < bnd::detail::rational{0})
      return slim::unexpected(errc::domain_error);

    constexpr int W = detail::working_bits<Out>();
    imax a_w = detail::to_fixed(v, W);
    return detail::store_grid<Out>(detail::fixed_to_rational(detail::sqrt_fixed<W>(a_w), W));
  }

  //---------------------------------------------------------------------------
  // Auto-deducing forms — monotonic transcendental tier (phase 2 of step 10).
  //
  // Each function below takes the same constraints as its explicit-Out
  // counterpart but derives the output bound type from the input's interval
  // and notch:
  //   - Lower / Upper computed by running the engine cores on the
  //     input endpoints at compile time.
  //   - Outward rounding to `Notch<In>` ensures the deduced bound covers
  //     the true mathematical range even when the endpoint is irrational.
  //   - Notch and policy inherited from `In`.
  //---------------------------------------------------------------------------
  namespace detail
  {
    // Round a rational down to the nearest multiple of `notch`.
    constexpr bnd::detail::rational floor_to_notch(bnd::detail::rational x, bnd::detail::rational notch) noexcept
    {
      auto q  = bnd::detail::rational::div_unchecked(x, notch);
      imax n  = q.floor();
      return bnd::detail::rational::mul_unchecked(bnd::detail::rational{n}, notch);
    }

    // Round a rational up to the nearest multiple of `notch`.
    constexpr bnd::detail::rational ceil_to_notch(bnd::detail::rational x, bnd::detail::rational notch) noexcept
    {
      auto q = bnd::detail::rational::div_unchecked(x, notch);
      imax n = q.ceil();
      return bnd::detail::rational::mul_unchecked(bnd::detail::rational{n}, notch);
    }

    // Helpers: evaluate the engine cores on a compile-time-known
    // rational endpoint and return the result as a rational.
    constexpr bnd::detail::rational sqrt_endpoint(bnd::detail::rational v) noexcept
    {
      if (v == 0) return bnd::detail::rational{0};
      return fixed_to_rational(sqrt_fixed<kRefBits>(to_fixed(v, kRefBits)), kRefBits);
    }

    constexpr bnd::detail::rational exp2_endpoint(bnd::detail::rational v) noexcept
    { return exp2_fixed<kRefBits>(v); }

    constexpr bnd::detail::rational log2_endpoint(bnd::detail::rational v) noexcept
    { return log2_fixed<kRefBits>(v); }

    constexpr bnd::detail::rational exp_endpoint(bnd::detail::rational v) noexcept
    { return exp_fixed<kRefBits>(v); }

    constexpr bnd::detail::rational log_endpoint(bnd::detail::rational v) noexcept
    { return ln_fixed<kRefBits>(v); }

    template <imax Base>
    constexpr bnd::detail::rational pow_base_endpoint(bnd::detail::rational v) noexcept
    {
      imax sc_w = fmul(to_fixed(v, kRefBits),
                       log2_to_fixed<kRefBits>(bnd::detail::rational{Base}), kRefBits);
      return exp2_from_fixed<kRefBits>(sc_w);
    }

    // Deduction aliases. Each rounds endpoints outward to Notch<In> and
    // adds `round_nearest` to the policy — transcendental output values
    // come out of the engine cores with sub-notch drift, so the assignment
    // into the deduced bound needs a rounding rule to land on the grid.
    template <boundable In>
    using sqrt_auto_t = bound<{{bnd::detail::rational{0},
                                ceil_to_notch(sqrt_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    // Mixed-sign sqrt: Upper of the result is sqrt of the larger absolute
    // endpoint, since the runtime value can be anywhere in [Lower, Upper].
    template <boundable In>
    inline constexpr bnd::detail::rational sqrt_signed_upper =
        (bnd::detail::abs(Lower<In>) > bnd::detail::abs(Upper<In>))
            ? bnd::detail::abs(Lower<In>) : bnd::detail::abs(Upper<In>);

    template <boundable In>
    using sqrt_signed_auto_t = bound<{{bnd::detail::rational{0},
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
    requires (Lower<In> == bnd::detail::rational{0})
  [[nodiscard]] BND_MATH_FN auto sqrt(In x) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return sqrt_impl<detail::sqrt_auto_t<In>>(x);
#else
    return dbl::sqrt_core<detail::sqrt_auto_t<In>>(x);
#endif
  }

  // Mixed-sign overload: dispatches to `sqrt_signed_impl`, returning
  // `slim::expected<bound, errc>` so a negative runtime value surfaces as
  // `unexpected(errc::domain_error)` instead of UB.
  template <boundable In>
    requires (Lower<In> < bnd::detail::rational{0})
  [[nodiscard]] BND_MATH_FN auto sqrt(In x) noexcept
  {
    static_assert(detail::require_real<In>());
    using Out = detail::sqrt_signed_auto_t<In>;
#ifdef BND_MATH_FIXED
    return sqrt_signed_impl<Out>(x);
#else
    double v = x;
    if (v < 0.0)
      return slim::expected<Out, errc>{slim::unexpected(errc::domain_error)};
    return slim::expected<Out, errc>{Out{dbl::detail::d_sqrt(v)}};
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto exp2(In x) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return exp2_impl<detail::exp2_auto_t<In>>(x);
#else
    return dbl::exp2_core<detail::exp2_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto log2(In x) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return log2_impl<detail::log2_auto_t<In>>(x);
#else
    return dbl::log2_core<detail::log2_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto exp(In x) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return exp_impl<detail::exp_auto_t<In>>(x);
#else
    return dbl::exp_core<detail::exp_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto log(In x) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return log_impl<detail::log_auto_t<In>>(x);
#else
    return dbl::log_core<detail::log_auto_t<In>>(x);
#endif
  }

  template <imax Base, boundable In>
  [[nodiscard]] BND_MATH_FN auto pow_base(In x) noexcept
  {
    static_assert(detail::require_real<In>());
    using Out = detail::pow_base_auto_t<Base, In>;
#ifdef BND_MATH_FIXED
    return pow_base_impl<Base, Out>(x);
#else
    return Out{dbl::detail::d_pow(static_cast<double>(Base), x)};
#endif
  }

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
    template <boundable In>
    using sin_auto_t = bound<{{-bnd::detail::rational{1}, bnd::detail::rational{1}},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using cos_auto_t = sin_auto_t<In>;

    // Output covers [-π, π] rounded outward to notch multiples — the exact
    // ±π endpoints are irrational and would violate the grid's divides-evenly
    // invariant against a rational notch.
    template <boundable In>
    using atan2_auto_t = bound<{{floor_to_notch(-pi_r, Notch<In>),
                                 ceil_to_notch (pi_r, Notch<In>)},
                                Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using tan_auto_t = bound<{{-bnd::detail::rational{1024}, bnd::detail::rational{1024}},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable InX, boundable InY>
    using fmod_auto_t = bound<{{-bnd::detail::abs(Upper<InY>), bnd::detail::abs(Upper<InY>)},
                                Notch<InX>}, BoundPolicy<InX> | round_nearest>;
  } // namespace detail

  // Public auto-form trig — radians input, std::sin-shaped. The only
  // public trig entry points; the turn-input workers live in `detail::`
  // (`sin_turn_impl`, `cos_turn_impl`, `tan_turn_impl`) for internal use.
  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto sin(In angle) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return sin_impl<detail::sin_auto_t<In>>(angle);
#else
    return dbl::sin_core<detail::sin_auto_t<In>>(angle);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto cos(In angle) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return cos_impl<detail::cos_auto_t<In>>(angle);
#else
    return dbl::cos_core<detail::cos_auto_t<In>>(angle);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto atan2(In y, In x) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return atan2_impl<detail::atan2_auto_t<In>>(y, x);
#else
    return dbl::atan2_core<detail::atan2_auto_t<In>>(y, x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto tan(In angle) noexcept
  {
    static_assert(detail::require_real<In>());
    using Out = detail::tan_auto_t<In>;
#ifdef BND_MATH_FIXED
    return tan_impl<Out>(angle);
#else
    double x = angle;
    double c = dbl::detail::d_cos(x);
    if (c == 0.0)
      return slim::expected<Out, errc>{slim::unexpected(errc::division_by_zero)};
    double t = dbl::detail::d_sin(x) / c;
    if (t < static_cast<double>(Lower<Out>) || t > static_cast<double>(Upper<Out>))
      return slim::expected<Out, errc>{slim::unexpected(errc::overflow)};
    return slim::expected<Out, errc>{Out{t}};
#endif
  }

  template <boundable InX, boundable InY>
  [[nodiscard]] constexpr auto fmod(InX x, InY y) noexcept
  { return fmod_impl<detail::fmod_auto_t<InX, InY>>(x, y); }

  //===========================================================================
  // Grid-native periodic trig: circle<M> angle + caller-owned amplitude.
  //
  // A `circle<M>` is one full revolution split into M equal slots, valued in
  // DEGREES. Degrees have an *integer* period (360), so a notch divides the
  // circle exactly and the `wrap` policy is drift-free — unlike radians, whose
  // 2π period no rational notch divides. M selects the angular resolution; the
  // raw storage is simply the slot index 0..M-1.
  //
  // `sin`/`cos` write the result into a caller-supplied amplitude bound whose
  // own grid fixes the output precision — so the work scales with the grids,
  // never a fixed internal tier. The runtime path is a table lookup: no
  // ×1/(2π) conversion and no polynomial. A first-quadrant table is built at
  // compile time (per the reproducibility banner: derived, never runtime-
  // loaded); the CORDIC core only seeds it at build time and never runs at
  // runtime. Power-of-two M is the optimal path (slot reflection is a
  // bitmask); any M divisible by 4 is supported.
  //===========================================================================
  template <std::uint64_t M>
  using circle = bound<{{bnd::detail::rational{0},
                         bnd::detail::rational{std::uint64_t{360} * (M - 1),
                                               static_cast<imax>(M)}},
                        notch<360, static_cast<imax>(M)>}, real | wrap>;

  // Amplitude output grid: [-1, 1] at 1/K resolution. The natural target for
  // `sin(circle<M>, amp<K>&)` — angle precision (M) and amplitude precision (K)
  // are chosen independently.
  template <std::uint64_t K>
  using amp = bound<{{bnd::detail::rational{-1}, bnd::detail::rational{1}},
                     notch<1, static_cast<imax>(K)>}, real>;

  namespace detail
  {
    // First-quadrant sine table for an M-slot circle at working scale 2^W:
    // entry j = sin(j/M turn) for j ∈ [0, M/4], as a rational. Filled at
    // compile time by the grid-scaled CORDIC rotation (`sin_from_turn_fixed`),
    // never evaluated at runtime. Keyed by <M, W> so the entry precision tracks
    // the amplitude grid that selected W.
    template <imax M, int W>
    inline constexpr auto sin_quarter_table = []{
      std::array<bnd::detail::rational, static_cast<std::size_t>(M / 4) + 1> t{};
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
    constexpr bnd::detail::rational sin_slot(imax i) noexcept
    {
      constexpr imax half = M / 2, quarter = M / 4;
      i = ((i % M) + M) % M;                  // wrap into [0, M)
      bool flip = i >= half;
      if (flip) i -= half;                    // sin(π + x) = -sin(x)
      if (i > quarter) i = half - i;          // sin(π - x) =  sin(x)
      bnd::detail::rational mag = sin_quarter_table<M, W>[i];
      return flip ? -mag : mag;
    }

    // Recover the slot count M from a circle-shaped angle: the degree period
    // 360 divided by the notch. The public entry points validate the shape.
    template <boundable DEG>
    inline constexpr imax circle_slots =
      (bnd::detail::rational{360} / Notch<DEG>).value().round();

    // Shared shape check for the circle-input entry points.
    template <boundable DEG>
    constexpr bool valid_circle() noexcept
    {
      static_assert(Lower<DEG> == 0,
                    "bnd::math: circle angle must have Lower 0 (degrees)");
      static_assert((BoundPolicy<DEG> & wrap) == wrap,
                    "bnd::math: circle angle must carry the wrap policy");
      static_assert((BoundPolicy<DEG> & real) == real,
                    "bnd::math: circle angle must carry the `real` policy "
                    "(circle<M> already does; custom angle bounds must add `| real`)");
      static_assert(circle_slots<DEG> % 4 == 0,
                    "bnd::math: circle slot count M must be divisible by 4");
      return true;
    }
  } // namespace detail

  // sin(angle) → out, on out's amplitude grid. angle is a circle<M>; out's
  // grid fixes the result precision. Reference output (not a return value)
  // lets AMP be deduced from the caller's object and reuses its assignment
  // policy for the final rounding onto the amplitude grid.
  template <boundable DEG, boundable AMP>
  BND_MATH_FN void sin(DEG angle, AMP& out) noexcept
  {
    static_assert(detail::valid_circle<DEG>());
#ifdef BND_MATH_FIXED
    constexpr imax M = detail::circle_slots<DEG>;
    constexpr int  W = detail::working_bits<AMP>();
    out = detail::sin_slot<M, W>(bnd::detail::raw_imax(angle));
#else
    out = dbl::detail::d_sin(static_cast<double>(angle) * (dbl::detail::kPi / 180.0));
#endif
  }

  // cos(angle) → out. cos(x) = sin(x + ¼ turn): shift the slot by M/4.
  template <boundable DEG, boundable AMP>
  BND_MATH_FN void cos(DEG angle, AMP& out) noexcept
  {
    static_assert(detail::valid_circle<DEG>());
#ifdef BND_MATH_FIXED
    constexpr imax M = detail::circle_slots<DEG>;
    constexpr int  W = detail::working_bits<AMP>();
    out = detail::sin_slot<M, W>(bnd::detail::raw_imax(angle) + M / 4);
#else
    out = dbl::detail::d_cos(static_cast<double>(angle) * (dbl::detail::kPi / 180.0));
#endif
  }

  // tan(angle) → out = sin/cos. Returns false (and leaves out untouched) when
  // the angle lands exactly on a pole (cos == 0); overflow of the amplitude
  // grid is handled by out's own policy (e.g. clamp).
  template <boundable DEG, boundable AMP>
  [[nodiscard]] BND_MATH_FN bool tan(DEG angle, AMP& out) noexcept
  {
    static_assert(detail::valid_circle<DEG>());
#ifdef BND_MATH_FIXED
    constexpr imax M = detail::circle_slots<DEG>;
    constexpr int  W = detail::working_bits<AMP>();
    imax i = bnd::detail::raw_imax(angle);
    bnd::detail::rational c = detail::sin_slot<M, W>(i + M / 4);
    if (c == 0) return false;                                  // pole
    out = (detail::sin_slot<M, W>(i) / c).value();             // sin / cos
    return true;
#else
    double rad = static_cast<double>(angle) * (dbl::detail::kPi / 180.0);
    double c = dbl::detail::d_cos(rad);
    if (c == 0.0) return false;                                // pole
    out = dbl::detail::d_sin(rad) / c;
    return true;
#endif
  }

  //===========================================================================
  // Extended transcendentals — inverse trig, hyperbolic, log10, pow, cbrt,
  // hypot. Each composes the CORDIC / Newton cores defined above; no new
  // polynomial machinery. Outputs follow the bnd::math conventions: angles in
  // radians, runtime-conditional failures via `slim::expected<Out, errc>`,
  // statically-knowable domain limits via `static_assert`.
  //===========================================================================
  namespace detail
  {
    // --- inverse trig (radians) -------------------------------------------
    // atan(v) in radians at scale 2^W: atan2(v, 1) — x = 1 > 0, so the
    // vectoring CORDIC runs with no pre-rotation. Grid-scaled (no Q.30).
    // Full domain: |v| > 1 reduces via atan(v) = sign(v)·(π/2 − atan(1/|v|)),
    // keeping the CORDIC argument inside its [-1, 1] window. Inputs with
    // |v| ≤ 1 take the original branch unchanged (bit-identical results).
    template <int W>
    constexpr bnd::detail::rational atan_fixed(bnd::detail::rational v) noexcept
    {
      bnd::detail::rational av = bnd::detail::abs(v);
      if (av <= bnd::detail::rational{1})
      {
        imax rad = cordic_atan2_rad<W, W>(to_fixed(v, W), imax{1} << W);
        return fixed_to_rational(rad, W);
      }
      bnd::detail::rational inv =
          bnd::detail::rational::div_unchecked(bnd::detail::rational{1}, av);
      imax rad = cordic_atan2_rad<W, W>(to_fixed(inv, W), imax{1} << W);
      bnd::detail::rational mag = bnd::detail::rational::add_unchecked(
          bnd::detail::rational::mul_unchecked(pi_r, bnd::detail::rational{1, 2}),
          -fixed_to_rational(rad, W));
      return (v < bnd::detail::rational{0}) ? -mag : mag;
    }

    // asin(v) = atan2(v, sqrt(1 − v²)); v ∈ [−1, 1] → result ∈ [−π/2, π/2].
    constexpr bnd::detail::rational asin_endpoint(bnd::detail::rational v) noexcept
    {
      imax one = imax{1} << kRefBits;
      imax v_w = to_fixed(v, kRefBits);
      imax c_w = sqrt_fixed<kRefBits>(one - fmul(v_w, v_w, kRefBits));   // √(1−v²) ≥ 0
      if (c_w == 0) {                                                    // v = ±1 → ±π/2
        bnd::detail::rational half_pi =
            bnd::detail::rational::mul_unchecked(pi_r, bnd::detail::rational{1, 2});
        return (v < bnd::detail::rational{0}) ? -half_pi : half_pi;
      }
      imax rad = cordic_atan2_rad<kRefBits, kRefBits>(v_w, c_w);        // x = c_w > 0
      return fixed_to_rational(rad, kRefBits);
    }

    // acos(v) = π/2 − asin(v); v ∈ [−1, 1] → result ∈ [0, π].
    constexpr bnd::detail::rational acos_endpoint(bnd::detail::rational v) noexcept
    {
      bnd::detail::rational half_pi =
        bnd::detail::rational::mul_unchecked(pi_r, bnd::detail::rational{1, 2});
      return bnd::detail::rational::add_unchecked(half_pi, -asin_endpoint(v));
    }

    // --- hyperbolic (from e^x via the exp core) ---------------------------
    // Combine e^x and e^-x in fixed-point integer space, not in rational space:
    // e^x (x≈10) and e^-x have wildly different denominators (2^16 vs 2^53),
    // and the cross-multiply of `e^x ± e^-x` as rationals overflows imax.
    // At scale kRefBits each term is a single scaled integer (|v| ≤ 10 ⇒ e^|v| ≤ 22027,
    // so e^|v|·2^30 ≤ 2.4e13, well inside int63).
    // sinh/cosh = (e^v ∓ e^-v)/2, combined in fixed-point at kRefBits (the
    // rational form cross-multiplies to ~1e24 and overflows imax).
    constexpr bnd::detail::rational sinh_endpoint(bnd::detail::rational v) noexcept
    {
      imax ex  = to_fixed(exp_fixed<kRefBits>(v),  kRefBits);
      imax enx = to_fixed(exp_fixed<kRefBits>(-v), kRefBits);
      return fixed_to_rational((ex - enx) / 2, kRefBits);
    }

    constexpr bnd::detail::rational cosh_endpoint(bnd::detail::rational v) noexcept
    {
      imax ex  = to_fixed(exp_fixed<kRefBits>(v),  kRefBits);
      imax enx = to_fixed(exp_fixed<kRefBits>(-v), kRefBits);
      return fixed_to_rational((ex + enx) / 2, kRefBits);
    }

    // tanh via the overflow-safe form tanh(x) = (1 − e^-2|x|)/(1 + e^-2|x|),
    // odd-extended for x < 0. With u = e^-2|x| ∈ (0, 1] at scale kRefBits, the
    // quotient `((1−u)·2^kRefBits)/(1+u)` keeps the dividend bounded.
    constexpr bnd::detail::rational tanh_endpoint(bnd::detail::rational v) noexcept
    {
      constexpr imax one = imax{1} << kRefBits;
      bnd::detail::rational av = bnd::detail::abs(v);
      imax u = to_fixed(exp_fixed<kRefBits>(
          bnd::detail::rational::mul_unchecked(av, bnd::detail::rational{-2})), kRefBits);
      imax t = ((one - u) << kRefBits) / (one + u);
      return (v < bnd::detail::rational{0}) ? fixed_to_rational(-t, kRefBits)
                                            : fixed_to_rational(t, kRefBits);
    }

    // --- log10, cbrt ------------------------------------------------------
    constexpr bnd::detail::rational log10_endpoint(bnd::detail::rational v) noexcept
    { return fixed_to_rational(fmul(ln_to_fixed<kRefBits>(v), inv_ln10_fixed<kRefBits>(), kRefBits), kRefBits); }

    // cbrt(v) = sign(v)·e^(ln|v|/3); cbrt(0) = 0.
    constexpr bnd::detail::rational cbrt_endpoint(bnd::detail::rational v) noexcept
    {
      if (v == bnd::detail::rational{0}) return bnd::detail::rational{0};
      bnd::detail::rational av = bnd::detail::abs(v);
      bnd::detail::rational mag = exp_from_fixed<kRefBits>(ln_to_fixed<kRefBits>(av) / 3);
      return (v < bnd::detail::rational{0}) ? -mag : mag;
    }

    // hypot(x, y) = sqrt(x²+y²), computed as m·sqrt((x/m)²+(y/m)²) with
    // m = max(|x|,|y|) so the radicand stays in [1, 2]. Exact rational scaling;
    // reuses the grid-scaled sqrt_fixed.
    constexpr bnd::detail::rational hypot_endpoint(bnd::detail::rational x,
                                                   bnd::detail::rational y) noexcept
    {
      bnd::detail::rational ax = bnd::detail::abs(x);
      bnd::detail::rational ay = bnd::detail::abs(y);
      bnd::detail::rational m  = (ax > ay) ? ax : ay;
      if (m == bnd::detail::rational{0}) return bnd::detail::rational{0};
      // Form the radicand (x/m)²+(y/m)² ∈ [1, 2] at scale kRefBits — keeping it
      // a rational overflows imax (the squared numerators cross-multiply to
      // ~1e24). Each ratio is ≤ 1, so its fixed-point square fits comfortably.
      imax rx = to_fixed(bnd::detail::rational::div_unchecked(x, m), kRefBits);
      imax ry = to_fixed(bnd::detail::rational::div_unchecked(y, m), kRefBits);
      imax s_w = fmul(rx, rx, kRefBits) + fmul(ry, ry, kRefBits);
      const imax root_w = sqrt_fixed<kRefBits>(s_w);
      // Exact product when it fits (checked multiply — identical to the old
      // unchecked result whenever that didn't overflow). A wide-denominator m
      // (e.g. a full-precision double) can push m·root past imax; that case
      // previously wrapped silently, so the fixed-point fallback only ever
      // replaces a wrong answer. |m| ≤ 2^20 (domain envelope) keeps
      // m·2^kRefBits comfortably in range.
      if (auto exact = m * fixed_to_rational(root_w, kRefBits))
        return *exact;
      return fixed_to_rational(fmul(to_fixed(m, kRefBits), root_w, kRefBits),
                               kRefBits);
    }

    // pow(b, e) = 2^(e·log2(b)), b > 0. The exponent is saturated into
    // exp2's [−30, 30] envelope here so compile-time output deduction never
    // UB-shifts; the runtime impl reports envelope overflow via `expected`.
    constexpr bnd::detail::rational pow_endpoint(bnd::detail::rational b,
                                                 bnd::detail::rational e) noexcept
    {
      // exponent e·log2(b) in fixed-point log2 units, saturated into exp2's
      // [−30, 30] envelope so the power-of-two denominator never UB-shifts.
      imax sc_w = fmul(to_fixed(e, kRefBits), log2_to_fixed<kRefBits>(b), kRefBits);
      constexpr imax lim = imax{30} << kRefBits;
      sc_w = (sc_w > lim) ? lim : (sc_w < -lim) ? -lim : sc_w;
      return exp2_from_fixed<kRefBits>(sc_w);
    }

    // --- deduction aliases ------------------------------------------------
    // Monotonic-increasing functions round endpoints outward like the exp/log
    // family. acos is decreasing; cosh is even (min at 0 if the interval
    // spans it). round_nearest lands sub-notch drift onto the grid.
    template <boundable In>
    using atan_auto_t = bound<{{floor_to_notch(atan_fixed<working_bits<In>()>(Lower<In>), Notch<In>),
                                ceil_to_notch (atan_fixed<working_bits<In>()>(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using asin_auto_t = bound<{{floor_to_notch(asin_endpoint(Lower<In>), Notch<In>),
                                ceil_to_notch (asin_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using acos_auto_t = bound<{{floor_to_notch(acos_endpoint(Upper<In>), Notch<In>),
                                ceil_to_notch (acos_endpoint(Lower<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using sinh_auto_t = bound<{{floor_to_notch(sinh_endpoint(Lower<In>), Notch<In>),
                                ceil_to_notch (sinh_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    inline constexpr bnd::detail::rational cosh_auto_lo =
      (Lower<In> <= bnd::detail::rational{0} && Upper<In> >= bnd::detail::rational{0})
        ? bnd::detail::rational{1}
        : (cosh_endpoint(Lower<In>) < cosh_endpoint(Upper<In>)
             ? cosh_endpoint(Lower<In>) : cosh_endpoint(Upper<In>));

    template <boundable In>
    inline constexpr bnd::detail::rational cosh_auto_hi =
      (cosh_endpoint(Lower<In>) > cosh_endpoint(Upper<In>))
        ? cosh_endpoint(Lower<In>) : cosh_endpoint(Upper<In>);

    template <boundable In>
    using cosh_auto_t = bound<{{floor_to_notch(cosh_auto_lo<In>, Notch<In>),
                                ceil_to_notch (cosh_auto_hi<In>, Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using tanh_auto_t = bound<{{floor_to_notch(tanh_endpoint(Lower<In>), Notch<In>),
                                ceil_to_notch (tanh_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using log10_auto_t = bound<{{floor_to_notch(log10_endpoint(Lower<In>), Notch<In>),
                                 ceil_to_notch (log10_endpoint(Upper<In>), Notch<In>)},
                                Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using cbrt_auto_t = bound<{{floor_to_notch(cbrt_endpoint(Lower<In>), Notch<In>),
                                ceil_to_notch (cbrt_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    // hypot output: non-negative, Upper at the largest-magnitude corner.
    template <boundable InX, boundable InY>
    inline constexpr bnd::detail::rational hypot_auto_hi =
      hypot_endpoint(
        (bnd::detail::abs(Lower<InX>) > bnd::detail::abs(Upper<InX>)
           ? bnd::detail::abs(Lower<InX>) : bnd::detail::abs(Upper<InX>)),
        (bnd::detail::abs(Lower<InY>) > bnd::detail::abs(Upper<InY>)
           ? bnd::detail::abs(Lower<InY>) : bnd::detail::abs(Upper<InY>)));

    template <boundable InX, boundable InY>
    using hypot_auto_t = bound<{{bnd::detail::rational{0},
                                 ceil_to_notch(hypot_auto_hi<InX, InY>, Notch<InX>)},
                                Notch<InX>}, BoundPolicy<InX> | round_nearest>;

    // pow output: extrema of b^e over the input rectangle occur at corners
    // (monotone in each argument for b > 0). Min and max of the 4 corners.
    template <boundable InB, boundable InE>
    inline constexpr bnd::detail::rational pow_corner[4] = {
      pow_endpoint(Lower<InB>, Lower<InE>), pow_endpoint(Lower<InB>, Upper<InE>),
      pow_endpoint(Upper<InB>, Lower<InE>), pow_endpoint(Upper<InB>, Upper<InE>),
    };

    template <boundable InB, boundable InE>
    inline constexpr bnd::detail::rational pow_auto_lo = []{
      bnd::detail::rational m = pow_corner<InB, InE>[0];
      for (int i = 1; i < 4; ++i)
        if (pow_corner<InB, InE>[i] < m) m = pow_corner<InB, InE>[i];
      return m;
    }();

    template <boundable InB, boundable InE>
    inline constexpr bnd::detail::rational pow_auto_hi = []{
      bnd::detail::rational m = pow_corner<InB, InE>[0];
      for (int i = 1; i < 4; ++i)
        if (pow_corner<InB, InE>[i] > m) m = pow_corner<InB, InE>[i];
      return m;
    }();

    template <boundable InB, boundable InE>
    using pow_auto_t = bound<{{floor_to_notch(pow_auto_lo<InB, InE>, Notch<InB>),
                               ceil_to_notch (pow_auto_hi<InB, InE>, Notch<InB>)},
                              Notch<InB>}, BoundPolicy<InB> | round_nearest>;
  } // namespace detail

  // --- explicit-Out impls -------------------------------------------------
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out atan_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "bnd::math::atan: input magnitudes must be \u2264 2^20 for the working-scale envelope");
    return detail::store_grid<Out>(detail::atan_fixed<detail::working_bits<Out>()>(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out asin_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -1 && Upper<In> <= 1,
                  "bnd::math::asin: input must be in [-1, 1]");
    return detail::store_grid<Out>(detail::asin_endpoint(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out acos_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -1 && Upper<In> <= 1,
                  "bnd::math::acos: input must be in [-1, 1]");
    return detail::store_grid<Out>(detail::acos_endpoint(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out sinh_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -10 && Upper<In> <= 10,
                  "bnd::math::sinh: input must be in [-10, 10]");
    return detail::store_grid<Out>(detail::sinh_endpoint(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out cosh_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -10 && Upper<In> <= 10,
                  "bnd::math::cosh: input must be in [-10, 10]");
    static_assert(Lower<Out> <= bnd::detail::rational{1},
                  "bnd::math::cosh: Out must include 1 (cosh ≥ 1)");
    return detail::store_grid<Out>(detail::cosh_endpoint(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out tanh_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -10 && Upper<In> <= 10,
                  "bnd::math::tanh: input must be in [-10, 10]");
    return detail::store_grid<Out>(detail::tanh_endpoint(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out log10_impl(In x) noexcept
  {
    static_assert(Lower<In> > 0,
                  "bnd::math::log10: input must be strictly positive");
    return detail::store_grid<Out>(detail::log10_endpoint(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out cbrt_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "bnd::math::cbrt: input magnitude must be ≤ 2^20 for the working-scale envelope");
    return detail::store_grid<Out>(detail::cbrt_endpoint(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable InX, boundable InY>
  [[nodiscard]] constexpr Out hypot_impl(InX x, InY y) noexcept
  {
    static_assert(Lower<InX> >= -(imax{1} << 20) && Upper<InX> <= (imax{1} << 20)
               && Lower<InY> >= -(imax{1} << 20) && Upper<InY> <= (imax{1} << 20),
                  "bnd::math::hypot: input magnitudes must be ≤ 2^20 for the working-scale envelope");
    static_assert(Lower<Out> <= 0, "bnd::math::hypot: Out must include 0");
    return detail::store_grid<Out>(detail::hypot_endpoint(bnd::detail::rational{x}, bnd::detail::rational{y}));
  }

  // pow: b^e for runtime base b > 0. Returns `expected` — `overflow` when
  // e·log2(b) leaves exp2's [-30, 30] envelope or the result leaves Out's
  // interval. The auto form requires Lower<InB> > 0 (so b > 0 is guaranteed
  // and the output range is bounded for deduction).
  template <boundable Out, boundable InB, boundable InE>
  [[nodiscard]] constexpr slim::expected<Out, errc> pow_impl(InB base, InE exp) noexcept
  {
    bnd::detail::rational bv = base;
    if (bv <= bnd::detail::rational{0})
      return slim::unexpected(errc::domain_error);

    constexpr int W = detail::working_bits<Out>();
    imax sc_w = detail::fmul(detail::to_fixed(bnd::detail::rational{exp}, W),
                             detail::log2_to_fixed<W>(bv), W);     // e·log2(b), scale 2^W
    constexpr imax lim = imax{30} << W;
    if (sc_w > lim || sc_w < -lim)
      return slim::unexpected(errc::overflow);

    bnd::detail::rational r = detail::exp2_from_fixed<W>(sc_w);
    if (r < Lower<Out> || r > Upper<Out>)
      return slim::unexpected(errc::overflow);
    return detail::store_grid<Out>(r);
  }

  // --- public auto-deducing forms ----------------------------------------
  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto atan(In x) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return atan_impl<detail::atan_auto_t<In>>(x);
#else
    return dbl::atan_core<detail::atan_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto asin(In x) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return asin_impl<detail::asin_auto_t<In>>(x);
#else
    return dbl::asin_core<detail::asin_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto acos(In x) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return acos_impl<detail::acos_auto_t<In>>(x);
#else
    return dbl::acos_core<detail::acos_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto sinh(In x) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return sinh_impl<detail::sinh_auto_t<In>>(x);
#else
    return dbl::sinh_core<detail::sinh_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto cosh(In x) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return cosh_impl<detail::cosh_auto_t<In>>(x);
#else
    return dbl::cosh_core<detail::cosh_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto tanh(In x) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return tanh_impl<detail::tanh_auto_t<In>>(x);
#else
    return dbl::tanh_core<detail::tanh_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto log10(In x) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return log10_impl<detail::log10_auto_t<In>>(x);
#else
    return dbl::log10_core<detail::log10_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto cbrt(In x) noexcept
  {
    static_assert(detail::require_real<In>());
#ifdef BND_MATH_FIXED
    return cbrt_impl<detail::cbrt_auto_t<In>>(x);
#else
    return dbl::cbrt_core<detail::cbrt_auto_t<In>>(x);
#endif
  }

  template <boundable InX, boundable InY>
  [[nodiscard]] BND_MATH_FN auto hypot(InX x, InY y) noexcept
  {
    static_assert(detail::require_real<InX>() && detail::require_real<InY>());
#ifdef BND_MATH_FIXED
    return hypot_impl<detail::hypot_auto_t<InX, InY>>(x, y);
#else
    return dbl::hypot_core<detail::hypot_auto_t<InX, InY>>(x, y);
#endif
  }

  template <boundable InB, boundable InE>
    requires (Lower<InB> > bnd::detail::rational{0})
  [[nodiscard]] BND_MATH_FN auto pow(InB base, InE exp) noexcept
  {
    static_assert(detail::require_real<InB>() && detail::require_real<InE>());
    using Out = detail::pow_auto_t<InB, InE>;
#ifdef BND_MATH_FIXED
    return pow_impl<Out>(base, exp);
#else
    double b = base;
    if (b <= 0.0)
      return slim::expected<Out, errc>{slim::unexpected(errc::domain_error)};
    double r = dbl::detail::d_pow(b, exp);
    if (r < static_cast<double>(Lower<Out>) || r > static_cast<double>(Upper<Out>))
      return slim::expected<Out, errc>{slim::unexpected(errc::overflow)};
    return slim::expected<Out, errc>{Out{r}};
#endif
  }
}

#endif
