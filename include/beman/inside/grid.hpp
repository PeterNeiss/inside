// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_GRID_HPP
#define BEMAN_INSIDE_GRID_HPP

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
// anything that converts to the 64-bit rational, such as an inside.
template <typename T>
concept grid_number_like = std::convertible_to<T, detail::grid_rational> || std::convertible_to<T, detail::rational>;

inline namespace BEMAN_INSIDE_GRID_ABI {
struct grid {
    interval              Interval;
    detail::grid_rational Notch;

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
    // limits give 1; {0.5, 10} gives 1/2; {frac<-6,5>, frac<3,5>} gives 1/5.
    constexpr grid(grid_number_like auto lower, grid_number_like auto upper)
        : grid{interval{to_number(lower), to_number(upper)}, derive_notch(lower, upper)} {}
    constexpr grid(grid_number_like auto lower)
        : grid{interval{to_number(lower), to_number(lower)}, detail::grid_rational{0}} {}
    constexpr grid(interval val, detail::grid_rational notch) : Interval{val}, Notch{notch} {}

  private:
    template <typename T>
    static constexpr detail::grid_rational to_number(const T& v) {
        if constexpr (std::convertible_to<T, detail::grid_rational>)
            return detail::grid_rational{v};
        else
            return detail::grid_rational{detail::rational{v}};
    }

    // With 64-bit grid numbers a combined denominator past imax has no
    // rational notch: fall back to a continuous grid (notch 0), always valid.
    static constexpr detail::grid_rational derive_notch(auto lower, auto upper) {
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
    template <typename T>
    static constexpr void check_short_binary([[maybe_unused]] T v) {
        if constexpr (std::floating_point<T>)
            if (std::is_constant_evaluated() &&
                detail::grid_wide{1024} < detail::wide_denominator(detail::grid_rational{v}))
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

        return true;
    }

    // Runtime sibling of validate<G>(): same invariants, but returns a typed
    // error instead of failing a static_assert — for grids built from runtime
    // config. A value, so it can't be an inside<G,P> template argument.
    [[nodiscard]] static constexpr std::expected<grid, errc> try_make(interval iv, detail::grid_rational notch) {
        if (iv.Lower > iv.Upper)
            return std::unexpected{errc::domain_error};
        if (notch < 0)
            return std::unexpected{errc::domain_error};
        if (!iv.divides_evenly(notch))
            return std::unexpected{errc::rounding_error};
        return grid{iv, notch};
    }

    // Exact slot count (Upper − Lower)/Notch, however large: with Upper = a/b,
    // Lower = c/d and Notch = e/f it is (a·d − c·b)·f / (b·d·e), exact on a
    // valid grid. 0 for a continuous grid.
    [[nodiscard]] constexpr detail::grid_wide slot_count() const noexcept {
        using detail::wide_numerator, detail::wide_denominator;
        if (Notch == 0)
            return detail::grid_wide{0};
        const auto&             U = Interval.Upper;
        const auto&             L = Interval.Lower;
        const detail::grid_wide num =
            (wide_numerator(U) * wide_denominator(L) - wide_numerator(L) * wide_denominator(U)) *
            wide_denominator(Notch);
        return num / (wide_denominator(U) * wide_denominator(L) * wide_numerator(Notch));
    }

    // Bits needed to hold every slot index 0..slot_count().
    [[nodiscard]] constexpr int slot_bits() const noexcept { return bit_width_of(slot_count()); }

    // The slot count as a umax; false (out = 0) when it needs more than 64
    // bits — such a grid stores a wide_int index.
    [[nodiscard]] constexpr bool max_index_checked(umax& out) const {
        const detail::grid_wide c    = slot_count();
        const bool              fits = !(detail::grid_wide{std::numeric_limits<umax>::max()} < c);
        out                          = fits ? static_cast<umax>(c) : umax{0};
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
            return true;
#if BEMAN_INSIDE_BIG_GRIDS
        return detail::grid_divides_evenly(v - Interval.Lower, Notch);
#else
        auto diff = v - Interval.Lower; // expected<rational, errc>
        if (!diff)
            return false;
        auto off = diff.value() / Notch; // expected<rational, errc>
        return off.has_value() && detail::abs_den(off->Denominator) == 1;
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
    [[nodiscard]] constexpr grid operator-() const { return {-Interval, Notch}; }

    // (Raw → double decoding lives in `detail::as_double` (generic.hpp): the
    // decode depends on the storage KIND, not the raw type's signedness — a
    // whole-number grid above 0 has an unsigned raw that IS the value.)
};
} // namespace BEMAN_INSIDE_GRID_ABI

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
    return g.Notch == 1 && wide_denominator(g.Interval.Lower) == grid_wide{1};
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
// rational, or — for limits past 64 bits (C++26) — a reduced fraction of K-limb
// integers, K holding twice the limits' bits. A value that needs more reports
// overflow, as one past the 64-bit rational does.
#if BEMAN_INSIDE_BIG_GRIDS
template <grid G>
inline constexpr std::size_t frac_limbs = [] {
    auto bits = [](const grid_rational& r) {
        const grid_wide n = wide_numerator(r);
        return bit_width_of(n.negative() ? -n : n) + bit_width_of(wide_denominator(r));
    };
    const int b = bits(G.Interval.Lower) > bits(G.Interval.Upper) ? bits(G.Interval.Lower) : bits(G.Interval.Upper);
    return limbs_for_bits(2 * b + 2);
}();
template <grid G>
using continuous_raw_t = std::conditional_t<fits_rational(G.Interval.Lower) && fits_rational(G.Interval.Upper),
                                            detail::rational,
                                            exact_frac<frac_limbs<G>>>;
#else
template <grid G>
using continuous_raw_t = detail::rational;
#endif

template <grid G>
constexpr auto storage_min() {
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
using storage_min_t = decltype(storage_min<G>());

// Dyadic grid: power-of-2 notch denominator and Lower denominator, so every
// on-grid value is a binary fraction (a double when it fits double_exact).
// A positive power of two, and its log2 (exact at any width).
constexpr bool is_pow2(const grid_wide& v) noexcept {
    return grid_wide{0} < v && v == (grid_wide{1} << (bit_width_of(v) - 1));
}

template <grid G>
inline constexpr bool dyadic_grid =
    G.Notch != 0 && is_pow2(wide_denominator(G.Notch)) && is_pow2(wide_denominator(G.Interval.Lower));

// Bits of |r · 2^f| — an integer on a dyadic grid, where r's denominator is
// a power of two dividing 2^f (0 for r == 0).
constexpr int scaled_numerator_bits(const grid_rational& r, int f) noexcept {
    const grid_wide n = wide_numerator(r);
    if (n == grid_wide{0})
        return 0;
    return bit_width_of(n.negative() ? -n : n) + f - (bit_width_of(wide_denominator(r)) - 1);
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

[[nodiscard]] constexpr std::expected<grid, errc> operator+(const grid&, const grid&);
[[nodiscard]] constexpr std::expected<grid, errc> operator-(const grid&, const grid&);
[[nodiscard]] constexpr std::expected<grid, errc> operator*(const grid&, const grid&);
[[nodiscard]] constexpr std::expected<grid, errc> operator/(const grid&, const grid&);

//---------------------------------------------------------------------------
// grid_sum_fits / grid_product_fits — whether a + b / a × b has a result
// grid. With 64-bit grid numbers a limit or notch can leave the rational
// range; these test it with the quiet try_ ops, so an arithmetic operator
// can static_assert with its own message before the result grid's loud
// rational error. Big grid numbers (C++26) always fit.
//---------------------------------------------------------------------------
namespace detail {
constexpr bool grid_sum_fits([[maybe_unused]] const grid& a, [[maybe_unused]] const grid& b) noexcept {
#if BEMAN_INSIDE_BIG_GRIDS
    return true;
#else
    return try_add(a.Interval.Lower, b.Interval.Lower) && try_add(a.Interval.Upper, b.Interval.Upper) &&
           gcd(a.Notch, b.Notch);
#endif
}

// The notch of a × b. A product (La + i·Na)(Lb + j·Nb) differs from La·Lb
// by multiples of Na·Nb, Na·Lb and Nb·La; on an anchored operand its term is
// already a multiple of Na·Nb, so only an unanchored operand adds one. A
// point c (notch 0) scales the other lattice: the notch becomes N·|c|.
constexpr std::expected<grid_rational, errc> product_notch(const grid& a, const grid& b) {
    const bool          ap = a.Interval.Lower == a.Interval.Upper, bp = b.Interval.Lower == b.Interval.Upper;
    const grid_rational an  = (ap && !bp) ? abs(a.Interval.Lower) : a.Notch;
    const grid_rational bn  = (bp && !ap) ? abs(b.Interval.Lower) : b.Notch;
    auto                gcd = [](const grid_rational& x, const grid_rational& y) { return grid_gcd(x, y); };
    std::expected<grid_rational, errc> n = lift([](const grid_rational& x) { return x; }, an * bn);
    if (ap || bp)
        return n;
    if (!b.anchored())
        n = lift(gcd, n, a.Notch * b.Interval.Lower);
    if (!a.anchored())
        n = lift(gcd, n, b.Notch * a.Interval.Lower);
    return n;
}

constexpr bool grid_product_fits([[maybe_unused]] const grid& a, [[maybe_unused]] const grid& b) noexcept {
#if BEMAN_INSIDE_BIG_GRIDS
    return true;
#else
    return try_mul(a.Interval.Lower, b.Interval.Lower) && try_mul(a.Interval.Lower, b.Interval.Upper) &&
           try_mul(a.Interval.Upper, b.Interval.Lower) && try_mul(a.Interval.Upper, b.Interval.Upper) &&
           product_notch(a, b).has_value();
#endif
}
} // namespace detail

//---------------------------------------------------------------------------
// operator+
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<grid, errc> operator+(const grid& lhs, const grid& rhs) {
    // A continuous operand makes the sum continuous. (A point, also notch 0,
    // shifts the other lattice: gcd(0, n) = n is its notch.)
    auto continuous = [](const grid& g) { return g.Notch == 0 && g.Interval.Lower != g.Interval.Upper; };
    if (continuous(lhs) || continuous(rhs))
        return detail::lift([](interval i) { return grid{i, detail::grid_rational{0}}; }, lhs.Interval + rhs.Interval);
    // gcd returns expected — lift it so a notch-denominator overflow produces
    // errc::overflow rather than a silently wrapped result grid.
    return detail::lift([](interval i, detail::grid_rational n) { return grid{i, n}; },
                        lhs.Interval + rhs.Interval,
                        detail::grid_gcd(lhs.Notch, rhs.Notch));
}

//---------------------------------------------------------------------------
// operator-
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<grid, errc> operator-(const grid& lhs, const grid& rhs) {
    return operator+(lhs, -rhs);
}

//---------------------------------------------------------------------------
// operator*
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<grid, errc> operator*(const grid& lhs, const grid& rhs) {
    // A point operand c (notch 0) scales the other lattice exactly: its notch
    // becomes N·|c|, so `x * just<c>` keeps integer storage instead of turning
    // continuous.
    return detail::lift([](interval i, detail::grid_rational n) { return grid{i, n}; },
                        lhs.Interval * rhs.Interval,
                        detail::product_notch(lhs, rhs));
}

//---------------------------------------------------------------------------
// operator/
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<grid, errc> operator/(const grid& lhs, const grid& rhs) {
    auto d = lhs.Interval / rhs.Interval;
    if (d.has_value())
        return grid{*d, detail::grid_rational{0}};

    // Divisor interval includes zero — exclude zero for result interval.
    if (rhs.Interval.Lower == 0 && rhs.Interval.Upper == 0)
        return std::unexpected{errc::division_by_zero};

    // `step` = smallest non-zero divisor magnitude; splits the divisor interval
    // into positive [step, Upper] and negative [Lower, -step] (skipping zero).
    // Both sides present → the result is their union.
    detail::grid_rational step    = (rhs.Notch != 0) ? detail::abs(rhs.Notch) : detail::grid_rational{1};
    bool                  has_pos = 0 < rhs.Interval.Upper;
    bool                  has_neg = 0 > rhs.Interval.Lower;

    if (has_pos && has_neg) {
        return detail::lift(
            [](interval pos, interval neg) {
                return grid{interval{neg.Lower < pos.Lower ? neg.Lower : pos.Lower,
                                     neg.Upper < pos.Upper ? pos.Upper : neg.Upper},
                            detail::grid_rational{0}};
            },
            lhs.Interval / interval{step, rhs.Interval.Upper},
            lhs.Interval / interval{rhs.Interval.Lower, -step});
    } else if (has_pos) {
        return detail::lift([](interval i) { return grid{i, detail::grid_rational{0}}; },
                            lhs.Interval / interval{step, rhs.Interval.Upper});
    } else {
        return detail::lift([](interval i) { return grid{i, detail::grid_rational{0}}; },
                            lhs.Interval / interval{rhs.Interval.Lower, -step});
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
    // A continuous operand makes the hull continuous; a point (also notch 0)
    // joins the other lattice through the offset between them.
    auto continuous = [](const grid& g) { return g.Notch == 0 && g.Interval.Lower != g.Interval.Upper; };
    if (continuous(lhs) || continuous(rhs))
        return grid{iv, detail::grid_rational{0}};
    auto gcd = [](const detail::grid_rational& x, const detail::grid_rational& y) { return detail::grid_gcd(x, y); };
    std::expected<detail::grid_rational, errc> n = detail::lift(gcd, lhs.Notch, rhs.Notch);
    if (n && (*n == 0 || !detail::grid_same_lattice(lhs.Interval.Lower, rhs.Interval.Lower, *n)))
        n = detail::lift(gcd, n, rhs.Interval.Lower - lhs.Interval.Lower);
    return detail::lift([iv](detail::grid_rational g) { return grid{iv, g}; }, n);
}
} // namespace beman::inside

#endif // BEMAN_INSIDE_GRID_HPP
