//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDgridHPP
#define BNDgridHPP

#include "bound/lift.hpp"
#include "bound/detail/rational.hpp"
#include "bound/interval.hpp"
#include "bound/policy_flag.hpp"

#include "slim/expected.hpp"     // slim::expected, slim::unexpected

#include <algorithm>
#include <concepts>              // std::convertible_to (grid corner ctors)

namespace bnd { struct grid; }

namespace slim
{
  template<>
  struct sentinel_traits<bnd::grid>
  {
    protected:
      static constexpr bnd::grid sentinel() noexcept;
      static constexpr bool is_sentinel(const bnd::grid& v) noexcept;
  };
} // namespace slim

namespace bnd
{
  //---------------------------------------------------------------------------
  // grid — structural NTTP type (public members only). Discretizes its interval
  // into notch-sized steps (interval must divide evenly by notch; Notch == 0
  // allows every rational, raw not offset). Its operator+/-/*// is the engine of
  // compile-time result-grid inference: every bound arithmetic operator computes
  // its result grid here, so the result interval contains every reachable value.
  //---------------------------------------------------------------------------
  struct grid
  {
    interval Interval;
    detail::rational Notch;

    grid() = default;
    // Corner ctors accept any type convertible to `rational` — int/float/rational and
    // any `bound` / `just<>` (via its implicit `operator rational()`), so a bound can be
    // a grid corner. They stay *templates* (deducing the corner type) on purpose: a
    // braced `{lo, hi}` can't deduce to a template parameter, so the `grid{{lo,hi}, notch}`
    // spelling unambiguously picks `grid(interval, rational)` below. The conversion is
    // resolved at the call site, so grid.hpp needs no dependency on `bound`.
    constexpr grid(std::convertible_to<detail::rational> auto lower,
                   std::convertible_to<detail::rational> auto upper,
                   std::convertible_to<detail::rational> auto notch)
      :grid{interval{lower, upper}, notch} { }
    constexpr grid(std::convertible_to<detail::rational> auto lower,
                   std::convertible_to<detail::rational> auto upper)
      :grid{interval{lower, upper}, detail::rational{1}} { }
    constexpr grid(std::convertible_to<detail::rational> auto lower)
      :grid{interval{lower, lower}, detail::rational{0}} { }
    constexpr grid(interval val, detail::rational notch):Interval{val}, Notch{notch} { }

    template <auto G>
    static constexpr bool validate()
    {
      interval::validate<G.Interval>();
      static_assert(G.Interval.divides_evenly(G.Notch));
      // Lower must sit on the notch lattice. divides_evenly avoids forming the
      // (possibly umax-overflowing) Lower/Notch quotient, so a grid finer than
      // uint64 index space is still valid (it stores as rational).
      static_assert(G.Notch == 0 || detail::divides_evenly(G.Interval.Lower, G.Notch));

      return true;
    }

    // Runtime sibling of validate<G>(): same invariants, but returns a typed
    // error instead of failing a static_assert — for grids built from runtime
    // config. A value, so it can't be a bound<G,P> template argument.
    [[nodiscard]] static constexpr slim::expected<grid, errc>
    try_make(interval iv, detail::rational notch)
    {
      if (iv.Lower > iv.Upper)
        return slim::unexpected{errc::domain_error};
      if (!iv.divides_evenly(notch))
        return slim::unexpected{errc::rounding_error};
      if (notch != 0 && !detail::divides_evenly(iv.Lower, notch))
        return slim::unexpected{errc::rounding_error};
      return grid{iv, notch};
    }

    // Notch-slot count = (Upper-Lower)/Notch, computed WITHOUT the rational
    // division (which throws at constant-eval on overflow). A valid grid is
    // notch-aligned, so with span = p/q and Notch = r/s the count is exactly
    // (p/r)·(s/q); mul_overflow flags when it exceeds umax. Returns false (and
    // count is meaningless) on overflow — such a grid stores as rational, never
    // an index, so the count is never used.
    constexpr bool notch_count(umax& out) const
    {
      if (Notch == 0) { out = 0; return true; }
      const detail::rational span = (Interval.Upper - Interval.Lower).value();
      const umax p = span.Numerator,  q = detail::abs_den(span.Denominator);
      const umax r = Notch.Numerator, s = detail::abs_den(Notch.Denominator);
      if (r == 0 || p % r != 0 || s % q != 0) { out = 0; return false; }
      return !mul_overflow(p / r, s / q, &out);
    }

