// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_GRID_HPP
#define BEMAN_INSIDE_GRID_HPP

#include <beman/inside/lift.hpp>
#include <beman/inside/detail/rational.hpp>
#include <beman/inside/interval.hpp>
#include <beman/inside/detail/grid_rational.hpp>
#include <beman/inside/policy_flag.hpp>
#include <beman/inside/detail/int_for_bits.hpp>

#include <expected>   // std::expected, std::unexpected

#include <bit>
#include <concepts>              // std::convertible_to (grid corner ctors)

namespace beman::inside::detail
{
  // Wide enough for every exact grid computation on 64-bit grid numbers: a
  // product of three 64-bit magnitudes plus a sign.
  using grid_wide = wide_sint<4>;

  // A grid number's signed numerator and positive denominator, widened.
  constexpr grid_wide wide_numerator(rational const& r) noexcept
  {
    const grid_wide n{r.Numerator};
    return r.Denominator < 0 ? -n : n;
  }
  constexpr grid_wide wide_denominator(rational const& r) noexcept
  { return grid_wide{abs_den(r.Denominator)}; }
}

namespace beman::inside
{
  //---------------------------------------------------------------------------
  // grid — structural NTTP type (public members only). Discretizes its interval
  // into notch-sized steps (interval must divide evenly by notch; Notch == 0
  // allows every rational, raw not offset). Its operator+/-/*// is the engine of
  // compile-time result-grid inference: every inside arithmetic operator computes
  // its result grid here, so the result interval contains every reachable value.
  //---------------------------------------------------------------------------
  inline namespace BEMAN_INSIDE_GRID_ABI {
  struct grid
  {
    interval Interval;
    detail::rational Notch;

    grid() = default;
    // Corner ctors accept any type convertible to `rational` — int/float/rational and
    // any `inside` / `just<>` (via its implicit `operator rational()`), so an inside can be
    // a grid corner. They stay *templates* (deducing the corner type) on purpose: a
    // braced `{lo, hi}` can't deduce to a template parameter, so the `grid{{lo,hi}, notch}`
    // spelling unambiguously picks `grid(interval, rational)` below. The conversion is
    // resolved at the call site, so grid.hpp needs no dependency on `inside`.
    constexpr grid(std::convertible_to<detail::rational> auto lower,
                   std::convertible_to<detail::rational> auto upper,
                   std::convertible_to<detail::rational> auto notch)
      :grid{interval{lower, upper}, notch} { }
    // Two limits: the notch is derived — gcd(1, Lower, Upper), the coarsest
    // step 1/k that keeps every integer and both limits on the lattice. Integer
    // limits give 1; {0.5, 10} gives 1/2; {frac<-6,5>, frac<3,5>} gives 1/5.
    constexpr grid(std::convertible_to<detail::rational> auto lower,
                   std::convertible_to<detail::rational> auto upper)
      :grid{interval{lower, upper}, derive_notch(lower, upper)} { }
    constexpr grid(std::convertible_to<detail::rational> auto lower)
      :grid{interval{lower, lower}, detail::rational{0}} { }
    constexpr grid(interval val, detail::rational notch):Interval{val}, Notch{notch} { }

  private:
    // A combined denominator past imax has no rational notch: fall back to a
    // continuous grid (notch 0), which is always valid.
    static constexpr detail::rational derive_notch(auto lower, auto upper)
    {
      check_short_binary(lower);
      check_short_binary(upper);
      const detail::rational lo{lower}, hi{upper};
      return detail::gcd(detail::rational{1}, lo)
          .and_then([&](detail::rational g) { return detail::gcd(g, hi); })
          .value_or(detail::rational{0});
    }

    // A floating-point limit is taken as its exact binary value, so 0.1 would
    // derive a 2^-55 notch. Past 1/1024 the literal almost surely meant a
    // decimal: reject it at compile time and point to the exact spellings.
    template <typename T>
    static constexpr void check_short_binary([[maybe_unused]] T v)
    {
      if constexpr (std::floating_point<T>)
        if (std::is_constant_evaluated() && detail::abs_den(detail::rational{v}.Denominator) > 1024)
          detail::constexpr_error<
            // Clang prints only the first ~34 characters: lead with the fix.
            "float limit: use _r literal (0.1_r) or give a notch {{lo, hi}, per<D>}; "
            "grid{lo, hi} derives a notch from a floating-point limit only down to "
            "1/1024 (0.1 is not 1/10 in binary)">();
    }

  public:

    template <auto G>
    static constexpr bool validate()
    {
      interval::validate<G.Interval>();
      // Decoding is Lower + raw·Notch: a negative notch would count downward.
      static_assert(G.Notch >= 0, "grid: the notch must be non-negative");
      static_assert(G.Interval.divides_evenly(G.Notch));
      // Lower must sit on the notch lattice. divides_evenly avoids forming the
      // (possibly umax-overflowing) Lower/Notch quotient, so a grid finer than
      // uint64 index space is still valid (it stores as rational).
      static_assert(G.Notch == 0 || detail::divides_evenly(G.Interval.Lower, G.Notch));

      return true;
    }

    // Runtime sibling of validate<G>(): same invariants, but returns a typed
    // error instead of failing a static_assert — for grids built from runtime
    // config. A value, so it can't be an inside<G,P> template argument.
    [[nodiscard]] static constexpr std::expected<grid, errc>
    try_make(interval iv, detail::rational notch)
    {
      if (iv.Lower > iv.Upper)
        return std::unexpected{errc::domain_error};
      if (notch < 0)
        return std::unexpected{errc::domain_error};
      if (!iv.divides_evenly(notch))
        return std::unexpected{errc::rounding_error};
      if (notch != 0 && !detail::divides_evenly(iv.Lower, notch))
        return std::unexpected{errc::rounding_error};
      return grid{iv, notch};
    }

    // Exact slot count (Upper − Lower)/Notch, however large: with Upper = a/b,
    // Lower = c/d and Notch = e/f it is (a·d − c·b)·f / (b·d·e), exact on a
    // valid grid. 0 for a continuous grid.
    [[nodiscard]] constexpr detail::grid_wide slot_count() const noexcept
    {
      using detail::wide_numerator, detail::wide_denominator;
      if (Notch == 0) return detail::grid_wide{0};
      const auto& U = Interval.Upper;
      const auto& L = Interval.Lower;
      const detail::grid_wide num =
          (wide_numerator(U) * wide_denominator(L) - wide_numerator(L) * wide_denominator(U))
        * wide_denominator(Notch);
      return num / (wide_denominator(U) * wide_denominator(L) * wide_numerator(Notch));
    }

    // Bits needed to hold every slot index 0..slot_count().
    [[nodiscard]] constexpr int slot_bits() const noexcept { return bit_width_of(slot_count()); }

    // The slot count as a umax; false (out = 0) when it needs more than 64
    // bits — such a grid stores a wide_int index.
    [[nodiscard]] constexpr bool max_index_checked(umax& out) const
    {
      const detail::grid_wide c = slot_count();
      const bool fits = !(detail::grid_wide{std::numeric_limits<umax>::max()} < c);
      out = fits ? static_cast<umax>(c) : umax{0};
      return fits;
    }

    // Index-storage slot count (0 on overflow; the over-flow branch of storage_min
    // is discarded for such grids, which pick rational storage instead).
    [[nodiscard]] constexpr umax max_index() const { umax c = 0; (void)max_index_checked(c); return c; }

    // True when the slot count fits umax (index storage is possible). False ⇒ the
    // grid is still valid but stores its value as a rational, never an index.
    [[nodiscard]] constexpr bool max_index_representable() const { umax c = 0; return max_index_checked(c); }

    // True when `v` is an *exact* slot: in the interval AND on a notch (notch-0
    // grids store verbatim, so any in-range value qualifies). Used to admit a
    // single representable value (e.g. `0_ins`) regardless of whole-range mapping.
    [[nodiscard]] constexpr bool representable(detail::rational v) const noexcept
    {
      if (!includes(Interval, v)) return false;
      if (Notch == 0) return true;
      auto diff = v - Interval.Lower;            // expected<rational, errc>
      if (!diff) return false;
      auto off = diff.value() / Notch;           // expected<rational, errc>
      return off.has_value() && detail::abs_den(off->Denominator) == 1;
    }

    // operator== be default for structural type
    [[nodiscard]] constexpr bool operator==(const grid& rhs) const = default;
    [[nodiscard]] constexpr grid operator-() const { return {-Interval, Notch}; }

    // (Raw → double decoding lives in `detail::as_double` (generic.hpp): the
    // decode depends on the storage KIND, not the raw type's signedness — a
    // `direct`-policy inside has an unsigned raw that IS the value.)
  };
  }

