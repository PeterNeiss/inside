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
        static_assert(G.Interval.divides_evenly(G.Notch));
        // Lower must sit on the notch lattice. divides_evenly avoids forming the
        // (possibly umax-overflowing) Lower/Notch quotient, so a grid finer than
        // uint64 index space is still valid (it stores as rational).
        static_assert(G.Notch == 0 || detail::grid_divides_evenly(G.Interval.Lower, G.Notch));

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
        if (notch != 0 && !detail::grid_divides_evenly(iv.Lower, notch))
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

    // Index-storage slot count (0 on overflow; the over-flow branch of storage_min
    // is discarded for such grids, which pick rational storage instead).
    [[nodiscard]] constexpr umax max_index() const {
        umax c = 0;
        (void)max_index_checked(c);
        return c;
    }

    // True when the slot count fits umax (index storage is possible). False ⇒ the
    // grid is still valid but stores its value as a rational, never an index.
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

    // operator== be default for structural type
    [[nodiscard]] constexpr bool operator==(const grid& rhs) const = default;
    [[nodiscard]] constexpr grid operator-() const { return {-Interval, Notch}; }

    // (Raw → double decoding lives in `detail::as_double` (generic.hpp): the
    // decode depends on the storage KIND, not the raw type's signedness — a
    // `direct`-policy inside has an unsigned raw that IS the value.)
};
} // namespace BEMAN_INSIDE_GRID_ABI