    // Index-storage slot count (0 on overflow; the over-flow branch of storage_min
    // is discarded for such grids, which pick rational storage instead).
    constexpr umax max_notch() const { umax c = 0; (void)notch_count(c); return c; }

    // True when the slot count fits umax (index storage is possible). False ⇒ the
    // grid is still valid but stores its value as a rational, never an index.
    constexpr bool notch_count_representable() const { umax c = 0; return notch_count(c); }

    // True when `v` is an *exact* slot: in the interval AND on a notch (notch-0
    // grids store verbatim, so any in-range value qualifies). Used to admit a
    // single representable value (e.g. `0_b`) regardless of whole-range mapping.
    constexpr bool representable(detail::rational v) const
    {
      if (!includes(Interval, v)) return false;
      if (Notch == 0) return true;
      auto diff = v - Interval.Lower;            // optional<rational>
      if (!diff) return false;
      auto off = diff.value() / Notch;           // optional<rational>
      return off.has_value() && detail::abs_den(off->Denominator) == 1;
    }

    // operator== be default for structural type
    constexpr bool operator==(const grid& rhs) const = default;
    constexpr grid operator-() const { return {-Interval, Notch}; }

    // (Raw → double decoding lives in `detail::as_double` (generic.hpp): the
    // decode depends on the storage KIND, not the raw type's signedness — a
    // `direct`-policy bound has an unsigned raw that IS the value.)

    // Snap a double to the nearest grid point `Lower + k·Notch`. `real` storage
    // is only selected for dyadic grids, so the snap is lossless. A continuous
    // grid (Notch == 0) passes `v` through. The round stays constexpr/<cmath>-free.
    constexpr double snap_double(double v) const
    {
      if (Notch == detail::rational{0}) return v;
      const double lo = static_cast<double>(Interval.Lower);
      const double nd = static_cast<double>(Notch);
      const double q  = (v - lo) / nd;
      // Round q to the nearest integer, half away from zero (matching the
      // integer engine's round_nearest). Narrow to imax only when provably safe;
      // for |q| >= 2^52 the double is already integral, so snap is a no-op.
      // This avoids the `floor(q+0.5)` double-rounding flaw and the unguarded
      // double->imax cast (UB for huge q).
      double r;
      const double aq = q < 0 ? -q : q;
      if (aq >= 4503599627370496.0)            // 2^52
        r = q;
      else
      {
        const imax   t    = static_cast<imax>(q);   // trunc toward zero; |q| < 2^52 < imax
        const double frac = q - static_cast<double>(t);
        if      (frac >=  0.5) r = static_cast<double>(t + 1);
        else if (frac <= -0.5) r = static_cast<double>(t - 1);
        else                   r = static_cast<double>(t);
      }
      return lo + r * nd;
    }

    static constexpr grid make_sentinel() noexcept
    { return grid{interval{detail::rational{0}, detail::rational{0}}, detail::rational::make_sentinel()}; }
  };

  // Smallest raw type holding every reachable index in G. Order: notch-zero →
  // rational (no integer index space); index count too large for any integer →
  // rational (store the value's fraction directly, no index); signed-direct fits
  // Lower < 0 with notch 1; unsigned-offset (max_notch slots) otherwise.
  namespace detail
  {
  template <grid G>
  using storage_min =
    std::conditional_t<(G.Notch == 0), detail::rational,
    std::conditional_t<(!G.notch_count_representable()), detail::rational,
    std::conditional_t<(G.Interval.Lower < 0 && G.Notch == 1),
      smallest_int_for<trunc(G.Interval.Lower), trunc(G.Interval.Upper)>,
      smallest_uint_for<G.max_notch()>>>>;

  // Dyadic grid: power-of-2 notch denominator and Lower denominator, so every
  // on-grid value is exactly representable in IEEE-754 `double`. Precondition
  // for double-backed (`real`) storage.
  constexpr bool is_pow2(umax n) { return n != 0 && (n & (n - 1)) == 0; }

  template <grid G>
  inline constexpr bool dyadic_grid =
       G.Notch.Numerator != 0
    && is_pow2(bnd::detail::abs_den(G.Notch.Denominator))
    && is_pow2(bnd::detail::abs_den(G.Interval.Lower.Denominator));

  // log2 of a power-of-two magnitude (>= 1); 0 for 1. (grid.hpp can't include
  // cmath.hpp — that depends on us — so this mirrors detail::log2_pow2.)
  constexpr int log2_pow2_mag(umax d) noexcept { int n = 0; while (d > 1) { d >>= 1; ++n; } return n; }