  namespace detail
  {
  // Snap a double onto the (dyadic) grid G by rounding mode M — the same rule
  // as integer storage (rounding_of; ties of `nearest` half away from zero). On
  // an fp grid the notch is a power of two, so v/notch is the exact signed value
  // index. A continuous grid (notch 0) has nothing to snap to. |index| >= 2^52 is
  // already integral, so the imax narrowing below is always safe. G and M are
  // template parameters so each store compiles to its own branch-free rounding.
  // AnySign: v may lie below a grid that starts at 0 or higher (the wrap path
  // rounds out-of-range values); otherwise that half of the tie test is dead.
  template <grid G, round_mode M = round_mode::nearest, bool AnySign = (G.Interval.Lower < 0)>
  [[nodiscard]] constexpr double snap_double(double v) noexcept
  {
    if constexpr (G.Notch == rational{0})
      return v;
    else
    {
      constexpr double nd = static_cast<double>(G.Notch);
      const double q = v / nd;
      if ((q < 0 ? -q : q) >= 4503599627370496.0)        // 2^52
        return v;
      const imax   t = static_cast<imax>(q);              // toward zero
      const double f = q - static_cast<double>(t);        // exact, sign of q, |f| < 1
      imax k = t;
      if constexpr (M == round_mode::nearest)
      {
        k += (f >= 0.5);
        if constexpr (AnySign) k -= (f <= -0.5);
      }
      else if constexpr (M == round_mode::floor)     k -= (f < 0);
      else if constexpr (M == round_mode::ceil)      k += (f > 0);
      else if constexpr (M == round_mode::half_even) k += (f > 0.5  || (f ==  0.5 && (t & 1)))
                                                       - (f < -0.5 || (f == -0.5 && (t & 1)));
      return static_cast<double>(k) * nd;
    }
  }
  }