namespace detail {
// Snap a double onto the (dyadic) grid G by rounding mode M — the same rule
// as integer storage (rounding_of; ties of `nearest` half away from zero). On
// an fp grid the notch is a power of two, so v/notch is the exact signed value
// index. A continuous grid (notch 0) has nothing to snap to. |index| >= 2^52 is
// already integral, so the imax narrowing below is always safe. G and M are
// template parameters so each store compiles to its own branch-free rounding.
// AnySign: v may lie below a grid that starts at 0 or higher (the wrap path
// rounds out-of-range values); otherwise that half of the tie test is dead.
template <grid G, round_mode M = round_mode::nearest, bool AnySign = (G.Interval.Lower < 0)>
[[nodiscard]] constexpr double snap_double(double v) noexcept {
    if constexpr (G.Notch == rational{0})
        return v;
    else {
        constexpr double nd = static_cast<double>(G.Notch);
        const double     q  = v / nd;
        if ((q < 0 ? -q : q) >= 4503599627370496.0) // 2^52
            return v;
        const imax   t = static_cast<imax>(q);       // toward zero
        const double f = q - static_cast<double>(t); // exact, sign of q, |f| < 1
        imax         k = t;
        if constexpr (M == round_mode::nearest) {
            k += (f >= 0.5);
            if constexpr (AnySign)
                k -= (f <= -0.5);
        } else if constexpr (M == round_mode::floor)
            k -= (f < 0);
        else if constexpr (M == round_mode::ceil)
            k += (f > 0);
        else if constexpr (M == round_mode::half_even)
            k += (f > 0.5 || (f == 0.5 && (t & 1))) - (f < -0.5 || (f == -0.5 && (t & 1)));
        return static_cast<double>(k) * nd;
    }
}

// The side of a source that is a double exactly: none.
struct exact_side {
    constexpr int operator()() const noexcept { return 0; }
};

// snap_double for a v that was rounded from an exact value x: side() gives
// the sign of x − v. Snapping v rounds twice, and that can differ from
// rounding x only where v sits exactly on a rounding boundary of G (a tie for
// the nearest modes, a grid point for the directed ones): a boundary strictly
// between x and v would be a double nearer to x than v is. There x decides,
// and side() is only called there.
template <grid G, round_mode M = round_mode::nearest, bool AnySign = (G.Interval.Lower < 0), typename Side>
[[nodiscard]] constexpr double snap_double_from(double v, const Side& side) noexcept {
    if constexpr (G.Notch == rational{0})
        return v;
    else if constexpr (std::is_same_v<Side, exact_side>)
        return snap_double<G, M, AnySign>(v); // a double source rounds once
    else {
        constexpr double nd = static_cast<double>(G.Notch);
        const double     q  = v / nd;
        if (!((q < 0 ? -q : q) < 9007199254740992.0)) // 2^53
            return snap_double<G, M, AnySign>(v);
        const imax     t       = static_cast<imax>(q);
        const double   f       = q - static_cast<double>(t);
        constexpr bool nearest = M == round_mode::nearest || M == round_mode::half_even;
        if (!(nearest ? (f == 0.5 || f == -0.5) : f == 0))
            return snap_double<G, M, AnySign>(v);
        const int s = side();
        if (s == 0)
            return snap_double<G, M, AnySign>(v);
        imax k;
        if constexpr (nearest)
            k = (f < 0 ? t - 1 : t) + (s > 0); // the half point: x picks its side
        else if constexpr (M == round_mode::floor)
            k = s < 0 ? t - 1 : t;
        else if constexpr (M == round_mode::ceil)
            k = s > 0 ? t + 1 : t;
        else // toward zero
            k = (t > 0 && s < 0) ? t - 1 : (t < 0 && s > 0) ? t + 1 : t;
        return static_cast<double>(k) * nd;
    }
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

// Both endpoints lie in imax — the signed-direct candidates (and every
// `trunc(endpoint)` constant) are only meaningful then.
namespace detail {
constexpr bool fits_imax(const interval& iv) noexcept {
    return iv.Lower >= rational{std::numeric_limits<imax>::min()} &&
           iv.Upper <= rational{std::numeric_limits<imax>::max()};
}
} // namespace detail

// Smallest raw type holding every reachable index in G. Order: point →
// empty point_slot; notch-zero → rational (no integer index space); more
// than 2^64 slots → a wide_int index; signed-direct fits Lower < 0 with
// notch 1; unsigned-offset (max_index slots) otherwise.
namespace detail {
// Unsigned index raw for G's slots: a builtin up to 64 bits, else wide.
template <grid G>
using index_raw_for_t = std::conditional_t<G.max_index_representable(),
                                           smallest_uint_for_t<G.max_index()>,
                                           int_for_bits_t<G.slot_bits(), false>>;

// Signed value raw of a notch-1 grid within int64 (named only when chosen:
// its limits are truncated to 64 bits).
template <grid G, bool = (G.Interval.Lower < 0 && G.Notch == 1 && fits_imax(G.Interval))>
struct signed_direct_raw {
    using type = void;
};
template <grid G>
struct signed_direct_raw<G, true> {
    using type = smallest_int_for_t<trunc(G.Interval.Lower), trunc(G.Interval.Upper)>;
};

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
using storage_min_t = std::conditional_t<
    (G.Interval.Lower == G.Interval.Upper),
    point_slot,
    std::conditional_t<
        (G.Notch == 0),
        continuous_raw_t<G>,
        std::conditional_t<(!G.max_index_representable()),
                           index_raw_for_t<G>,
                           std::conditional_t<(G.Interval.Lower < 0 && G.Notch == 1 && fits_imax(G.Interval)),
                                              typename signed_direct_raw<G>::type,
                                              smallest_uint_for_t<G.max_index()>>>>>;

// Dyadic grid: power-of-2 notch denominator and Lower denominator, so every
// on-grid value is exactly representable in IEEE-754 `double`. Precondition
// for double-backed (`f64`) storage.
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
        constexpr int f = bit_width_of(wide_denominator(G.Notch)) - 1;
        return f <= MaxF && scaled_numerator_bits(G.Interval.Lower, f) <= Digits &&
               scaled_numerator_bits(G.Interval.Upper, f) <= Digits;
    }
}

// double: 53-bit significand, notch at least 2^-1022. Necessary
// precondition for `f64` storage.
template <grid G>
inline constexpr bool double_exact = compute_fp_exact<G, 53, 1022>();

// float: 24-bit significand, notch at least 2^-126. Necessary precondition
// for `f32` (binary32-backed) storage.
template <grid G>
inline constexpr bool float_exact = compute_fp_exact<G, 24, 126>();

// Fixed-width raw storage (policy_flag.hpp i8..u64) — pin the exact backing
// type instead of letting storage_min pick the smallest fit.
//
// has_width_flag / width_flag_count: detect "a width is pinned" and enforce
// exactly one (combining two width flags is a misuse, caught in storage_pick).
constexpr bool has_width_flag(policy_flag P) noexcept { return (P & raw_width_mask) != none; }

constexpr int width_flag_count(policy_flag P) noexcept { return std::popcount(P & raw_width_mask); }

// The type of the (lowest) set width bit, only valid when has_width_flag: the
// flags i8, u8, …, u64 are consecutive bits.
template <int I, typename T, typename... Ts>
struct nth_type : nth_type<I - 1, Ts...> {};
template <typename T, typename... Ts>
struct nth_type<0, T, Ts...> {
    using type = T;
};
template <policy_flag P>
using raw_type_of_t = typename nth_type<std::countr_zero(P& raw_width_mask) - std::countr_zero(i8),
                                        std::int8_t,
                                        std::uint8_t,
                                        std::int16_t,
                                        std::uint16_t,
                                        std::int32_t,
                                        std::uint32_t,
                                        std::int64_t,
                                        std::uint64_t>::type;

// Does raw type R hold every reachable raw value of grid G under the given
// encoding? Index storage runs 0..max_index (unsigned); value storage runs
// Lower..Upper. The full range of R is usable, matching smallest_uint_for /
// smallest_int_for.
template <grid G, typename R, bool Index>
constexpr bool storage_fits() noexcept {
    using lim = std::numeric_limits<R>;
    if constexpr (Index)
        return G.max_index_representable() && G.max_index() <= static_cast<umax>(lim::max());
    else if constexpr (std::is_unsigned_v<R>)
        return G.Interval.Lower >= 0 && G.Interval.Upper <= rational{static_cast<umax>(lim::max())};
    else
        return G.Interval.Lower >= rational{static_cast<imax>(lim::min())} &&
               G.Interval.Upper <= rational{static_cast<imax>(lim::max())};
}

// Storage for an inside<G, P>: representation flags pick the raw type, widest-wins
// (exact > f64 > f32 > {width} > direct > indexed > deduced).
//   exact   → rational raw on any grid.
//   f64     → double-backed on a dyadic or notch-0 grid; elided under
//             BEMAN_INSIDE_MATH_NO_FP (falls through to deduced).
//   f32     → float-backed when float holds the grid, else widened to double.
//   {width} → the pinned i8..u64 type, value or (with `indexed`) index storage.
//   direct  → raw == value, plain integer (Notch == 1).
//   indexed → raw == 0-based notch index (Notch != 0).
//   none    → storage_min deduction.
template <grid G, policy_flag P>
constexpr auto storage_pick() {
    // A point's value is its type: empty raw whatever the representation flag,
    // unless a width flag pins a wire layout.
    if constexpr (G.Interval.Lower == G.Interval.Upper && !has_width_flag(P))
        return point_slot{};
    else if constexpr (has_flag(P, exact))
        return detail::rational{};
#ifndef BEMAN_INSIDE_MATH_NO_FP
    else if constexpr (has_flag(P, f64) && double_exact<G>)
        return double{};
    else if constexpr (has_flag(P, f64) && dyadic_grid<G>) {
        // `f64` explicitly requested on a dyadic grid double can't represent
        // exactly (max |value·2^f| ≥ 2^53, or notch below the smallest normal).
        // Arithmetic drops the flag before reaching here, so this is direct misuse.
        static_assert(double_exact<G>,
                      "f64 storage: grid exceeds double's 53-bit significand — coarsen the "
                      "notch/range or use `exact`");
        return double{}; // unreachable; fixes the deduced return type
    } else if constexpr (has_flag(P, f32) && float_exact<G>)
        return float{};
    else if constexpr (has_flag(P, f32) && double_exact<G>)
        // `f32` requested on a grid too fine for float but representable in double:
        // WIDEN the storage to binary64. This makes a deduced f32 output (a cmath
        // result inheriting the operand's flag) whose grid overflows float store its
        // value in double rather than hard-erroring — the value stays exact. The f32
        // POLICY bit remains (harmless; storage is raw-driven via fp_raw).
        return double{};
    else if constexpr (has_flag(P, f32) && dyadic_grid<G>) {
        // Too fine for double too → genuinely unrepresentable as fp storage.
        static_assert(double_exact<G>,
                      "f32 storage: grid exceeds double's 53-bit significand — coarsen the "
                      "notch/range or use `exact`");
        return float{}; // unreachable; fixes the deduced return type
    }
#endif
    else if constexpr (has_width_flag(P)) {
        // User-pinned raw width (i8..u64). Encoding follows `indexed` (0-based
        // notch index) else value storage (raw == value, Notch == 1 like `direct`).
        // No silent widening — a type too small for the grid is a hard error.
        static_assert(width_flag_count(P) == 1, "storage: pick a single fixed-width flag (e.g. `u16`), not several");
        using R            = raw_type_of_t<P>;
        constexpr bool idx = (P & indexed) == indexed;
        // A point (notch 0) has one value: value storage holds it, index storage
        // holds slot 0 — the notch requirement does not apply.
        static_assert(G.Interval.Lower == G.Interval.Upper || (idx ? (G.Notch != 0) : (G.Notch == 1)),
                      "fixed-width storage: value storage needs Notch == 1 — add `indexed` to "
                      "store a notched grid's 0-based index instead");
        static_assert(storage_fits<G, R, idx>(),
                      "fixed-width storage: the chosen raw type is too small for this grid — "
                      "widen the flag, coarsen the grid/notch, or use `exact`");
        return R{};
    } else if constexpr ((P & direct) == direct && G.Notch == 1) {
        static_assert(G.Interval.Lower >= 0 || fits_imax(G.Interval),
                      "direct storage: a negative grid must fit int64 — drop `direct` (index storage) or use `exact`");
        return std::conditional_t<(G.Interval.Lower < 0),
                                  smallest_int_for_t<trunc(G.Interval.Lower), trunc(G.Interval.Upper)>,
                                  smallest_uint_for_t<static_cast<umax>(trunc(G.Interval.Upper))>>{};
    } else if constexpr ((P & indexed) == indexed && G.Notch != 0)
        return index_raw_for_t<G>{};
    else
        return storage_min_t<G>{};
}

template <grid G, policy_flag P>
using storage_for_t = decltype(storage_pick<G, P>());
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

constexpr bool grid_product_fits([[maybe_unused]] const grid& a, [[maybe_unused]] const grid& b) noexcept {
#if BEMAN_INSIDE_BIG_GRIDS
    return true;
#else
    const bool     ap = a.Interval.Lower == a.Interval.Upper, bp = b.Interval.Lower == b.Interval.Upper;
    const rational an = (ap && !bp) ? abs(a.Interval.Lower) : a.Notch;
    const rational bn = (bp && !ap) ? abs(b.Interval.Lower) : b.Notch;
    return try_mul(a.Interval.Lower, b.Interval.Lower) && try_mul(a.Interval.Lower, b.Interval.Upper) &&
           try_mul(a.Interval.Upper, b.Interval.Lower) && try_mul(a.Interval.Upper, b.Interval.Upper) &&
           try_mul(an, bn);
#endif
}
} // namespace detail