  // |r · 2^f| as an integer. On a dyadic grid every endpoint's denominator is a
  // power of two dividing 2^f, so r·2^f is integral. Writes |N| and returns true
  // when it fits in umax; returns false on overflow (which already means ≥ 2^53).
  constexpr bool scaled_numerator(const rational& r, int f, umax& out) noexcept
  {
    if (r.Numerator == 0) { out = 0; return true; }
    const int sh = f - log2_pow2_mag(abs_den(r.Denominator));   // 0 <= sh <= f
    if (sh >= 64) return false;
    if (r.Numerator > (~umax{0} >> sh)) return false;           // Numerator << sh overflows
    out = r.Numerator << sh;
    return true;
  }

  // `double`-exactness of a dyadic grid: the IEEE-754 double path equals the
  // exact grid arithmetic iff, at the coarsest-magnitude end, the value's ULP is
  // no coarser than the notch. Writing v = N·2^(−f) with f = log2(den(Notch)),
  // that is |N| < 2^53 (53-bit significand) AND f ≤ 1022 (notch ≥ smallest
  // normal, so no on-grid value is subnormal). The 2^1024 overflow ceiling is
  // unreachable once |N| < 2^53. Necessary precondition for `real` storage.
  template <grid G>
  constexpr bool compute_double_exact() noexcept
  {
    if constexpr (!dyadic_grid<G>) return false;
    else
    {
      constexpr int f = log2_pow2_mag(abs_den(G.Notch.Denominator));
      if (f > 1022) return false;
      umax nlo = 0, nhi = 0;
      if (!scaled_numerator(G.Interval.Lower, f, nlo)) return false;
      if (!scaled_numerator(G.Interval.Upper, f, nhi)) return false;
      constexpr umax lim = umax{1} << 53;
      return nlo < lim && nhi < lim;
    }
  }

  template <grid G>
  inline constexpr bool double_exact = compute_double_exact<G>();

  // `float`-exactness: the binary32 analogue of double_exact. Every on-grid value
  // v = N·2^(−f) must fit float's 24-bit significand (|N| < 2^24) with f ≤ 126
  // (notch ≥ float's smallest normal, so no on-grid value is subnormal).
  // Necessary precondition for `f32` (binary32-backed) storage.
  template <grid G>
  constexpr bool compute_float_exact() noexcept
  {
    if constexpr (!dyadic_grid<G>) return false;
    else
    {
      constexpr int f = log2_pow2_mag(abs_den(G.Notch.Denominator));
      if (f > 126) return false;
      umax nlo = 0, nhi = 0;
      if (!scaled_numerator(G.Interval.Lower, f, nlo)) return false;
      if (!scaled_numerator(G.Interval.Upper, f, nhi)) return false;
      constexpr umax lim = umax{1} << 24;
      return nlo < lim && nhi < lim;
    }
  }

  template <grid G>
  inline constexpr bool float_exact = compute_float_exact<G>();

  // Fixed-width raw storage (policy_flag.hpp i8..u64) — pin the exact backing
  // type instead of letting storage_min pick the smallest fit.
  //
  // has_width_flag / width_flag_count: detect "a width is pinned" and enforce
  // exactly one (combining two width flags is a misuse, caught in storage_pick).
  constexpr bool has_width_flag(policy_flag P) noexcept
  { return (P & bnd::raw_width_mask) != bnd::none; }

  constexpr int width_flag_count(policy_flag P) noexcept
  {
    policy_flag w = P & bnd::raw_width_mask;
    int n = 0;
    for (; w; w >>= 1) n += static_cast<int>(w & 1);
    return n;
  }

  // Map the single set width bit to its C++ type (only valid when has_width_flag).
  template <policy_flag P>
  using raw_type_of =
    std::conditional_t<(P & bnd::i8 ) == bnd::i8 , std::int8_t,
    std::conditional_t<(P & bnd::u8 ) == bnd::u8 , std::uint8_t,
    std::conditional_t<(P & bnd::i16) == bnd::i16, std::int16_t,
    std::conditional_t<(P & bnd::u16) == bnd::u16, std::uint16_t,
    std::conditional_t<(P & bnd::i32) == bnd::i32, std::int32_t,
    std::conditional_t<(P & bnd::u32) == bnd::u32, std::uint32_t,
    std::conditional_t<(P & bnd::i64) == bnd::i64, std::int64_t,
                                                    std::uint64_t>>>>>>>;

