// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_GENERIC_HPP
#define BEMAN_INSIDE_GENERIC_HPP

#include <beman/inside/detail/debug.hpp>
#include <beman/inside/lift.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy_flag.hpp>

#include <algorithm>
#include <bit>
#include <initializer_list>

//---------------------------------------------------------------------------
// generic — type-level traits and predicates used everywhere else. Public
// grid/policy introspection (`grid_of<B>`, `policy_of<B>`, `notch64<B>`,
// `interval_of<B>`) plus the `insidable`/`numeric`/`inside_assignable` concepts; the
// storage-shape predicates and raw/value converters are internal (`beman::inside::detail`).
//---------------------------------------------------------------------------
namespace beman::inside {
template <grid G = grid{{0, 0}, 0}, policy_flag P = checked>
struct inside;

template <class>
inline constexpr bool is_inside_v = false;
template <grid G, policy_flag P>
inline constexpr bool is_inside_v<inside<G, P>> = true;

template <typename B>
concept insidable = is_inside_v<std::remove_cvref_t<B>>;

//---------------------------------------------------------------------------
// Public grid/policy introspection — extract an inside's template parameters.
// These mirror std::numeric_limits: they report what the grid is, used
// opaquely (the rational return type is never named by callers).
//---------------------------------------------------------------------------
namespace detail {
template <typename B>
struct inside_params;
template <grid G, policy_flag P>
struct inside_params<inside<G, P>> {
    static constexpr grid        grid_v   = G;
    static constexpr policy_flag policy_v = P;
};
} // namespace detail

template <insidable B>
inline constexpr grid grid_of = detail::inside_params<std::remove_cvref_t<B>>::grid_v;

template <insidable B>
inline constexpr policy_flag policy_of = detail::inside_params<std::remove_cvref_t<B>>::policy_v;

template <typename T>
inline constexpr interval interval_of = {0, 0};

template <insidable B>
inline constexpr interval interval_of<B> = grid_of<B>.Interval;

template <std::integral I>
inline constexpr interval interval_of<I> = {std::numeric_limits<I>::lowest(), std::numeric_limits<I>::max()};

template <insidable B>
inline constexpr detail::grid_rational lower_of = grid_of<B>.Interval.Lower;
template <insidable B>
inline constexpr detail::grid_rational upper_of = grid_of<B>.Interval.Upper;
template <insidable B>
inline constexpr detail::grid_rational notch_of = grid_of<B>.Notch;

namespace detail {
// The same as 64-bit rationals, for the paths that work in 64 bits (the
// integer fast paths, the math engines). A grid number past 64 bits fails
// the build here, never truncates. Under C++23 they equal lower_of & co.
template <insidable B>
inline constexpr rational lower64 = to_rational(grid_of<B>.Interval.Lower);
template <insidable B>
inline constexpr rational upper64 = to_rational(grid_of<B>.Interval.Upper);
template <insidable B>
inline constexpr rational notch64 = to_rational(grid_of<B>.Notch);
} // namespace detail

template <typename N>
concept numeric = insidable<N> or detail::arithmetic<N>;

//---------------------------------------------------------------------------
// Internal plumbing — storage shape, raw/value conversion, dispatch.
//---------------------------------------------------------------------------
namespace detail {
template <typename T>
using plain_t = std::remove_cvref_t<T>;

// Always-false but template-dependent: lets a `static_assert` inside a
// template body fire only when that template is actually instantiated
// (e.g. the guidance overloads that make `inside + 1` ill-formed).
template <typename...>
inline constexpr bool dependent_false = false;

//-------------------------------------------------------------------------
// Conversion-helper legend — the value/raw plumbing reused across the
// engine. "value space" = the number an inside denotes; "raw space" = how it
// is stored (see §2 "Storage encoding" in docs/internals.md). Use this to
// tell the similarly-named helpers apart:
//
//   as_rational(x)         value → rational    exact view of a scalar or inside
//   as_double(b)           raw   → double       kind-aware decode; lossy off dyadic grids
//   to_value(b)            raw   → imax         the inside's integer value (decodes an index)
//   from_value(b, v)       imax  → raw          store integer value v into b (inverse of to_value)
//   raw_cast<B>(x)         x     → raw_t<B>     TYPE cast only — no value arithmetic
//   raw_imax(b)            raw   → imax         widen the raw bits (NOT the value for index storage)
//   raw_from_offset<B>(o)  index → raw_t<B>     adds raw_lo (Lower for a value raw, 0 for an index)
//   raw_of_slot<B>(o)      index → raw_t<B>     any storage, any width (detail/wide_value.hpp)
//-------------------------------------------------------------------------

// Uniform rational view of a scalar or inside (rational{v} / operator rational()).
template <numeric N>
[[nodiscard]] constexpr rational as_rational(N v) {
    if constexpr (arithmetic<N>)
        return rational{v};
    else
        return v;
}

// Canonical-zero test for a divisor. rational stores zero as {0, 1}, so
// Numerator == 0 catches it regardless of representation; other types compare
// against their own zero.
template <typename T>
[[nodiscard]] constexpr bool is_canonical_zero(const T& v) {
    if constexpr (std::same_as<T, rational>)
        return v.Numerator == 0;
    else
        return v == T{0};
}

template <insidable B>
using raw_t = typename B::raw_type;

//-------------------------------------------------------------------------
// Storage — how an inside's value lives in its raw, a function of the grid
// alone (grid.hpp storage_min). The leaves partition every
// inside; each names the raw type and what it means:
//
//   value_storage — the raw IS the value
//     rational_storage                         a 64-bit rational
//     fraction_storage                         an exact_frac (continuous, limits past 64 bits)
//     integer_value_storage                    a builtin integer
//   index_storage — the raw is a 0-based slot index; value = Lower + raw·Notch
//     point_storage                            empty (point_slot): one slot, the value is the type
//     integer_index_storage                    a builtin integer
//     wide_index_storage                       a wide_int (more than 2^64 slots)
//   integer_storage = index_storage | integer_value_storage   (an integer raw)
//
// Concepts, so the groups subsume their leaves and can constrain helpers.
//-------------------------------------------------------------------------
template <typename B>
concept rational_storage = insidable<B> && std::same_as<raw_t<B>, rational>;

// A continuous grid whose limits pass 64 bits (C++26): a reduced exact
// fraction of as many limbs as the limits need.
template <typename B>
concept fraction_storage = insidable<B> && is_exact_frac_v<raw_t<B>>;

// Whether an integer raw holds the value itself (else a slot index): the
// whole-number grids where that is free (grid.hpp deduces_value).
template <insidable B>
inline constexpr bool integer_raw_holds_value = deduces_value<grid_of<B>>;

template <typename B>
concept integer_value_storage = insidable<B> && std::integral<raw_t<B>> && integer_raw_holds_value<B>;

template <typename B>
concept integer_index_storage = insidable<B> && std::integral<raw_t<B>> && !integer_raw_holds_value<B>;

// A point's raw acts as index slot 0.
template <typename B>
concept point_storage = insidable<B> && std::same_as<raw_t<B>, point_slot>;

// Its values need the exact wide paths (detail/wide_value.hpp): a 64-bit
// rational or imax cannot hold them.
template <typename B>
concept wide_index_storage = insidable<B> && is_wide_int_v<raw_t<B>>;

template <typename B>
concept value_storage = rational_storage<B> || fraction_storage<B> || integer_value_storage<B>;

template <typename B>
concept index_storage = point_storage<B> || integer_index_storage<B> || wide_index_storage<B>;

template <typename B>
concept integer_storage = index_storage<B> || integer_value_storage<B>;

// Same raw type AND same encoding (value vs index): only then does one
// inside's raw mean the same as another's. Two insides on one grid always
// share it; two grids may not ({5, 100} holds values, {5, 1000} an index).
template <insidable L, insidable R>
inline constexpr bool same_storage = std::is_same_v<raw_t<L>, raw_t<R>> && index_storage<L> == index_storage<R>;

//-------------------------------------------------------------------------
// Grid shape and magnitude.
//   notched     — Notch != 0: the values sit on a lattice (a point may still
//                 have Notch 0).
//   point_grid  — Lower == Upper: one value. Its raw is point_storage.
//   wide_valued — values past 64 bits: a wide raw, or grid numbers (a limit
//                 or the notch) past 64 bits even with few slots
//                 (wide_grid_numbers, never under C++23). They take the
//                 exact paths of detail/wide_value.hpp.
//-------------------------------------------------------------------------
template <insidable B>
inline constexpr bool notched = notch_of<B> != 0;

template <insidable B>
inline constexpr bool point_grid = lower_of<B> == upper_of<B>;

// The lattice passes through 0 (grid::anchored): every value is a whole
// number of notches, the value index J = value/Notch an integer. Unanchored
// grids ({{0.5, 10.5}, 1}) take the paths that work from Lower.
template <insidable B>
inline constexpr bool anchored = grid_of<B>.anchored();

template <insidable B>
inline constexpr bool wide_grid_numbers =
    !fits_rational(grid_of<B>.Interval.Lower) || !fits_rational(grid_of<B>.Interval.Upper) ||
    !fits_rational(grid_of<B>.Notch);

template <insidable B>
inline constexpr bool wide_valued = wide_index_storage<B> || wide_grid_numbers<B>;

// Ungated double view of any inside (the public operator double() is gated
// on a rounding flag; this is always available). Everything but index storage holds the value verbatim; an
// index decodes through the grid.
template <insidable B>
constexpr auto exact_of(const B& b); // wide_value.hpp

// Index raw → double in one IEEE operation. On an anchored grid with Notch
// p/q the value is J·p/q, J = raw + Lower/Notch. With |J·p| and q at most 2^53
// both are exact doubles, so double(J·p) / double(q) is the correctly rounded
// quotient: what the rational decode gives (a power-of-two q compiles to a
// multiply).
struct index_double_codec {
    bool   Ok;
    imax   LowerIndex; // Lower / Notch
    imax   P;          // Notch numerator
    double Q;          // Notch denominator
};
template <insidable B>
inline constexpr index_double_codec index_double = [] {
    if constexpr (!integer_index_storage<B> || !anchored<B> || wide_valued<B>)
        return index_double_codec{};
    else {
        constexpr umax k53 = umax{1} << 53;
        const rational n   = notch64<B>;
        const auto     lo = lower64<B> / n, hi = upper64<B> / n;
        if (!lo || !hi || abs_den(n.Denominator) > k53)
            return index_double_codec{};
        const umax m = lo->Numerator > hi->Numerator ? lo->Numerator : hi->Numerator; // max |J|
        if (m > k53 / n.Numerator)
            return index_double_codec{};
        return index_double_codec{
            true, signed_numerator(*lo), static_cast<imax>(n.Numerator), static_cast<double>(abs_den(n.Denominator))};
    }
}();

template <insidable B>
[[nodiscard]] constexpr double as_double(const B& b) noexcept {
    if constexpr (wide_valued<B>)
        return static_cast<double>(exact_of(b));
    else if constexpr (point_storage<B>)
        return static_cast<double>(detail::lower64<B>);
    else if constexpr (value_storage<B>)
        return static_cast<double>(b.raw());
    else if constexpr (constexpr auto c = index_double<B>; c.Ok)
        return static_cast<double>((static_cast<imax>(b.raw()) + c.LowerIndex) * c.P) / c.Q;
    else
        return static_cast<double>((*(b.raw() * detail::notch64<B>)+detail::lower64<B>).value());
}

// True when R's interval cannot contain zero — so `a / b` can return a plain
// `inside` instead of `expected<inside, errc>` (see detail/division.hpp). A point
// grid at 0 is *not* excluded.
template <insidable R>
inline constexpr bool divisor_excludes_zero = (lower_of<R> > 0) || (upper_of<R> < 0);

// Storage-agnostic int truncation of interval endpoints — intent-revealing
// `static_cast<imax>(detail::lower64<B>)`. Used by from_value, raw_lo, the fast paths.
template <insidable B>
inline constexpr imax lower_imax = trunc(detail::lower64<B>);

template <insidable B>
inline constexpr imax upper_imax = trunc(detail::upper64<B>);

// Slot count via grid::max_index (overflow-safe: 0 when it doesn't fit umax,
// for grids with a wide index, which take the exact wide paths).
template <insidable B>
inline constexpr umax max_index_v = grid_of<B>.max_index();

// Every value — and, for index storage, every slot — fits imax. Gates the
// integer fast paths that work in imax (raw_imax / to_value / raw_lo /
// raw_hi); a grid reaching past int64 (e.g. {0, 2^64−1} in a uint64) takes
// the exact rational / umax paths instead.
template <insidable B>
inline constexpr bool values_fit_imax =
    !wide_valued<B> && fits_imax(interval_of<B>) &&
    (value_storage<B> || max_index_v<B> <= static_cast<umax>(std::numeric_limits<imax>::max()));

// Notch a non-zero integer and Lower an integer — every value is an
// integer ({{0.5, 10.5}, 1} has an integer notch but not integer values).
// Gates the implicit imax/size_t conversions.
template <grid G>
inline constexpr bool integer_notch =
    wide_denominator(G.Notch) == grid_wide{1} && G.Notch != 0 && wide_denominator(G.Interval.Lower) == grid_wide{1};

// ONLY type conversion, NO value representation conversion calculation
template <insidable B>
[[nodiscard]] constexpr raw_t<B> raw_cast(auto value) noexcept {
    return static_cast<raw_t<B>>(value);
}

template <insidable B>
[[nodiscard]] constexpr raw_t<B> raw_cast(rational value) noexcept {
    if constexpr (rational_storage<B>)
        return value;
    else
        return value.to<raw_t<B>>().value_or(0);
}

// Widen raw storage to imax. Distinct from `to_value(b)` for notch-stored
// grids where raw is an index rather than a value — naming separates the
// two intents that today both spell `static_cast<imax>`.
template <insidable B>
    requires(!wide_index_storage<B>) // a wide raw does not fit imax: use the exact wide path
constexpr imax raw_imax(B b) noexcept {
    return static_cast<imax>(b.raw());
}

//-------------------------------------------------------------------------
// Q-format integer fast path: for grids with integer Lower, unit-numerator
// Notch, and raw fitting imax, value↔raw is pure integer arithmetic. Shared
// by operator rational(), from_value, and assignment::store.
//-------------------------------------------------------------------------
template <insidable B>
inline constexpr bool qformat_codec_fits =
    abs_den(detail::lower64<B>.Denominator) == 1 && detail::notch64<B>.Numerator == 1 &&
    values_fit_imax<B>; // Lower·nd and the raw both in imax

// value → raw, integer math only.
template <insidable B>
    requires qformat_codec_fits<B>
constexpr raw_t<B> q_format_encode(imax value) noexcept {
    constexpr imax nd = abs_den(detail::notch64<B>.Denominator);
    return raw_cast<B>((value - lower_imax<B>)*nd);
}

// raw → rational, integer math only.
template <insidable B>
    requires qformat_codec_fits<B>
constexpr rational q_format_decode(B b) noexcept {
    constexpr imax nd = abs_den(detail::notch64<B>.Denominator);
    const imax     n  = raw_imax(b) + lower_imax<B> * nd;
    if constexpr (std::has_single_bit(static_cast<umax>(nd))) {
        // A power-of-two denominator reduces by the common trailing zeros
        // (0 reduces to 0/1).
        const umax m = n < 0 ? umax{0} - static_cast<umax>(n) : static_cast<umax>(n);
        const int  s = std::countr_zero(m | static_cast<umax>(nd));
        rational   r;
        r.Numerator   = m >> s;
        r.Denominator = n < 0 ? -(nd >> s) : nd >> s;
        return r;
    } else
        return rational{n, nd};
}

// Library-internal extraction helper. Always succeeds (returns `imax`
// unconditionally) but does not check the value fits in any narrower
// target. User code should prefer `b.to<T>()`, which carries a typed
// overflow error.
template <insidable B>
[[nodiscard]] constexpr imax to_value(B b) noexcept {
    if constexpr (value_storage<B>)
        return raw_imax(b);
    else if constexpr (abs_den(detail::notch64<B>.Denominator) == 1 && abs_den(detail::lower64<B>.Denominator) == 1)
        return lower_imax<B> + raw_imax(b) * static_cast<imax>(detail::notch64<B>.Numerator);
    else if constexpr (qformat_codec_fits<B>) {
        constexpr imax nd = abs_den(detail::notch64<B>.Denominator);
        return (raw_imax(b) + lower_imax<B> * nd) / nd; // q_format_decode, truncated
    } else                                              // index storage, generic rational path
        return trunc(as_rational(b));
}

template <insidable B>
constexpr void from_value(B& b, imax val) {
    if constexpr (value_storage<B>)
        b = B::from_raw(raw_cast<B>(val));
    else if constexpr (abs_den(detail::notch64<B>.Denominator) == 1 && abs_den(detail::lower64<B>.Denominator) == 1)
        b = B::from_raw(raw_cast<B>((val - lower_imax<B>) / static_cast<imax>(detail::notch64<B>.Numerator)));
    else if constexpr (qformat_codec_fits<B>)
        b = B::from_raw(q_format_encode<B>(val));
    else // index storage, generic rational path
    {
        auto offset = (rational{val} - detail::lower64<B>) / detail::notch64<B>;
        b           = B::from_raw(raw_cast<B>(offset.value().Numerator));
    }
}

//-------------------------------------------------------------------------
// raw_lo / raw_hi / raw_from_offset — an integer raw's endpoints. An index
// raw is 0-based (raw_lo == 0); a value raw IS the value (raw_lo == Lower),
// so an offset needs raw_lo<L> added back before storing.
//-------------------------------------------------------------------------
template <insidable B>
inline constexpr imax raw_lo = integer_value_storage<B> ? lower_imax<B> : 0;

template <insidable B>
inline constexpr imax raw_hi = integer_value_storage<B> ? upper_imax<B> : static_cast<imax>(max_index_v<B>);

// The exact raw range: 0 .. slot count for index storage, Lower .. Upper
// for value storage (integers there). Sizes the work types below.
template <insidable B>
inline constexpr grid_wide raw_lo_exact = index_storage<B> ? grid_wide{0} : wide_numerator(lower_of<B>);
template <insidable B>
inline constexpr grid_wide raw_hi_exact = index_storage<B> ? grid_of<B>.slot_count() : wide_numerator(upper_of<B>);

// Value bits a signed integer needs to hold every value in [lo, hi].
constexpr int signed_value_bits(const grid_wide& lo, const grid_wide& hi) noexcept {
    auto      mag = [](const grid_wide& v) { return bit_width_of(v.negative() ? -(v + grid_wide{1}) : v); };
    const int a = mag(lo), b = mag(hi);
    return a > b ? a : b;
}

// Value bits for every value in a list (their min .. max).
constexpr int signed_value_bits_of(std::initializer_list<grid_wide> vals) noexcept {
    grid_wide mn = *vals.begin(), mx = *vals.begin();
    for (const grid_wide& v : vals) {
        if (v < mn)
            mn = v;
        if (mx < v)
            mx = v;
    }
    return signed_value_bits(mn, mx);
}

// Signed work type for an exact intermediate of Bits value bits: imax for
// everything within int64 (the builtin fast paths keep their codegen),
// a wide_int beyond.
template <int Bits>
using work_int_t = int_for_bits_t<(Bits < 63 ? 63 : Bits), true>;

// t = Quot·m + Rem with 0 ≤ Rem < m (m > 0): division rounded toward −∞,
// the fold of every wrap. A wide int divides once for both parts.
template <typename W>
struct floored {
    W Quot, Rem;
};
template <typename W>
[[nodiscard]] constexpr floored<W> floor_divmod(const W& t, const W& m) noexcept {
    floored<W> f;
    if constexpr (is_wide_int_v<W>) {
        auto [q, r] = W::divmod(t, m);
        f           = {q, r};
    } else
        f = {t / m, t % m};
    if (f.Rem < W{0}) {
        f.Rem += m;
        f.Quot -= W{1};
    }
    return f;
}

// Offset (umax or imax) → raw. Adds in umax: the bits are the same, but a
// value raw of a grid reaching past int64 (offset + Lower ≥ 2^63) must not
// overflow imax.
template <insidable L, std::integral W>
constexpr raw_t<L> raw_from_offset(W offset) noexcept {
    return raw_cast<L>(static_cast<umax>(offset) + static_cast<umax>(raw_lo<L>)); // raw_lo: 0 for an index raw
}

// The raw of L's Lower (low) or Upper endpoint: the exact constant for a
// rational raw, slot 0 or max_index_v for an integer one.
template <insidable L>
constexpr raw_t<L> endpoint_raw(bool low) noexcept {
    if constexpr (rational_storage<L>)
        return low ? detail::lower64<L> : detail::upper64<L>;
    else
        return raw_from_offset<L>(low ? umax{0} : max_index_v<L>);
}

// The raw of a value v on L's lattice within [Lower, Upper], through its
// exact offset (v − Lower)/Notch.
template <insidable L>
constexpr raw_t<L> raw_of_lattice_value(rational v) {
    return raw_from_offset<L>(((v - detail::lower64<L>).value() / detail::notch64<L>).value().Numerator);
}

//-------------------------------------------------------------------------
// integer_limits vs integer_lattice — easy to confuse, both needed.
//   integer_limits<B>: Lower and Upper integer (Notch may be fractional,
//     e.g. inside<{0,100}, 1/10>). Lets Lower/Upper be used as imax constants.
//   integer_lattice<B>: every value an integer — an integer Lower and an
//     integer non-zero Notch, or a point ⇒ integer_limits (not the converse).
//     A continuous grid (Notch 0) is not one, whatever its limits.
//-------------------------------------------------------------------------
template <insidable B>
inline constexpr bool integer_limits =
    wide_denominator(lower_of<B>) == grid_wide{1} && wide_denominator(upper_of<B>) == grid_wide{1};

template <insidable B>
inline constexpr bool integer_lattice =
    integer_notch<grid_of<B>> || (lower_of<B> == upper_of<B> && wide_denominator(lower_of<B>) == grid_wide{1});

// Q-format: the canonical fixed-point shape (Q8.8, Q16.16, ...). Notch has
// unit numerator with integer denominator > 1, Lower is an integer at 0.
// Value = Raw / Notch.Denominator. Used to gate the integer fast path for
// fixed-point division, which would otherwise fall into the slow rational
// route because Notch.Denominator > 1 disqualifies integer_lattice.
template <insidable B>
inline constexpr bool qformat_grid =
    wide_numerator(notch_of<B>) == grid_wide{1} && wide_denominator(notch_of<B>) > grid_wide{1} && lower_of<B> == 0;

// Policy test: checks both type-level and per-operation policy.
// Composite flags (e.g. round_nearest = bit5 | snap) require all
// their bits set — having a subset like just `snap` does NOT match.
template <insidable B, typename P, policy_flag F>
inline constexpr bool has_policy = has_flag(policy_of<B>, F) || plain_t<P>::test(F);

// rounding_of (policy_flag.hpp) over L's type policy and the call's policy P.
template <insidable L, typename P>
inline constexpr round_mode rounding_for = has_policy<L, P, round_floor>       ? round_mode::floor
                                           : has_policy<L, P, round_ceil>      ? round_mode::ceil
                                           : has_policy<L, P, round_half_even> ? round_mode::half_even
                                           : has_policy<L, P, round_nearest>   ? round_mode::nearest
                                                                               : round_mode::trunc;

// Whether the lattice index of Lower, ⌊Lower/Notch⌋ (= Lower/Notch when
// anchored), is odd: with an offset's parity it says which lattice points are
// even. Exact past imax.
template <insidable L>
inline constexpr bool lower_index_odd =
    detail::notched<L> &&
    (round_to_integral((detail::lower64<L> / detail::notch64<L>).value(), round_mode::floor).Numerator & 1) != 0;

// The rounding step onto L's lattice by rounding_for<L, P>: the exact
// lattice offset q (v/Notch when anchored, (v − Lower)/Notch otherwise) as
// a base index and whether to step one unit up from it. `negative`: v < 0.
// A rational index never wraps, however far past imax it lies.
struct lattice_step {
    rational Base;
    bool     Step;
};
template <insidable L, typename P>
[[nodiscard]] constexpr lattice_step lattice_step_of(rational q, bool negative) {
    constexpr round_mode m = rounding_for<L, P>;
    if constexpr (anchored<L>)
        return {round_to_integral(q, m), false}; // the value index's own sign rules
    else {
        const rational k = round_to_integral(q, round_mode::floor);
        const rational f = (q - k).value(); // in [0, 1): no overflow
        return {k,
                rounds_up(m,
                          negative,
                          classify_remainder(m, f.Numerator, abs_den(f.Denominator)),
                          ((k.Numerator & 1) != 0) != lower_index_odd<L>)};
    }
}

// v rounded onto L's lattice {Lower + k·Notch} by rounding_for<L, P> (value
// space, ties half away from zero). Pre: the result fits the rational range
// (a v within one notch of L's limits); try_round_to_lattice checks it.
template <insidable L, typename P>
[[nodiscard]] constexpr rational round_to_lattice(rational v) {
    if constexpr (!detail::notched<L>)
        return v;
    else if constexpr (anchored<L>) {
        return (lattice_step_of<L, P>((v / detail::notch64<L>).value(), v < 0).Base * detail::notch64<L>).value();
    } else {
        const auto [k, up] =
            lattice_step_of<L, P>(((v - detail::lower64<L>).value() / detail::notch64<L>).value(), v < 0);
        const rational j = up ? (k + rational{1}).value() : k;
        return (detail::lower64<L> + (j * detail::notch64<L>).value()).value();
    }
}

// The same for any v — not limited to [Lower, Upper], so wrap can round
// first and fold an on-lattice value after: a result past the rational
// range is errc::overflow. (Separate from round_to_lattice: an expected
// result on that hot store path cost instructions.)
template <insidable L, typename P>
[[nodiscard]] constexpr std::expected<rational, errc> try_round_to_lattice(rational v) {
    if constexpr (!detail::notched<L>)
        return v;
    else {
        const auto q =
            anchored<L> ? try_div(v, detail::notch64<L>) : try_sub(v, detail::lower64<L>).and_then([](rational off) {
                return try_div(off, detail::notch64<L>);
            });
        if (!q)
            return q;
        const auto [base, step] = lattice_step_of<L, P>(*q, v < 0);
        const auto j            = step ? try_add(base, rational{1}) : std::expected<rational, errc>{base};
        if (!j)
            return j;
        const auto offset = try_mul(*j, detail::notch64<L>);
        return (offset && !anchored<L>) ? try_add(detail::lower64<L>, *offset) : offset;
    }
}

// Round, then range-check. Lower and Upper are lattice points, so rounding
// an in-range value keeps it in range; only an out-of-range value can change
// outcome. When the policy may round (snap), rounds_into_range rounds such a
// value and reports whether it lands inside the interval (`out` = the
// rounded value). Only values within one notch of the interval can, which
// also keeps round_to_lattice's division bounded for huge sources.
template <insidable L, typename P>
inline constexpr bool rounds_before_range_check = detail::notched<L> && has_policy<L, P, snap>;

template <insidable L, typename P>
[[nodiscard]] constexpr bool rounds_into_range(rational v, rational& out) {
    if constexpr (!rounds_before_range_check<L, P>)
        return false;
    else {
        constexpr rational lo = (detail::lower64<L> - detail::notch64<L>).value_or(detail::lower64<L>);
        constexpr rational hi = (detail::upper64<L> + detail::notch64<L>).value_or(detail::upper64<L>);
        if (v <= lo || v >= hi)
            return false;
        out = round_to_lattice<L, P>(v); // within one notch of the limits: fits
        return includes(interval_of<L>, out);
    }
}

// Store-side form for the assignment paths: when v rounds inside, the raw of
// the rounded lattice point (an exact in-range point: an index or rational
// raw, no further rounding). Cold and out of line, and it returns the
// raw in registers instead of writing through the caller's inside:
//   - a second call site of the large store functions stops GCC inlining
//     them into the hot path (~40 instructions per in-range store);
//   - an escaping `lhs` address turns on the stack protector there (~3).
template <insidable L>
struct rounded_raw {
    raw_t<L> Raw;
    bool     Ok;
};

template <insidable L, typename P>
[[gnu::cold, gnu::noinline]] constexpr rounded_raw<L> raw_if_rounds_inside(rational v) {
    rational r;
    if (!rounds_into_range<L, P>(v, r))
        return {raw_t<L>{}, false};
    if constexpr (rational_storage<L>)
        return {r, true};
    else
        return {raw_of_lattice_value<L>(r), true};
}

// Rounds the split offset quotient q + r/den (r < den ≤ imax_max) per L's
// rounding policy — q/r form so no expression can overflow umax
// (num + den/2 could, for num near umax). round_quotient's offset rule.
template <insidable L, typename P>
[[nodiscard]] constexpr umax round_offset(umax q, umax r, umax den) noexcept {
    constexpr round_mode m = rounding_for<L, P>;
    return q + rounds_away(m, false, classify_remainder(m, r, den), (q & 1) != 0);
}

// The offset quotient num/den (den ≥ 1) of an unanchored grid rounded in
// value space: the value Lower + (num/den)·Notch is below zero exactly when
// num/den < −Lower/Notch, compared without overflow in 128 bits.
template <insidable L, typename P>
[[nodiscard]] constexpr umax round_offset_unanchored(umax num, umax den) noexcept {
    const umax           q = num / den, r = num % den;
    constexpr round_mode m        = rounding_for<L, P>;
    bool                 negative = false;
    if constexpr (detail::lower64<L> < 0) {
        constexpr rational c = (-detail::lower64<L> / detail::notch64<L>).value(); // > 0
        using W              = wide_uint<2>;
        negative             = W{num} * W{abs_den(c.Denominator)} < W{c.Numerator} * W{den};
    }
    return q + rounds_up(m, negative, classify_remainder(m, r, den), ((q & 1) != 0) != lower_index_odd<L>);
}

// Round the non-negative offset quotient num/den (den >= 1) to an integer
// notch index per L's rounding policy.
//
// Tie/sign rules are in VALUE space, not offset space, so assigning a value
// rounds it the same way dividing down to it does (detail::div_rounded is the
// reference). The offset num/den is >= 0 (sign lost by subtracting Lower), so
// we rebuild the signed value-index NUM = m·den + num (m = Lower/Notch), round
// it like div_rounded, and return the offset J - m. m is integral on every
// anchored grid; an unanchored one rounds the offset against the value's sign.
template <insidable L, typename P>
[[nodiscard]] constexpr umax round_quotient(umax num, umax den) noexcept {
    constexpr rational zl =
        (!detail::notched<L>) ? rational{0} : (detail::lower64<L> / detail::notch64<L>).value_or(rational{0});
    constexpr bool vidx = (zl.Denominator == 1 || zl.Denominator == -1);
    constexpr imax m    = vidx ? signed_numerator(zl) : imax{0};

    if constexpr (!anchored<L>)
        return round_offset_unanchored<L, P>(num, den);
    else if constexpr (!vidx)
        return round_offset<L, P>(num / den, num % den, den);
    else {
        // Round the signed value-index NUM/di exactly like detail::div_rounded.
        // A numerator or m·di beyond imax (fine-denominator sources on grids
        // with large |Lower·count|) cannot rebuild the signed index: round the
        // offset's floor q instead, in value space — the value is below zero
        // exactly when the floor's value index m + q is.
        const imax di = static_cast<imax>(den);
        imax       mdi, NUM;
        if (num > static_cast<umax>(std::numeric_limits<imax>::max()) || mul_overflow(m, di, &mdi) ||
            add_overflow(mdi, static_cast<imax>(num), &NUM)) [[unlikely]] {
            constexpr round_mode mode = rounding_for<L, P>;
            const umax           q = num / den, r = num % den;
            const bool           negative = m < 0 && q < umax{0} - static_cast<umax>(m);
            return q +
                   rounds_up(mode, negative, classify_remainder(mode, r, den), ((q ^ static_cast<umax>(m)) & 1) != 0);
        }
        const imax           t    = NUM / di; // C++ truncation toward zero
        const imax           rr   = NUM % di; // sign of NUM, |rr| < di
        const bool           neg  = NUM < 0;
        const umax           ar   = (rr < 0) ? ~static_cast<umax>(rr) + 1u : static_cast<umax>(rr);
        constexpr round_mode mode = rounding_for<L, P>;
        const imax J = rounds_away(mode, neg, classify_remainder(mode, ar, static_cast<umax>(di)), (t & 1) != 0)
                           ? (neg ? t - 1 : t + 1)
                           : t;
        return static_cast<umax>(J - m); // offset index k = J - m (>= 0)
    }
}

// Forward decl — defined in assignment.hpp
template <typename L, typename R>
struct assignment;

// A single-point source (Lower == Upper) carries one value, so its notch
// question is only whether that value lies on L's lattice — admitting
// `3_ins` into `{{0,9},3}` while rejecting `1_ins`. (Range is the interval
// check's job, so clamp/wrap still take an out-of-range point.) The raw
// Factor says nothing here: a point's notch is 0.
template <typename L, typename R>
inline constexpr bool point_on_lattice = grid_same_lattice(lower_of<R>, lower_of<L>, notch_of<L>);

// The notch half of inside_assignable for an insidable R.
template <typename L, typename R>
inline constexpr bool notches_compatible = [] {
    if constexpr (point_grid<R>)
        return point_on_lattice<L, R>;
    else if constexpr (!notched<L> || !notched<R>)
        // A continuous target holds every value; a continuous source is
        // checked when stored, like a rational scalar.
        return true;
    else if constexpr (wide_valued<L> || wide_valued<R>)
        // Every R value on L's lattice: R's notch a multiple of L's, and R's
        // lattice anchored on L's.
        return grid_divides_evenly(notch_of<R>, notch_of<L>) &&
               grid_same_lattice(lower_of<R>, lower_of<L>, notch_of<L>);
    else
        // R's notch a whole number of L's, and R's lattice on L's (a given
        // when both pass through 0).
        return abs_den(assignment<L, R>::Factor.Denominator) == 1 &&
               grid_same_lattice(lower_of<R>, lower_of<L>, notch_of<L>);
}();

// Tail of the policy cascade: checked reports.
// Returns true if a policy handled the failure (caller should return).
// Cheap default — reports through the static category message (no string).
template <typename P>
constexpr bool range_fail(P&& policy) {
    if (policy.range_check()) {
        policy.report(errc::overflow);
        return true;
    }
    return false;
}

// The two non-trivial clauses of `inside_assignable`, named so the concept and
// its `inside_assignable_why` diagnostic share one definition. Concepts (not
// bools) so `||` short-circuits *instantiation* (e.g. assignment<L,R>::Factor
// is never formed when R isn't insidable).
template <typename L, typename R, policy_flag P = checked>
concept assign_intervals_ok =
    (!insidable<R> && !std::integral<R>)
    // wrap/clamp bring any value into range, so a disjoint rhs interval is fine
    // for them (the integral-rhs path already allows it — int's interval is unbounded).
    || ((policy_of<L> | P) & (wrap | clamp)) != 0 || not excludes(interval_of<L>, interval_of<R>);

template <typename L, typename R, policy_flag P>
concept assign_notch_ok = !insidable<R> || ((policy_of<L> | P) & snap) != 0 || notches_compatible<L, R>;
} // namespace detail

// Compile-time prerequisites for L = R, gating three failure modes at the call
// site: (1) R is numeric; (2) intervals overlap (typed-interval R only —
// skipped for float/rational, which have no static interval); (3) integer
// notch ratio or snap set (else R's notch doesn't divide L's; opt into
// rounding). Named `inside_assignable` to avoid shadowing std::assignable_from.
template <typename L, typename R, policy_flag P = checked>
concept inside_assignable = numeric<R> && detail::assign_intervals_ok<L, R, P> && detail::assign_notch_ok<L, R, P>;

// Diagnostic helper: instantiating `inside_assignable_why<L,R,P>` fires a named
// static_assert per failed clause, so a developer can see which tripped. Backs
// both the default-build diagnostic fallbacks in `inside` (core.hpp, gated by
// `BEMAN_INSIDE_STRICT_SFINAE`) and the public `why_assignable` probe below.
template <typename L, typename R, policy_flag P = checked>
struct inside_assignable_why {
    // Collapse each clause to a plain bool *before* the static_assert. Asserting on
    // a concept-id makes GCC dump the whole satisfaction tree ("constraints not
    // satisfied / no operand of the disjunction…") on top of the message; a bool
    // condition prints just the message. Each clause is self-guarding (the inner
    // disjunctions gate `assignment<L,R>::Factor` on `insidable<R>`), so evaluating
    // all three unconditionally is safe even when R is not numeric.
    static constexpr bool is_numeric   = numeric<R>;
    static constexpr bool intervals_ok = detail::assign_intervals_ok<L, R, P>;
    static constexpr bool notch_ok     = detail::assign_notch_ok<L, R, P>;
    static_assert(is_numeric, "inside_assignable: rhs is not numeric (must be an inside or arithmetic type)");
    static_assert(intervals_ok,
                  "inside_assignable: rhs interval lies entirely outside lhs interval and the policy "
                  "(not wrap/clamp) cannot bring it into range — assignment can never succeed");
    static_assert(notch_ok,
                  "inside_assignable: incompatible notches — use `with_snap()` or `policy<snap>()` to allow rounding");
    static constexpr bool value = inside_assignable<L, R, P>;
};

// Public manual probe: `static_assert(beman::inside::why_assignable<DstInside, decltype(src)>);`
// emits the named per-clause reasons in any build — including a strict
// (`BEMAN_INSIDE_STRICT_SFINAE`) build where the automatic in-`inside` fallbacks are absent.
template <typename Dst, typename Src, policy_flag P = policy_of<Dst>>
inline constexpr bool why_assignable = inside_assignable_why<Dst, std::remove_cvref_t<Src>, P>::value;
} // namespace beman::inside

#endif // BEMAN_INSIDE_GENERIC_HPP