  // Raw of a point grid (Lower == Upper): its value lives in the type, so the
  // raw is empty. It acts as index slot 0 — constructible from any index,
  // converting to integer 0 — so the index-storage decode (Lower + raw·Notch)
  // yields the point's value without special cases. Declared
  // [[no_unique_address]] in inside, a point member of another struct (also
  // marked [[no_unique_address]]) takes no space.
  namespace detail
  {
  struct point_slot
  {
    constexpr point_slot() = default;
    template <typename T> requires std::is_arithmetic_v<T>
    constexpr point_slot(T) noexcept {}                     // any index: the only slot
    constexpr point_slot(rational const&) noexcept {}       // any value: the type holds it
    constexpr operator imax() const noexcept { return 0; }   // reads as index 0
    constexpr bool operator==(point_slot const&) const = default;
    constexpr auto operator<=>(point_slot const&) const = default;
  };
  }

  // Both endpoints lie in imax — the signed-direct candidates (and every
  // `trunc(endpoint)` constant) are only meaningful then.
  namespace detail
  {
  constexpr bool fits_imax(interval const& iv) noexcept
  {
    return iv.Lower >= rational{std::numeric_limits<imax>::min()}
        && iv.Upper <= rational{std::numeric_limits<imax>::max()};
  }
  }