  // Does raw type R hold every reachable raw value of grid G under the given
  // encoding? Index storage runs 0..max_notch (unsigned); value storage runs
  // Lower..Upper. The strict margins (max-1 unsigned / min+1 signed) match the
  // sentinel-slot reservation in smallest_uint_for / smallest_int_for.
  template <grid G, typename R, bool Index>
  constexpr bool storage_fits() noexcept
  {
    using lim = std::numeric_limits<R>;
    if constexpr (Index)
      return G.notch_count_representable()
          && G.max_notch() < static_cast<umax>(lim::max());
    else if constexpr (std::is_unsigned_v<R>)
      return G.Interval.Lower >= 0
          && G.Interval.Upper <= rational{static_cast<umax>(lim::max()) - 1};
    else
      return G.Interval.Lower >= rational{static_cast<imax>(lim::min()) + 1}
          && G.Interval.Upper <= rational{static_cast<imax>(lim::max())};
  }

  // Demote an fp STORAGE flag a result grid can't represent — for DEDUCED policies
  // (cmath auto-outputs, which inherit the operand's storage flag), so a deduced
  // f32 output whose grid overflows binary32 silently widens instead of hard-
  // erroring. (A grid a user spells `f32` on directly still static_asserts in
  // storage_pick — that's deliberate misuse, not deduction.) f32 needs float_exact,
  // f64 needs double_exact (Notch == 0 continuous fits either). When the flag
  // doesn't fit: widen f32→f64 if double holds the grid, else drop the fp flag so
  // storage is deduced. The snap/round bits are preserved.
  // Storage for a bound<G, P>: representation flags pick the raw type, widest-wins
  // (exact > real > direct > indexed > deduced).
  //   exact   → rational raw on any grid.
  //   real    → double-backed under the default engine, on a dyadic or notch-0
  //             grid; elided under BND_MATH_FIXED (falls through to deduced).
  //   direct  → raw == value, plain integer (Notch == 1).
  //   indexed → raw == 0-based notch index (Notch != 0).
  //   none    → storage_min deduction.
  template <grid G, policy_flag P>
  constexpr auto storage_pick()
  {
    if constexpr ((P & bnd::exact) == bnd::exact)
      return detail::rational{};
#ifndef BND_MATH_FIXED
    else if constexpr (((P & bnd::real) == bnd::real)
                    && (double_exact<G> || G.Notch == 0))
      return double{};
    else if constexpr (((P & bnd::real) == bnd::real) && dyadic_grid<G>)
    {
      // `real`/`f64` explicitly requested on a dyadic grid double can't represent
      // exactly (max |value·2^f| ≥ 2^53, or notch below the smallest normal).
      // Arithmetic drops the flag before reaching here, so this is direct misuse.
      static_assert(double_exact<G>,
        "f64 storage: grid exceeds double's 53-bit significand — coarsen the "
        "notch/range or use `exact`");
      return double{};   // unreachable; fixes the deduced return type
    }
    else if constexpr (((P & bnd::f32) == bnd::f32)
                    && (float_exact<G> || G.Notch == 0))
      return float{};
    else if constexpr (((P & bnd::f32) == bnd::f32) && double_exact<G>)
      // `f32` requested on a grid too fine for float but representable in double:
      // WIDEN the storage to binary64. This makes a deduced f32 output (a cmath
      // result inheriting the operand's flag) whose grid overflows float store its
      // value in double rather than hard-erroring — the value stays exact. The f32
      // POLICY bit remains (harmless; storage is raw-driven via fp_raw).
      return double{};
    else if constexpr (((P & bnd::f32) == bnd::f32) && dyadic_grid<G>)
    {
      // Too fine for double too → genuinely unrepresentable as fp storage.
      static_assert(double_exact<G>,
        "f32 storage: grid exceeds double's 53-bit significand — coarsen the "
        "notch/range or use `exact`");
      return float{};    // unreachable; fixes the deduced return type
    }
#endif
    else if constexpr (has_width_flag(P))
    {
      // User-pinned raw width (i8..u64). Encoding follows `indexed` (0-based
      // notch index) else value storage (raw == value, Notch == 1 like `direct`).
      // No silent widening — a type too small for the grid is a hard error.
      static_assert(width_flag_count(P) == 1,
        "storage: pick a single fixed-width flag (e.g. `u16`), not several");
      using R = raw_type_of<P>;
      constexpr bool idx = (P & bnd::indexed) == bnd::indexed;
      static_assert(idx ? (G.Notch != 0) : (G.Notch == 1),
        "fixed-width storage: value storage needs Notch == 1 — add `indexed` to "
        "store a notched grid's 0-based index instead");
      static_assert(storage_fits<G, R, idx>(),
        "fixed-width storage: the chosen raw type is too small for this grid — "
        "widen the flag, coarsen the grid/notch, or use `exact`");
      return R{};
    }
    else if constexpr ((P & bnd::direct) == bnd::direct && G.Notch == 1)
      return std::conditional_t<(G.Interval.Lower < 0),
          smallest_int_for<trunc(G.Interval.Lower), trunc(G.Interval.Upper)>,
          smallest_uint_for<static_cast<umax>(trunc(G.Interval.Upper))>>{};
    else if constexpr ((P & bnd::indexed) == bnd::indexed && G.Notch != 0)
      return smallest_uint_for<G.max_notch()>{};
    else
      return storage_min<G>{};
  }