//---------------------------------------------------------------------------
// operator+
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<grid, errc> operator+(const grid& lhs, const grid& rhs) {
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
    // continuous (rational-backed).
    const bool                  lp = lhs.Interval.Lower == lhs.Interval.Upper;
    const bool                  rp = rhs.Interval.Lower == rhs.Interval.Upper;
    const detail::grid_rational ln = (lp && !rp) ? detail::abs(lhs.Interval.Lower) : lhs.Notch;
    const detail::grid_rational rn = (rp && !lp) ? detail::abs(rhs.Interval.Lower) : rhs.Notch;
    return detail::lift(
        [](interval i, detail::grid_rational n) { return grid{i, n}; }, lhs.Interval * rhs.Interval, ln * rn);
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
// interval hull + notch gcd. A valid grid anchors Lower on a multiple of its
// notch, so both lattices are sub-lattices of the gcd lattice — no offset
// term is needed, and the hull is a valid grid by construction. A continuous
// operand (Notch 0) makes the hull continuous. errc::overflow when the notch gcd's
// combined denominator exceeds the representable rational range.
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<grid, errc> hull(const grid& lhs, const grid& rhs) {
    const interval iv{lhs.Interval.Lower < rhs.Interval.Lower ? lhs.Interval.Lower : rhs.Interval.Lower,
                      lhs.Interval.Upper < rhs.Interval.Upper ? rhs.Interval.Upper : lhs.Interval.Upper};
    if (lhs.Notch == 0 || rhs.Notch == 0)
        return grid{iv, detail::grid_rational{0}};
    return detail::lift([iv](detail::grid_rational g) { return grid{iv, g}; }, detail::grid_gcd(lhs.Notch, rhs.Notch));
}
} // namespace beman::inside

#endif // BEMAN_INSIDE_GRID_HPP