  // Smallest raw type holding every reachable index in G. Order: point →
  // empty point_slot; notch-zero → rational (no integer index space); more
  // than 2^64 slots → a wide_int index; signed-direct fits Lower < 0 with
  // notch 1; unsigned-offset (max_index slots) otherwise.
  namespace detail
  {
  // Unsigned index raw for G's slots: a builtin up to 64 bits, else wide.
  template <grid G>
  using index_raw_for_t =
    std::conditional_t<G.max_index_representable(), smallest_uint_for_t<G.max_index()>,
                       int_for_bits_t<G.slot_bits(), false>>;

  template <grid G>
  using storage_min_t =
    std::conditional_t<(G.Interval.Lower == G.Interval.Upper), point_slot,
    std::conditional_t<(G.Notch == 0), detail::rational,
    std::conditional_t<(!G.max_index_representable()), index_raw_for_t<G>,
    std::conditional_t<(G.Interval.Lower < 0 && G.Notch == 1 && fits_imax(G.Interval)),
      smallest_int_for_t<trunc(G.Interval.Lower), trunc(G.Interval.Upper)>,
      smallest_uint_for_t<G.max_index()>>>>>;

  // Dyadic grid: power-of-2 notch denominator and Lower denominator, so every
  // on-grid value is exactly representable in IEEE-754 `double`. Precondition
  // for double-backed (`f64`) storage.
  template <grid G>
  inline constexpr bool dyadic_grid =
       G.Notch.Numerator != 0
    && std::has_single_bit(detail::abs_den(G.Notch.Denominator))
    && std::has_single_bit(detail::abs_den(G.Interval.Lower.Denominator));

  // log2 of a power-of-two magnitude (>= 1); 0 for 1.
  constexpr int log2_pow2_mag(umax d) noexcept { return std::countr_zero(d); }

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
  // unreachable once |N| < 2^53. Necessary precondition for `f64` storage.
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
  { return (P & raw_width_mask) != none; }

  constexpr int width_flag_count(policy_flag P) noexcept
  { return std::popcount(P & raw_width_mask); }

  // Map the single set width bit to its C++ type (only valid when has_width_flag).
  template <policy_flag P>
  using raw_type_of_t =
    std::conditional_t<(P & i8 ) == i8 , std::int8_t,
    std::conditional_t<(P & u8 ) == u8 , std::uint8_t,
    std::conditional_t<(P & i16) == i16, std::int16_t,
    std::conditional_t<(P & u16) == u16, std::uint16_t,
    std::conditional_t<(P & i32) == i32, std::int32_t,
    std::conditional_t<(P & u32) == u32, std::uint32_t,
    std::conditional_t<(P & i64) == i64, std::int64_t,
                                                    std::uint64_t>>>>>>>;

  // Does raw type R hold every reachable raw value of grid G under the given
  // encoding? Index storage runs 0..max_index (unsigned); value storage runs
  // Lower..Upper. The full range of R is usable, matching smallest_uint_for /
  // smallest_int_for.
  template <grid G, typename R, bool Index>
  constexpr bool storage_fits() noexcept
  {
    using lim = std::numeric_limits<R>;
    if constexpr (Index)
      return G.max_index_representable()
          && G.max_index() <= static_cast<umax>(lim::max());
    else if constexpr (std::is_unsigned_v<R>)
      return G.Interval.Lower >= 0
          && G.Interval.Upper <= rational{static_cast<umax>(lim::max())};
    else
      return G.Interval.Lower >= rational{static_cast<imax>(lim::min())}
          && G.Interval.Upper <= rational{static_cast<imax>(lim::max())};
  }