  template <grid G, policy_flag P>
  using storage_for = decltype(storage_pick<G, P>());
  }

  constexpr slim::optional<grid> operator+(const grid&, const grid&);
  constexpr slim::optional<grid> operator-(const grid&, const grid&);
  constexpr slim::optional<grid> operator*(const grid&, const grid&);
  constexpr slim::optional<grid> operator/(const grid&, const grid&);

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<grid> operator+(const grid& lhs, const grid& rhs)
  {
    // gcd returns optional — lift it so a notch-denominator overflow produces
    // nullopt rather than a silently wrapped result grid.
    return lift(
      [](interval i, detail::rational n){ return grid{i, n}; },
      lhs.Interval + rhs.Interval, detail::gcd(lhs.Notch, rhs.Notch));
  }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<grid> operator-(const grid& lhs, const grid& rhs)
  {
    return operator+(lhs, -rhs);
  }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<grid> operator*(const grid& lhs, const grid& rhs)
  {
    return lift(
      [](interval i, detail::rational n){ return grid{i, n}; },
      lhs.Interval * rhs.Interval, lhs.Notch * rhs.Notch);
  }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<grid> operator/(const grid& lhs, const grid& rhs)
  {
    auto d = lhs.Interval / rhs.Interval;
    if (d.has_value())
      return grid{*d, detail::rational{0}};

    // Divisor interval includes zero — exclude zero for result interval.
    if (rhs.Interval.Lower == 0 && rhs.Interval.Upper == 0)
      return slim::nullopt;

    // `step` = smallest non-zero divisor magnitude; splits the divisor interval
    // into positive [step, Upper] and negative [Lower, -step] (skipping zero).
    // Both sides present → the result is their union.
    detail::rational step = (rhs.Notch != 0) ? detail::abs(rhs.Notch) : detail::rational{1};
    bool has_pos = 0 < rhs.Interval.Upper;
    bool has_neg = 0 > rhs.Interval.Lower;

    if (has_pos && has_neg)
    {
      return lift(
        [](interval pos, interval neg){
          return grid{interval{std::min(neg.Lower, pos.Lower),
                               std::max(neg.Upper, pos.Upper)}, detail::rational{0}};
        },
        lhs.Interval / interval{step, rhs.Interval.Upper},
        lhs.Interval / interval{rhs.Interval.Lower, -step});
    }
    else if (has_pos)
    {
      return lift([](interval i){ return grid{i, detail::rational{0}}; },
                  lhs.Interval / interval{step, rhs.Interval.Upper});
    }
    else
    {
      return lift([](interval i){ return grid{i, detail::rational{0}}; },
                  lhs.Interval / interval{rhs.Interval.Lower, -step});
    }
  }
} // namespace bnd

namespace slim
{
  constexpr bnd::grid sentinel_traits<bnd::grid>::sentinel() noexcept
  { return bnd::grid::make_sentinel(); }

  constexpr bool sentinel_traits<bnd::grid>::is_sentinel(const bnd::grid& v) noexcept
  { return v.Notch.Denominator == 0; }
} // namespace slim

//---------------------------------------------------------------------------
// Structured bindings: `auto [iv, notch] = some_grid;`
//---------------------------------------------------------------------------
template <> struct std::tuple_size<bnd::grid> : std::integral_constant<std::size_t, 2> {};
template <> struct std::tuple_element<0, bnd::grid> { using type = bnd::interval; };
template <> struct std::tuple_element<1, bnd::grid> { using type = bnd::detail::rational; };

namespace bnd
{
  template <std::size_t I, class G>
    requires std::same_as<std::remove_cvref_t<G>, bnd::grid>
  constexpr auto&& get(G&& g) noexcept
  {
    if constexpr (I == 0) return std::forward<G>(g).Interval;
    else                  return std::forward<G>(g).Notch;
  }
}

#endif // BNDgridHPP
