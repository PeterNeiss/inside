// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_GRID_HPP
#define BEMAN_INSIDE_GRID_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <beman/inside/lift.hpp>
#include <beman/inside/detail/rational.hpp>
#include <beman/inside/interval.hpp>
#include <beman/inside/detail/grid_rational.hpp>
#include <beman/inside/policy_flag.hpp>
#include <beman/inside/detail/int_for_bits.hpp>

#include <expected> // std::expected, std::unexpected

#include <bit>
#include <concepts> // std::convertible_to (grid corner ctors)

namespace beman::inside {
//---------------------------------------------------------------------------
// grid — structural NTTP type (public members only). Discretizes its interval
// into notch-sized steps (interval must divide evenly by notch; Notch == 0
// allows every rational, raw not offset). Its operator+/-/*// is the engine of
// compile-time result-grid inference: every inside arithmetic operator computes
// its result grid here, so the result interval contains every reachable value.
//---------------------------------------------------------------------------
// A grid corner: a number (int / float / rational / grid number), or
// anything that converts to the 64-bit rational, such as an inside (the grid
// number takes that one conversion).
template <typename T>
concept grid_number_like = std::convertible_to<T, detail::grid_rational>;

// Which limits of a grid are values. A grid is always stored closed; an open
// limit is a spelling, moved in by one notch at construction, so
// {0, 360, bounds::half_open} is the grid {0, 359} — the same type.
enum class bounds : unsigned char {
    closed,    // [Lower, Upper] — the default
    half_open, // [Lower, Upper) — the last value is Upper − Notch
    open,      // (Lower, Upper) — Lower + Notch .. Upper − Notch
};

// The largest maximum denominator a continuous grid can carry: past it a
// bound would size fractions without limit along an expression chain, so it
// is dropped (the grid is unbounded, as before). C++26 big grids only; with
// 64-bit grid numbers a bound must fit the 64-bit rational.
#ifndef BEMAN_INSIDE_MAX_FRACTION_BITS
    #define BEMAN_INSIDE_MAX_FRACTION_BITS 1024
#endif

inline namespace BEMAN_INSIDE_GRID_ABI {
// A continuous grid whose values have denominators of at most Value:
// inside<{{0, 1}, max_denominator{1000}}>. The bound makes exact arithmetic
// predictable — the result type knows how large every fraction can get.
struct max_denominator {
    detail::grid_rational Value;
    constexpr explicit max_denominator(grid_number_like auto v) : Value{detail::grid_rational{v}} {}
    constexpr explicit max_denominator(const detail::grid_rational& v) : Value{v} {}
};

struct grid {
    interval              Interval;
    detail::grid_rational Notch;
    // A continuous grid's bound on its values' denominators; 0 is unbounded
    // (any rational in range). Always 0 on a notched or point grid, whose
    // lattice already bounds them — so a value set has one grid.
    detail::grid_rational MaxDenominator{0};

    grid() = default;
    // Corner ctors accept any type convertible to `rational` — int/float/rational and
    // any `inside` / `just<>` (via its implicit `operator rational()`), so an inside can be
    // a grid corner. They stay *templates* (deducing the corner type) on purpose: a
    // braced `{lo, hi}` can't deduce to a template parameter, so the `grid{{lo,hi}, notch}`
    // spelling unambiguously picks `grid(interval, rational)` below. The conversion is
    // resolved at the call site, so grid.hpp needs no dependency on `inside`.
    constexpr grid(grid_number_like auto lower, grid_number_like auto upper, grid_number_like auto notch)
        : grid{interval{to_number(lower), to_number(upper)}, to_number(notch)} {}
    // Two limits: the notch is derived — gcd(1, Lower, Upper), the coarsest
    // step 1/k that keeps every integer and both limits on the lattice. Integer
    // limits give 1; {0.5, 10} gives 1/2; {just<-6, 5>, just<3, 5>} gives 1/5.
    constexpr grid(grid_number_like auto lower, grid_number_like auto upper)
        : grid{interval{to_number(lower), to_number(upper)}, derive_notch(lower, upper)} {}
    constexpr grid(grid_number_like auto lower)
        : grid{interval{to_number(lower), to_number(lower)}, detail::grid_rational{0}} {}
    constexpr grid(interval val, detail::grid_rational notch) : Interval{val}, Notch{notch} {}
    // The same with open limits: {{0, 64}, per<16>, bounds::half_open} is
    // {{0, 63.9375}, per<16>}, and {0, 360, bounds::half_open} — notch derived
    // from the limits as written — is {0, 359}.
    constexpr grid(grid_number_like auto lower, grid_number_like auto upper, grid_number_like auto notch, bounds b)
        : grid{interval{to_number(lower), to_number(upper)}, to_number(notch), b} {}
    constexpr grid(grid_number_like auto lower, grid_number_like auto upper, bounds b)
        : grid{interval{to_number(lower), to_number(upper)}, derive_notch(lower, upper), b} {}
    constexpr grid(interval val, detail::grid_rational notch, bounds b)
        : Interval{close_bounds(val, notch, b)}, Notch{notch} {}
    // A continuous grid with bounded denominators: {{0, 1}, max_denominator{1000}}.
    constexpr grid(interval val, max_denominator d)
        : Interval{val}, Notch{0}, MaxDenominator{check_bound(val, d.Value)} {}

  private:
    // A bound is a positive integer at least the limits' denominators (each
    // limit is a value); with 64-bit grid numbers it must fit the rational.
    static constexpr detail::grid_rational check_bound(const interval& iv, const detail::grid_rational& d) {
        using detail::wide_denominator;
        if (!(d > 0) || wide_denominator(d) != detail::grid_int{1})
            detail::constexpr_error<"max_denominator: the bound must be a positive integer">();
        if (detail::wide_numerator(d) < wide_denominator(iv.Lower) ||
            detail::wide_numerator(d) < wide_denominator(iv.Upper))
            detail::constexpr_error<"max_denominator: the bound is below a limit's denominator">();
        return d;
    }

    static constexpr interval close_bounds(interval iv, const detail::grid_rational& notch, bounds b) {
        if (b == bounds::closed)
            return iv;
        if (notch == 0)
            detail::constexpr_error<"bounds: an open limit needs a notch - a continuous grid has no next point">();
        if (b == bounds::open)
            iv.Lower = detail::grid_add(iv.Lower, notch);
        iv.Upper = detail::grid_sub(iv.Upper, notch);
        if (iv.Lower > iv.Upper)
            detail::constexpr_error<"bounds: no grid point lies between the limits">();
        return iv;
    }

    template <grid_number_like T>
    static constexpr detail::grid_rational to_number(const T& v) {
        return detail::grid_rational{v};
    }

    // With 64-bit grid numbers a combined denominator past imax has no
    // rational notch: fall back to a continuous grid (notch 0), always valid.
    static constexpr detail::grid_rational derive_notch(grid_number_like auto lower, grid_number_like auto upper) {
        check_short_binary(lower);
        check_short_binary(upper);
        const detail::grid_rational lo = to_number(lower), hi = to_number(upper);
        return detail::lift([](const detail::grid_rational& a,
                               const detail::grid_rational& b) { return detail::grid_gcd(a, b); },
                            detail::lift([](const detail::grid_rational& a,
                                            const detail::grid_rational& b) { return detail::grid_gcd(a, b); },
                                         detail::grid_rational{1},
                                         lo),
                            hi)
            .value_or(detail::grid_rational{0});
    }

    // A floating-point limit is taken as its exact binary value, so 0.1 would
    // derive a 2^-55 notch. Past 1/1024 the literal almost surely meant a
    // decimal: reject it at compile time and point to the exact spellings.
    template <grid_number_like T>
    static constexpr void check_short_binary([[maybe_unused]] T v) {
        if constexpr (std::floating_point<T>)
            if (std::is_constant_evaluated() &&
                detail::grid_int{1024} < detail::wide_denominator(detail::grid_rational{v}))
                detail::constexpr_error<
                    // Clang prints only the first ~34 characters: lead with the fix.
                    "float limit: use _r literal (0.1_r) or give a notch {{lo, hi}, per<D>}; "
                    "grid{lo, hi} derives a notch from a floating-point limit only down to "
                    "1/1024 (0.1 is not 1/10 in binary)">();
    }

  public:
    template <auto G>
    static constexpr bool validate() {
        interval::validate<G.Interval>();
        // Decoding is Lower + raw·Notch: a negative notch would count downward.
        static_assert(G.Notch >= 0, "grid: the notch must be non-negative");
        // The values are Lower, Lower + Notch, …, Upper: the notch steps from
        // Lower to Upper. Lower need not be a multiple of the notch
        // ({{0.5, 10.5}, 1} holds 0.5, 1.5, …).
        static_assert(G.Interval.divides_evenly(G.Notch), "grid: the notch must divide Upper − Lower evenly");
        static_assert(G.MaxDenominator == 0 || (G.Notch == 0 && G.Interval.Lower != G.Interval.Upper),
                      "grid: only a continuous grid carries a max_denominator");
        // Every value is a multiple of gcd(Lower, Notch): {{0.2, 1.2}, 2^-61}
        // counts in 1/(5·2^61), past the 64-bit rational. (The grid ops
        // report such a result grid as overflow, so this fires only for a
        // grid written out.)
        static_assert(G.Notch == 0 || detail::grid_gcd_fits(G.Notch, G.Interval.Lower),
                      "grid: the value unit gcd(Lower, Notch) does not fit a 64-bit rational (C++26 big grids lift "
                      "this)");

        return true;
    }

    // Runtime sibling of validate<G>(): same invariants, but returns a typed
    // error instead of failing a static_assert — for grids built from runtime
    // config. A value, so it can't be an inside<G,P> template argument.
    // An open limit (`bounds`) moves in by one notch first, as in the ctors.
    [[nodiscard]] static constexpr std::expected<grid, errc>
    try_make(interval iv, detail::grid_rational notch, bounds b = bounds::closed) {
        if (iv.Lower > iv.Upper)
            return std::unexpected{errc::domain_error};
        if (notch < 0)
            return std::unexpected{errc::domain_error};
        if (b != bounds::closed) {
            if (notch == 0)
                return std::unexpected{errc::domain_error};
            if (b == bounds::open)
                iv.Lower = detail::grid_add(iv.Lower, notch);
            iv.Upper = detail::grid_sub(iv.Upper, notch);
            if (iv.Lower > iv.Upper)
                return std::unexpected{errc::domain_error};
        }
        if (!iv.divides_evenly(notch))
            return std::unexpected{errc::rounding_error};
        if (notch != 0 && !detail::grid_gcd_fits(notch, iv.Lower))
            return std::unexpected{errc::overflow};
        return grid{iv, notch};
    }

    // Exact slot count (Upper − Lower)/Notch, however large: with Upper = a/b,
    // Lower = c/d and Notch = e/f it is (a·d − c·b)·f / (b·d·e), exact on a
    // valid grid. 0 for a continuous grid.
    [[nodiscard]] constexpr detail::grid_int slot_count() const noexcept {
        using detail::wide_numerator, detail::wide_denominator;
        if (Notch == 0)
            return detail::grid_int{0};
        const auto&            U = Interval.Upper;
        const auto&            L = Interval.Lower;
        const detail::grid_int num =
            (wide_numerator(U) * wide_denominator(L) - wide_numerator(L) * wide_denominator(U)) *
            wide_denominator(Notch);
        return num / (wide_denominator(U) * wide_denominator(L) * wide_numerator(Notch));
    }

    // Bits needed to hold every slot index 0..slot_count().
    [[nodiscard]] constexpr int slot_bits() const noexcept { return bit_width_of(slot_count()); }

    // The slot count as a umax; false (out = 0) when it needs more than 64
    // bits — such a grid stores a wide_int index.
    [[nodiscard]] constexpr bool max_index_checked(umax& out) const {
        const detail::grid_int c    = slot_count();
        const bool             fits = !(detail::grid_int{std::numeric_limits<umax>::max()} < c);
        out                         = fits ? static_cast<umax>(c) : umax{0};
        return fits;
    }

    // Index-storage slot count (0 on overflow: such grids store a wide index).
    [[nodiscard]] constexpr umax max_index() const {
        umax c = 0;
        (void)max_index_checked(c);
        return c;
    }

    // True when the slot count fits umax (a builtin index). False ⇒ the grid is
    // still valid and stores a wide index.
    [[nodiscard]] constexpr bool max_index_representable() const {
        umax c = 0;
        return max_index_checked(c);
    }

    // True when `v` is an *exact* slot: in the interval AND on a notch (notch-0
    // grids store verbatim, so any in-range value qualifies). Used to admit a
    // single representable value (e.g. `0_ins`) regardless of whole-range mapping.
    [[nodiscard]] constexpr bool representable(detail::grid_rational v) const noexcept {
        if (!includes(Interval, v))
            return false;
        if (Notch == 0)
            return MaxDenominator == 0 || !(detail::wide_numerator(MaxDenominator) < detail::wide_denominator(v));
        // (Without forming (v − Lower)/Notch: on a fine notch it may leave
        // the 64-bit rational although v lies on the lattice.)
        return detail::grid_same_lattice(v, Interval.Lower, Notch);
    }
    // A double exactly: one finer than 2^-62 has a denominator past every
    // 64-bit grid's (and fits no 64-bit rational store); a big grid number
    // past 64 bits exists only at compile time.
    template <std::floating_point T>
    [[nodiscard]] constexpr bool representable(T d) const noexcept {
        if (!includes(Interval, d))
            return false;
        if (detail::fits_rational(Interval.Lower) && detail::fits_rational(Interval.Upper) &&
            detail::fits_rational(Notch)) {
            if constexpr (detail::wide_float<T>)
                if (!detail::is_double_value(d)) { // a long double with bits past the double's
                    bool       exact = false;
                    const auto r     = detail::rational_of_wide_float(d, exact);
                    return exact && representable(detail::grid_rational{r});
                }
            if (!detail::abs_fraction_exact(static_cast<double>(d)))
                return false;
            return representable(detail::grid_rational{d});
        }
#if BEMAN_INSIDE_BIG_GRIDS
        // Big grid numbers: d as a transient fraction, which (unlike a big
        // grid number) also exists at runtime.
        const auto f = detail::big::fraction_of_float(d);
        if (Notch == 0)
            return MaxDenominator == 0 || detail::big::compare(f.Den, MaxDenominator.Num.magnitude()) <= 0;
        return detail::big::same_lattice(f, detail::fraction_of(Interval.Lower), detail::fraction_of(Notch));
#else
        return representable(detail::grid_rational{d}); // (unreachable: 64-bit numbers always fit)
#endif
    }

    // Whether the lattice passes through 0: Lower is a multiple of the notch
    // (every continuous grid is). An unanchored grid such as {{0.5, 10.5}, 1}
    // has its values offset from the multiples of the notch.
    [[nodiscard]] constexpr bool anchored() const { return detail::grid_divides_evenly(Interval.Lower, Notch); }

    // The largest number every value is an integer multiple of: gcd(Notch,
    // Lower) — the notch on an anchored grid, finer on an unanchored one
    // ({{0.5, 10.5}, 1}: 1/2). 0 for a continuous grid.
    [[nodiscard]] constexpr detail::grid_rational value_unit() const {
        if (Notch == 0 || anchored())
            return Notch;
#if BEMAN_INSIDE_BIG_GRIDS
        return detail::grid_gcd(Notch, Interval.Lower);
#else
        return detail::grid_gcd(Notch, Interval.Lower).value();
#endif
    }

    // operator== be default for structural type
    [[nodiscard]] constexpr bool operator==(const grid& rhs) const = default;
    [[nodiscard]] constexpr grid operator-() const {
        grid g{-Interval, Notch};
        g.MaxDenominator = MaxDenominator;
        return g;
    }

    // (Raw → double decoding lives in `detail::as_double` (detail/storage.hpp): the
    // decode depends on the storage KIND, not the raw type's signedness — a
    // whole-number grid above 0 has an unsigned raw that IS the value.)
};
} // namespace BEMAN_INSIDE_GRID_ABI

namespace detail {
//---------------------------------------------------------------------------
// Denominator bounds — every value of a grid is n/d with d ≤ den_bound and
// |n| ≤ num_bound. A lattice bounds d by its value unit's denominator (the
// lcm of Lower's and the notch's), a point by its own, a continuous grid by
// MaxDenominator; 0 is unbounded. The bounds hold for unreduced fractions
// too, so an exact result can be sized from its operands' bounds.
//---------------------------------------------------------------------------
constexpr bool continuous_grid(const grid& g) { return g.Notch == 0 && g.Interval.Lower != g.Interval.Upper; }

constexpr grid_int den_bound(const grid& g) {
    if (continuous_grid(g))
        return wide_numerator(g.MaxDenominator);
    if (g.Notch == 0)
        return wide_denominator(g.Interval.Lower);
    // The value unit gcd(Lower, Notch) has denominator lcm(den Lower, den
    // Notch), taken here in wide integers: the unit itself may not fit a
    // 64-bit rational (a grid op then reports overflow, not a hard error).
    const grid_int a = wide_denominator(g.Interval.Lower), b = wide_denominator(g.Notch);
    return a / grid_int_gcd(a, b) * b;
}

// floor(max(|Lower|, |Upper|) · d): the largest numerator over denominator d.
constexpr grid_int num_bound(const interval& iv, const grid_int& d) {
    auto mag = [&](const grid_rational& x) {
        const grid_int n = wide_numerator(x);
        return (n < grid_int{0} ? -n : n) * d / wide_denominator(x);
    };
    const grid_int a = mag(iv.Lower), b = mag(iv.Upper);
    return a < b ? b : a;
}
constexpr grid_int num_bound(const grid& g) { return num_bound(g.Interval, den_bound(g)); }

// The result grid of an exact op whose values are continuous on `iv` with
// denominators ≤ d (0: unbounded). The bound is kept while it can size a
// fraction: within the 64-bit rational with 64-bit grid numbers, within
// BEMAN_INSIDE_MAX_FRACTION_BITS with big ones — else the grid is unbounded.
constexpr grid bounded_continuous(const interval& iv, grid_int d) {
    grid g{iv, grid_rational{0}};
    if (d == grid_int{0} || iv.Lower == iv.Upper) // a point has its own denominator
        return g;
    // Each limit is a value of the result, but an operator's interval may be
    // wider than the values it reaches: cover the limits' denominators.
    for (const grid_rational* x : {&iv.Lower, &iv.Upper})
        if (d < wide_denominator(*x))
            d = wide_denominator(*x);
    const grid_int n = num_bound(iv, d);
#if BEMAN_INSIDE_BIG_GRIDS
    if (bit_width_of(n) + bit_width_of(d) > BEMAN_INSIDE_MAX_FRACTION_BITS)
        return g;
    g.MaxDenominator = grid_rational{d};
#else
    if (grid_int{std::numeric_limits<imax>::max()} < d || grid_int{std::numeric_limits<umax>::max()} < n)
        return g;
    g.MaxDenominator = rational{static_cast<imax>(d)};
#endif
    return g;
}

// The result grid of an op whose values step by n from iv.Lower. Every value
// is a multiple of gcd(Lower, n); with 64-bit grid numbers that unit may
// leave the rational range ({{1/5, 6/5}, 2^-61}) — overflow, as for a limit
// past it, so the ops' fits-tests see it instead of validate() failing.
constexpr std::expected<grid, errc> lattice_grid(const interval& iv, const grid_rational& n) {
    if (n != 0 && !grid_gcd_fits(n, iv.Lower))
        return std::unexpected{errc::overflow};
    return grid{iv, n};
}
} // namespace detail

namespace detail {
// The value index v/Notch of a double v rounded onto G by mode M, the
// integer storage's rule (rounding_of; ties of `nearest` half away from
// zero). G is anchored with a power-of-two notch whose values double holds
// (double_exact), so v/Notch is exact and below 2^53 for every v in range.
// G and M are template parameters so each store compiles to its own
// branch-free rounding.
template <grid G, round_mode M>
[[nodiscard]] constexpr imax snap_double_index(double v) noexcept {
    constexpr double nd = static_cast<double>(G.Notch);
    const double     q  = v / nd;
    const imax       t  = static_cast<imax>(q);       // toward zero
    const double     f  = q - static_cast<double>(t); // exact, sign of q, |f| < 1
    if constexpr (M == round_mode::nearest)
        return t + (f >= 0.5) - (G.Interval.Lower < 0 && f <= -0.5);
    else if constexpr (M == round_mode::floor)
        return t - (f < 0);
    else if constexpr (M == round_mode::ceil)
        return t + (f > 0);
    else if constexpr (M == round_mode::half_even)
        return t + (f > 0.5 || (f == 0.5 && (t & 1))) - (f < -0.5 || (f == -0.5 && (t & 1)));
    else
        return t;
}
} // namespace detail

// Raw of a point grid (Lower == Upper): its value lives in the type, so the
// raw is empty. It acts as index slot 0 — constructible from any index,
// converting to integer 0 — so the index-storage decode (Lower + raw·Notch)
// yields the point's value without special cases. Declared
// [[no_unique_address]] in inside, a point member of another struct (also
// marked [[no_unique_address]]) takes no space.
namespace detail {
struct point_slot {
    constexpr point_slot() = default;
    template <typename T>
        requires std::is_arithmetic_v<T>
    constexpr point_slot(T) noexcept {}                         // any index: the only slot
    constexpr point_slot(const rational&) noexcept {}           // any value: the type holds it
    constexpr      operator imax() const noexcept { return 0; } // reads as index 0
    constexpr bool operator==(const point_slot&) const  = default;
    constexpr auto operator<=>(const point_slot&) const = default;
};
} // namespace detail

// Both endpoints lie in imax — the signed value raw candidates (and every
// `trunc(endpoint)` constant) are only meaningful then.
namespace detail {
// Notch 1 from an integer Lower: the values are integers, so a raw can hold
// the value itself (value storage). {{0.5, 10.5}, 1} has notch 1 but not
// integer values.
constexpr bool unit_lattice(const grid& g) noexcept {
    return g.Notch == 1 && wide_denominator(g.Interval.Lower) == grid_int{1};
}

constexpr bool fits_imax(const interval& iv) noexcept {
    return iv.Lower >= rational{std::numeric_limits<imax>::min()} &&
           iv.Upper <= rational{std::numeric_limits<imax>::max()};
}
} // namespace detail

// Storage is a function of the grid alone. Order: point → empty point_slot;
// notch zero → an exact fraction (no integer index space); more than 2^64
// slots → a wide_int index; a whole-number grid → its value where that costs
// no width (deduces_value); otherwise the unsigned 0-based index.
namespace detail {
// Unsigned index raw for G's slots: a builtin up to 64 bits, else wide.
template <grid G>
using index_raw_for_t = std::conditional_t<G.max_index_representable(),
                                           smallest_uint_for_t<G.max_index()>,
                                           int_for_bits_t<G.slot_bits(), false>>;

// A whole-number grid stores the value itself where that is free: below
// zero a signed value (within int64); from 0 the index is the value; above
// 0 the value when its unsigned type is no wider than the index's ({5, 100}
// stores 5..100 in a uint8_t, {200, 300} the index 0..100, as 300 needs 16
// bits).
template <grid G>
inline constexpr bool deduces_value = [] {
    if constexpr (G.Interval.Lower == G.Interval.Upper || !unit_lattice(G) || !G.max_index_representable())
        return false;
    else if constexpr (G.Interval.Lower < 0)
        return fits_imax(G.Interval);
    else if constexpr (G.Interval.Lower == 0)
        return true;
    else if constexpr (!fits_imax(G.Interval))
        return false;
    else
        return sizeof(smallest_uint_for_t<static_cast<umax>(trunc(G.Interval.Upper))>) <=
               sizeof(smallest_uint_for_t<G.max_index()>);
}();

// A continuous grid stores its value as an exact fraction: the 64-bit
// rational, or — past it (C++26) — a reduced fraction of K-limb integers.
// A bounded grid sizes K from its bounds, so every value fits; an unbounded
// one from its limits, K holding twice the limits' bits, and a value that
// needs more reports overflow, as one past the 64-bit rational does.
#if BEMAN_INSIDE_BIG_GRIDS
template <grid G>
inline constexpr bool bound_fits_rational =
    G.MaxDenominator != 0 && !(grid_int{std::numeric_limits<imax>::max()} < den_bound(G)) &&
    !(grid_int{std::numeric_limits<umax>::max()} < num_bound(G));

template <grid G>
inline constexpr std::size_t continuous_limbs = [] {
    if constexpr (G.MaxDenominator != 0) {
        const int n = bit_width_of(num_bound(G)), d = bit_width_of(den_bound(G));
        return limbs_for_bits((n < d ? d : n) + 1);
    } else {
        auto bits = [](const grid_rational& r) {
            const grid_int n = wide_numerator(r);
            return bit_width_of(abs(n)) + bit_width_of(wide_denominator(r));
        };
        const int b =
            bits(G.Interval.Lower) > bits(G.Interval.Upper) ? bits(G.Interval.Lower) : bits(G.Interval.Upper);
        return limbs_for_bits(2 * b + 2);
    }
}();
template <grid G>
using continuous_raw_t =
    std::conditional_t<G.MaxDenominator != 0 ? bound_fits_rational<G>
                                             : fits_rational(G.Interval.Lower) && fits_rational(G.Interval.Upper),
                       detail::rational,
                       wide_rational<continuous_limbs<G>>>;
#else
template <grid G>
using continuous_raw_t = detail::rational;
#endif

template <grid G>
constexpr auto raw_for() {
    if constexpr (G.Interval.Lower == G.Interval.Upper)
        return point_slot{};
    else if constexpr (G.Notch == 0)
        return continuous_raw_t<G>{};
    else if constexpr (!deduces_value<G>)
        return index_raw_for_t<G>{};
    else if constexpr (G.Interval.Lower < 0)
        return smallest_int_for_t<trunc(G.Interval.Lower), trunc(G.Interval.Upper)>{};
    else if constexpr (G.Interval.Lower == 0)
        return smallest_uint_for_t<G.max_index()>{};
    else
        return smallest_uint_for_t<static_cast<umax>(trunc(G.Interval.Upper))>{};
}

// The raw type of inside<G, P> (the policy plays no part).
template <grid G>
using raw_for_t = decltype(raw_for<G>());

// Dyadic grid: power-of-2 notch denominator and Lower denominator, so every
// on-grid value is a binary fraction (a double when it fits double_exact).
// A positive power of two, and its log2 (exact at any width).
constexpr bool is_pow2(const grid_int& v) noexcept {
    return grid_int{0} < v && v == (grid_int{1} << (bit_width_of(v) - 1));
}

template <grid G>
inline constexpr bool dyadic_grid =
    G.Notch != 0 && is_pow2(wide_denominator(G.Notch)) && is_pow2(wide_denominator(G.Interval.Lower));

// Bits of |r · 2^f| — an integer on a dyadic grid, where r's denominator is
// a power of two dividing 2^f (0 for r == 0).
constexpr int scaled_numerator_bits(const grid_rational& r, int f) noexcept {
    const grid_int n = wide_numerator(r);
    if (n == grid_int{0})
        return 0;
    return bit_width_of(abs(n)) + f - (bit_width_of(wide_denominator(r)) - 1);
}

// `fp`-exactness of a dyadic grid: the IEEE-754 path equals the exact grid
// arithmetic iff, at the coarsest-magnitude end, the value's ULP is no
// coarser than the notch. Writing v = N·2^(−f) with f = log2(den(Notch)),
// that is |N| < 2^Digits (the significand) AND f ≤ MaxF (notch ≥ the
// smallest normal, so no on-grid value is subnormal). The overflow ceiling
// is unreachable once |N| < 2^Digits.
template <grid G, int Digits, int MaxF>
constexpr bool compute_fp_exact() noexcept {
    if constexpr (!dyadic_grid<G>)
        return false;
    else {
        // f: the finest power of two among the values — the notch's, or
        // Lower's on an unanchored grid ({{0.25, 4.25}, 1}: 2^-2).
        constexpr int f = bit_width_of(wide_denominator(G.value_unit())) - 1;
        return f <= MaxF && scaled_numerator_bits(G.Interval.Lower, f) <= Digits &&
               scaled_numerator_bits(G.Interval.Upper, f) <= Digits;
    }
}

// double: 53-bit significand, notch at least 2^-1022 — every value of G is
// a double.
template <grid G>
inline constexpr bool double_exact = compute_fp_exact<G, 53, 1022>();

} // namespace detail

template <std::same_as<grid> G>
[[nodiscard]] constexpr std::expected<grid, errc> operator+(const G&, const G&);
template <std::same_as<grid> G>
[[nodiscard]] constexpr std::expected<grid, errc> operator-(const G&, const G&);
template <std::same_as<grid> G>
[[nodiscard]] constexpr std::expected<grid, errc> operator*(const G&, const G&);
template <std::same_as<grid> G>
[[nodiscard]] constexpr std::expected<grid, errc> operator/(const G&, const G&);
// (Templates on G = grid: no conversion reaches them, so an inside — which
// converts to its point grid — never lands here when its own operator is
// constrained away.)

//---------------------------------------------------------------------------
// grid_sum_fits / grid_product_fits — whether a + b / a × b has a result
// grid. With 64-bit grid numbers a limit or notch can leave the rational
// range; these test it with the quiet try_ ops, so an arithmetic operator
// can static_assert with its own message before the result grid's loud
// rational error. A lattice result needs its value unit too (lattice_grid).
// Big grid numbers (C++26) always fit.
//---------------------------------------------------------------------------
namespace detail {
constexpr bool grid_sum_fits([[maybe_unused]] const grid& a, [[maybe_unused]] const grid& b) noexcept {
#if BEMAN_INSIDE_BIG_GRIDS
    return true;
#else
    const auto lo = try_add(a.Interval.Lower, b.Interval.Lower);
    const auto n  = gcd(a.Notch, b.Notch);
    if (!lo || !try_add(a.Interval.Upper, b.Interval.Upper) || !n)
        return false;
    return continuous_grid(a) || continuous_grid(b) || *n == 0 || grid_gcd_fits(*n, *lo);
#endif
}

// The notch of a × b. A product (La + i·Na)(Lb + j·Nb) differs from La·Lb
// by multiples of Na·Nb, Na·Lb and Nb·La; on an anchored operand its term is
// already a multiple of Na·Nb, so only an unanchored operand adds one. A
// point c (notch 0) scales the other lattice: the notch becomes N·|c|.
// (Quiet products: grid_product_fits asks this, so a product past the
// rational range is an error value, not a compile error.)
constexpr std::expected<grid_rational, errc> product_notch(const grid& a, const grid& b) {
    const bool          ap = a.Interval.Lower == a.Interval.Upper, bp = b.Interval.Lower == b.Interval.Upper;
    const grid_rational an  = (ap && !bp) ? abs(a.Interval.Lower) : a.Notch;
    const grid_rational bn  = (bp && !ap) ? abs(b.Interval.Lower) : b.Notch;
    auto                gcd = [](const grid_rational& x, const grid_rational& y) { return grid_gcd(x, y); };
    auto                mul = [](const grid_rational& x, const grid_rational& y) {
#if BEMAN_INSIDE_BIG_GRIDS
        return x * y;
#else
        return try_mul(x, y);
#endif
    };
    std::expected<grid_rational, errc> n = lift([](const grid_rational& x) { return x; }, mul(an, bn));
    if (ap || bp)
        return n;
    if (!b.anchored())
        n = lift(gcd, n, mul(a.Notch, b.Interval.Lower));
    if (!a.anchored())
        n = lift(gcd, n, mul(b.Notch, a.Interval.Lower));
    return n;
}

constexpr bool grid_product_fits([[maybe_unused]] const grid& a, [[maybe_unused]] const grid& b) noexcept {
#if BEMAN_INSIDE_BIG_GRIDS
    return true;
#else
    const auto c0 = try_mul(a.Interval.Lower, b.Interval.Lower), c1 = try_mul(a.Interval.Lower, b.Interval.Upper),
               c2 = try_mul(a.Interval.Upper, b.Interval.Lower), c3 = try_mul(a.Interval.Upper, b.Interval.Upper);
    const auto n = product_notch(a, b);
    if (!c0 || !c1 || !c2 || !c3 || !n)
        return false;
    if (*n == 0) // continuous or a point
        return true;
    grid_rational lo = *c0;
    for (const grid_rational& c : {*c1, *c2, *c3})
        lo = c < lo ? c : lo;
    return grid_gcd_fits(*n, lo);
#endif
}
// Whether a / b has a result grid: the divisor is not the point 0, an
// unbounded continuous divisor does not reach 0, and (64-bit grid numbers)
// every corner quotient fits — operator/ tested with the quiet try_ ops.
constexpr bool grid_quotient_fits([[maybe_unused]] const grid& a, const grid& b) {
    const interval& d = b.Interval;
    if (d.Lower == 0 && d.Upper == 0)
        return false;
    [[maybe_unused]] auto corners_fit = [&]([[maybe_unused]] const grid_rational& lo,
                                            [[maybe_unused]] const grid_rational& hi) {
#if BEMAN_INSIDE_BIG_GRIDS
        return true;
#else
        return try_div(a.Interval.Lower, lo) && try_div(a.Interval.Lower, hi) && try_div(a.Interval.Upper, lo) &&
               try_div(a.Interval.Upper, hi);
#endif
    };
    if (!includes(d, 0)) {
#if !BEMAN_INSIDE_BIG_GRIDS
        if (d.Lower == d.Upper && a.Notch != 0) { // a lattice scaled by 1/c (lattice_grid)
            const auto n = try_div(a.Notch, abs(d.Lower));
            const auto p = try_div(a.Interval.Lower, d.Lower), q = try_div(a.Interval.Upper, d.Lower);
            return n && p && q && grid_gcd_fits(*n, *p < *q ? *p : *q);
        }
#endif
        return corners_fit(d.Lower, d.Upper);
    }
    if (b.Notch == 0 && b.MaxDenominator == 0)
        return false;
#if BEMAN_INSIDE_BIG_GRIDS
    return true;
#else
    const auto step = b.Notch != 0 ? grid_gcd(b.Notch, d.Lower) : try_div(rational{1}, b.MaxDenominator);
    if (!step)
        return false;
    const grid_rational s = b.Notch != 0 && b.anchored() ? b.Notch : *step;
    return (!(0 < d.Upper) || corners_fit(s, d.Upper)) && (!(0 > d.Lower) || corners_fit(d.Lower, -s));
#endif
}
} // namespace detail

//---------------------------------------------------------------------------
// operator+
//---------------------------------------------------------------------------
template <std::same_as<grid> G>
[[nodiscard]] constexpr std::expected<grid, errc> operator+(const G& lhs, const G& rhs) {
    // A continuous operand makes the sum continuous, with denominators at most
    // the product of the operands' bounds. (A point, also notch 0, shifts the
    // other lattice: gcd(0, n) = n is its notch.)
    if (detail::continuous_grid(lhs) || detail::continuous_grid(rhs))
        return detail::lift([d = detail::den_bound(lhs) *
                                 detail::den_bound(rhs)](interval i) { return detail::bounded_continuous(i, d); },
                            lhs.Interval + rhs.Interval);
    // gcd returns expected — lift it so a notch-denominator overflow produces
    // errc::overflow rather than a silently wrapped result grid.
    return detail::lift(detail::lattice_grid, lhs.Interval + rhs.Interval, detail::grid_gcd(lhs.Notch, rhs.Notch));
}

//---------------------------------------------------------------------------
// operator-
//---------------------------------------------------------------------------
template <std::same_as<grid> G>
[[nodiscard]] constexpr std::expected<grid, errc> operator-(const G& lhs, const G& rhs) {
    return operator+(lhs, -rhs);
}

//---------------------------------------------------------------------------
// operator*
//---------------------------------------------------------------------------
template <std::same_as<grid> G>
[[nodiscard]] constexpr std::expected<grid, errc> operator*(const G& lhs, const G& rhs) {
    // A point operand c (notch 0) scales the other lattice exactly: its notch
    // becomes N·|c|, so `x * just<c>` keeps integer storage instead of turning
    // continuous. A continuous product bounds its denominators by the
    // product of the operands' bounds.
    return detail::lift(
        [d = detail::den_bound(lhs) * detail::den_bound(rhs)](interval i, detail::grid_rational n) {
            return n == 0 && i.Lower != i.Upper ? std::expected<grid, errc>{detail::bounded_continuous(i, d)}
                                                : detail::lattice_grid(i, n);
        },
        lhs.Interval * rhs.Interval,
        detail::product_notch(lhs, rhs));
}

//---------------------------------------------------------------------------
// operator/
//---------------------------------------------------------------------------
template <std::same_as<grid> G>
[[nodiscard]] constexpr std::expected<grid, errc> operator/(const G& lhs, const G& rhs) {
    auto d = lhs.Interval / rhs.Interval;
    // A nonzero point divisor c scales the other lattice exactly, as in
    // operator*: its notch becomes N/|c|, so `x / just<c>` keeps integer
    // storage instead of turning continuous. Any other quotient is continuous.
    if (d.has_value() && rhs.Interval.Lower == rhs.Interval.Upper && lhs.Notch != 0)
        return detail::lift(detail::lattice_grid, *d, lhs.Notch / detail::abs(rhs.Interval.Lower));
    // Any other quotient is continuous: x = p/a over y = r/b is p·b/(a·r), so
    // its denominators stay within den_bound(lhs)·num_bound(rhs).
    const detail::grid_int qd         = detail::den_bound(lhs) * detail::num_bound(rhs);
    auto                   continuous = [qd](interval i) { return detail::bounded_continuous(i, qd); };
    if (d.has_value())
        return continuous(*d);

    // Divisor interval includes zero — exclude zero for result interval.
    if (rhs.Interval.Lower == 0 && rhs.Interval.Upper == 0)
        return std::unexpected{errc::division_by_zero};

    // `step` bounds the non-zero divisor magnitudes from below; it splits the
    // divisor interval into positive [step, Upper] and negative [Lower, -step]
    // (skipping zero). Both sides present → the result is their union. Every
    // lattice value is a multiple of the value unit (the notch when anchored,
    // finer when not: {{-1.5, 1.5}, 1} holds ±0.5), and a bounded continuous
    // value p/q has q ≤ MaxDenominator. An unbounded continuous divisor comes
    // arbitrarily close to zero: no bounded quotient.
    if (rhs.Notch == 0 && rhs.MaxDenominator == 0)
        return std::unexpected{errc::overflow};
    detail::grid_rational step    = (rhs.Notch != 0) ? rhs.value_unit() : detail::grid_div_of(1, rhs.MaxDenominator);
    bool                  has_pos = 0 < rhs.Interval.Upper;
    bool                  has_neg = 0 > rhs.Interval.Lower;

    if (has_pos && has_neg) {
        return detail::lift(
            [continuous](interval pos, interval neg) {
                return continuous(interval{neg.Lower < pos.Lower ? neg.Lower : pos.Lower,
                                           neg.Upper < pos.Upper ? pos.Upper : neg.Upper});
            },
            lhs.Interval / interval{step, rhs.Interval.Upper},
            lhs.Interval / interval{rhs.Interval.Lower, -step});
    } else if (has_pos) {
        return detail::lift(continuous, lhs.Interval / interval{step, rhs.Interval.Upper});
    } else {
        return detail::lift(continuous, lhs.Interval / interval{rhs.Interval.Lower, -step});
    }
}

//---------------------------------------------------------------------------
// hull
//---------------------------------------------------------------------------
// The smallest grid that represents every value of both operands exactly:
// interval hull + notch gcd, refined by the offset between the two lattices
// when they do not line up ({{0, 2}, 1} and {{0.5, 1.5}, 1} hull to notch
// 1/2). A continuous operand (Notch 0) makes the hull continuous.
// errc::overflow when the notch's combined denominator exceeds the
// representable rational range.
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<grid, errc> hull(const grid& lhs, const grid& rhs) {
    const interval iv{lhs.Interval.Lower < rhs.Interval.Lower ? lhs.Interval.Lower : rhs.Interval.Lower,
                      lhs.Interval.Upper < rhs.Interval.Upper ? rhs.Interval.Upper : lhs.Interval.Upper};
    // A continuous operand makes the hull continuous, bounded by the larger
    // bound (unbounded if either is); a point (also notch 0) joins the other
    // lattice through the offset between them.
    if (detail::continuous_grid(lhs) || detail::continuous_grid(rhs)) {
        const detail::grid_int a = detail::den_bound(lhs), b = detail::den_bound(rhs);
        return detail::bounded_continuous(iv,
                                          a == detail::grid_int{0} || b == detail::grid_int{0} ? a * b
                                          : a < b                                              ? b
                                                                                               : a);
    }
    auto gcd = [](const detail::grid_rational& x, const detail::grid_rational& y) { return detail::grid_gcd(x, y); };
    std::expected<detail::grid_rational, errc> n = detail::lift(gcd, lhs.Notch, rhs.Notch);
    if (n && (*n == 0 || !detail::grid_same_lattice(lhs.Interval.Lower, rhs.Interval.Lower, *n)))
        n = detail::lift(gcd, n, rhs.Interval.Lower - lhs.Interval.Lower);
    return detail::lift([iv](detail::grid_rational g) { return grid{iv, g}; }, n);
}
} // namespace beman::inside

BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_GRID_HPP