  // Storage for an inside<G, P>: representation flags pick the raw type, widest-wins
  // (exact > f64 > f32 > {width} > direct > indexed > deduced).
  //   exact   → rational raw on any grid.
  //   f64     → double-backed under the default engine, on a dyadic or notch-0
  //             grid; elided under BEMAN_INSIDE_MATH_CORDIC (falls through to deduced).
  //   f32     → float-backed when float holds the grid, else widened to double.
  //   {width} → the pinned i8..u64 type, value or (with `indexed`) index storage.
  //   direct  → raw == value, plain integer (Notch == 1).
  //   indexed → raw == 0-based notch index (Notch != 0).
  //   none    → storage_min deduction.
  template <grid G, policy_flag P>
  constexpr auto storage_pick()
  {
    // A point's value is its type: empty raw whatever the representation flag,
    // unless a width flag pins a wire layout.
    if constexpr (G.Interval.Lower == G.Interval.Upper && !has_width_flag(P))
      return point_slot{};
    else if constexpr (has_flag(P, exact))
      return detail::rational{};
#ifndef BEMAN_INSIDE_MATH_NO_FP
    else if constexpr (has_flag(P, f64)
                    && (double_exact<G> || G.Notch == 0))
      return double{};
    else if constexpr (has_flag(P, f64) && dyadic_grid<G>)
    {
      // `f64` explicitly requested on a dyadic grid double can't represent
      // exactly (max |value·2^f| ≥ 2^53, or notch below the smallest normal).
      // Arithmetic drops the flag before reaching here, so this is direct misuse.
      static_assert(double_exact<G>,
        "f64 storage: grid exceeds double's 53-bit significand — coarsen the "
        "notch/range or use `exact`");
      return double{};   // unreachable; fixes the deduced return type
    }
    else if constexpr (has_flag(P, f32)
                    && (float_exact<G> || G.Notch == 0))
      return float{};
    else if constexpr (has_flag(P, f32) && double_exact<G>)
      // `f32` requested on a grid too fine for float but representable in double:
      // WIDEN the storage to binary64. This makes a deduced f32 output (a cmath
      // result inheriting the operand's flag) whose grid overflows float store its
      // value in double rather than hard-erroring — the value stays exact. The f32
      // POLICY bit remains (harmless; storage is raw-driven via fp_raw).
      return double{};
    else if constexpr (has_flag(P, f32) && dyadic_grid<G>)
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
      using R = raw_type_of_t<P>;
      constexpr bool idx = (P & indexed) == indexed;
      // A point (notch 0) has one value: value storage holds it, index storage
      // holds slot 0 — the notch requirement does not apply.
      static_assert(G.Interval.Lower == G.Interval.Upper
                    || (idx ? (G.Notch != 0) : (G.Notch == 1)),
        "fixed-width storage: value storage needs Notch == 1 — add `indexed` to "
        "store a notched grid's 0-based index instead");
      static_assert(storage_fits<G, R, idx>(),
        "fixed-width storage: the chosen raw type is too small for this grid — "
        "widen the flag, coarsen the grid/notch, or use `exact`");
      return R{};
    }
    else if constexpr ((P & direct) == direct && G.Notch == 1)
    {
      static_assert(G.Interval.Lower >= 0 || fits_imax(G.Interval),
        "direct storage: a negative grid must fit int64 — drop `direct` (index storage) or use `exact`");
      return std::conditional_t<(G.Interval.Lower < 0),
          smallest_int_for_t<trunc(G.Interval.Lower), trunc(G.Interval.Upper)>,
          smallest_uint_for_t<static_cast<umax>(trunc(G.Interval.Upper))>>{};
    }
    else if constexpr ((P & indexed) == indexed && G.Notch != 0)
      return index_raw_for_t<G>{};
    else
      return storage_min_t<G>{};
  }

  template <grid G, policy_flag P>
  using storage_for_t = decltype(storage_pick<G, P>());
  }

  [[nodiscard]] constexpr std::expected<grid, errc> operator+(const grid&, const grid&);
  [[nodiscard]] constexpr std::expected<grid, errc> operator-(const grid&, const grid&);
  [[nodiscard]] constexpr std::expected<grid, errc> operator*(const grid&, const grid&);
  [[nodiscard]] constexpr std::expected<grid, errc> operator/(const grid&, const grid&);

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<grid, errc> operator+(const grid& lhs, const grid& rhs)
  {
    // gcd returns expected — lift it so a notch-denominator overflow produces
    // errc::overflow rather than a silently wrapped result grid.
    return detail::lift(
      [](interval i, detail::rational n){ return grid{i, n}; },
      lhs.Interval + rhs.Interval, detail::gcd(lhs.Notch, rhs.Notch));
  }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<grid, errc> operator-(const grid& lhs, const grid& rhs)
  {
    return operator+(lhs, -rhs);
  }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<grid, errc> operator*(const grid& lhs, const grid& rhs)
  {
    // A point operand c (notch 0) scales the other lattice exactly: its notch
    // becomes N·|c|, so `x * just<c>` keeps integer storage instead of turning
    // continuous (rational-backed).
    const bool lp = lhs.Interval.Lower == lhs.Interval.Upper;
    const bool rp = rhs.Interval.Lower == rhs.Interval.Upper;
    const detail::rational ln = (lp && !rp) ? detail::abs(lhs.Interval.Lower) : lhs.Notch;
    const detail::rational rn = (rp && !lp) ? detail::abs(rhs.Interval.Lower) : rhs.Notch;
    return detail::lift(
      [](interval i, detail::rational n){ return grid{i, n}; },
      lhs.Interval * rhs.Interval, ln * rn);
  }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<grid, errc> operator/(const grid& lhs, const grid& rhs)
  {
    auto d = lhs.Interval / rhs.Interval;
    if (d.has_value())
      return grid{*d, detail::rational{0}};

    // Divisor interval includes zero — exclude zero for result interval.
    if (rhs.Interval.Lower == 0 && rhs.Interval.Upper == 0)
      return std::unexpected{errc::division_by_zero};

    // `step` = smallest non-zero divisor magnitude; splits the divisor interval
    // into positive [step, Upper] and negative [Lower, -step] (skipping zero).
    // Both sides present → the result is their union.
    detail::rational step = (rhs.Notch != 0) ? detail::abs(rhs.Notch) : detail::rational{1};
    bool has_pos = 0 < rhs.Interval.Upper;
    bool has_neg = 0 > rhs.Interval.Lower;

    if (has_pos && has_neg)
    {
      return detail::lift(
        [](interval pos, interval neg){
          return grid{interval{neg.Lower < pos.Lower ? neg.Lower : pos.Lower,
                               neg.Upper < pos.Upper ? pos.Upper : neg.Upper}, detail::rational{0}};
        },
        lhs.Interval / interval{step, rhs.Interval.Upper},
        lhs.Interval / interval{rhs.Interval.Lower, -step});
    }
    else if (has_pos)
    {
      return detail::lift([](interval i){ return grid{i, detail::rational{0}}; },
                  lhs.Interval / interval{step, rhs.Interval.Upper});
    }
    else
    {
      return detail::lift([](interval i){ return grid{i, detail::rational{0}}; },
                  lhs.Interval / interval{rhs.Interval.Lower, -step});
    }
  }

  //---------------------------------------------------------------------------
  // hull
  //---------------------------------------------------------------------------
  // The smallest grid that represents every value of both operands exactly:
  // interval hull + notch gcd. A valid grid anchors Lower on a multiple of its
  // notch, so both lattices are sub-lattices of the gcd lattice — no offset
  // term is needed, and the hull is a valid grid by construction. A continuous
  // operand (Notch 0) makes the hull continuous. errc::overflow when the notch gcd's
  // combined denominator exceeds the representable rational range.
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<grid, errc> hull(const grid& lhs, const grid& rhs)
  {
    const interval iv{lhs.Interval.Lower < rhs.Interval.Lower ? lhs.Interval.Lower : rhs.Interval.Lower,
                      lhs.Interval.Upper < rhs.Interval.Upper ? rhs.Interval.Upper : lhs.Interval.Upper};
    if (lhs.Notch == 0 || rhs.Notch == 0)
      return grid{iv, detail::rational{0}};
    return detail::lift([iv](detail::rational g){ return grid{iv, g}; },
                detail::gcd(lhs.Notch, rhs.Notch));
  }
} // namespace beman::inside

#endif // BEMAN_INSIDE_GRID_HPP
