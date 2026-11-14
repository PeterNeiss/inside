// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_DETAIL_STORAGE_HPP
#define BEMAN_INSIDE_INSIDE_DETAIL_STORAGE_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/detail/introspection.hpp>

#include <algorithm>
#include <bit>
#include <initializer_list>
#include <utility>

//---------------------------------------------------------------------------
// storage — how an inside's value lives in its raw (the storage concepts),
// the raw ↔ value plumbing, the grid shape predicates, and the integer
// rounding of quotients the stores share.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
//-------------------------------------------------------------------------
// The storage is a function of the grid alone (grid.hpp raw_for). The
// leaves partition every inside; each names the raw type and what it means:
//
//   value_storage — the raw IS the value
//     rational_storage                         a 64-bit rational
//     fraction_storage                         a wide_rational (continuous, limits past 64 bits)
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

// A continuous grid whose limits pass 64 bits: a reduced exact
// fraction of as many limbs as the limits need.
template <typename B>
concept fraction_storage = insidable<B> && wide_rational_like<raw_t<B>>;

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

// Its values need the exact wide paths (inside/detail/wide_value.hpp): a 64-bit
// rational or int64 cannot hold them.
template <typename B>
concept wide_index_storage = insidable<B> && wide_integer<raw_t<B>>;

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
//                 (wide_grid_numbers). They take the
//                 exact paths of inside/detail/wide_value.hpp.
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

// A lattice whose values pass the 64-bit rational between limits that fit:
// on {{-1.2e19, 1.2e19}, 1.5} the half-integers need numerators to 2.4e19.
template <insidable B>
inline constexpr bool lattice_past_rational =
    grid_of<B>.Notch != 0 && (grid_int{std::numeric_limits<std::int64_t>::max()} < den_bound(grid_of<B>) ||
                              grid_int{std::numeric_limits<std::uint64_t>::max()} < num_bound(grid_of<B>));

// Values past the 64-bit rational: a wide index, grid numbers past 64 bits,
// a lattice past it, or a fraction raw (a bounded continuous grid can need
// one).
template <insidable B>
inline constexpr bool wide_valued =
    wide_index_storage<B> || wide_grid_numbers<B> || lattice_past_rational<B> || fraction_storage<B>;

// Whether an inside converts to the 64-bit rational: a lattice past it still
// does, for its values that fit (the conversion reports the others).
template <insidable B>
inline constexpr bool rational_readable = !wide_valued<B> || (lattice_past_rational<B> && !wide_index_storage<B> &&
                                                              !wide_grid_numbers<B> && !fraction_storage<B>);

template <insidable B>
constexpr auto exact_of(const B& b); // wide_value.hpp

// The value indices Lower/Notch and Upper/Notch of a notched grid (0 on a
// continuous one), exactly, and as 64-bit rationals (for the 64-bit paths)
// where both fit.
template <insidable B>
inline constexpr grid_rational lower_index_of = notched<B> ? lower_of<B> / notch_of<B> : grid_rational{0};
template <insidable B>
inline constexpr grid_rational upper_index_of = notched<B> ? upper_of<B> / notch_of<B> : grid_rational{0};
template <insidable B>
inline constexpr bool value_indices_fit64 =
    notched<B> && fits_rational(lower_index_of<B>) && fits_rational(upper_index_of<B>);
template <insidable B>
inline constexpr rational lower_index64 = to_rational(lower_index_of<B>);
// Lower/Notch as an int64 value index: an integer within int64.
template <insidable B>
inline constexpr bool lower_index_fits_int64 =
    notched<B> && grid_denominator(lower_index_of<B>) == grid_int{1} &&
    !(lower_index_of<B> < grid_rational{std::numeric_limits<std::int64_t>::min()}) &&
    !(grid_rational{std::numeric_limits<std::int64_t>::max()} < lower_index_of<B>);
template <insidable B>
inline constexpr rational upper_index64 = to_rational(upper_index_of<B>);
// Every value index of an anchored grid (value/Notch, an integer) fits
// int64: value_index<std::int64_t> is exact. (uint64's do not.)
template <insidable B>
inline constexpr bool value_indices_fit_int64 =
    notched<B> && anchored<B> && !(lower_index_of<B> < grid_rational{std::numeric_limits<std::int64_t>::min()}) &&
    !(grid_rational{std::numeric_limits<std::int64_t>::max()} < upper_index_of<B>);

// Index raw → double in one IEEE operation. On an anchored grid with Notch
// p/q the value is J·p/q, J = raw + Lower/Notch. With |J·p| and q at most 2^53
// both are exact doubles, so double(J·p) / double(q) is the correctly rounded
// quotient: what the rational decode gives (a power-of-two q compiles to a
// multiply).
struct index_double_codec {
    bool         Ok;
    std::int64_t LowerIndex; // Lower / Notch
    std::int64_t P;          // Notch numerator
    double       Q;          // Notch denominator
};
template <insidable B>
inline constexpr index_double_codec index_double = [] {
    if constexpr (!integer_index_storage<B> || !anchored<B> || wide_valued<B> || !value_indices_fit64<B>)
        return index_double_codec{};
    else {
        constexpr std::uint64_t k53 = std::uint64_t{1} << 53;
        const rational          n   = notch64<B>;
        const auto              lo = lower_index64<B>, hi = upper_index64<B>;
        if (abs_den(n.Den) > k53)
            return index_double_codec{};
        const std::uint64_t m = lo.Num > hi.Num ? lo.Num : hi.Num; // max |J|
        if (m > k53 / n.Num)
            return index_double_codec{};
        return index_double_codec{
            true, signed_numerator(lo), static_cast<std::int64_t>(n.Num), static_cast<double>(abs_den(n.Den))};
    }
}();

// Ungated double view of any inside (the public operator double() is gated
// on a rounding flag; this is always available). Everything but index
// storage holds the value verbatim; an index decodes through the grid.
template <insidable B>
[[nodiscard]] constexpr double as_double(const B& b) noexcept {
    if constexpr (wide_valued<B>)
        return static_cast<double>(exact_of(b));
    else if constexpr (point_storage<B>)
        return static_cast<double>(lower64<B>);
    else if constexpr (value_storage<B>)
        return static_cast<double>(b.raw());
    else if constexpr (constexpr auto c = index_double<B>; c.Ok)
        return static_cast<double>((static_cast<std::int64_t>(b.raw()) + c.LowerIndex) * c.P) / c.Q;
    else
        return static_cast<double>(as_rational(b)); // exact, even where Raw·Notch passes 64 bits
}

// True when R's interval cannot contain zero — so `a / b` can return a plain
// `inside` instead of `expected<inside, errc>` (see inside/detail/division.hpp). A point
// grid at 0 is *not* excluded.
template <insidable R>
inline constexpr bool divisor_excludes_zero = (lower_of<R> > 0) || (upper_of<R> < 0);

// The interval's endpoints truncated toward zero, modulo 2^64 (exact on the
// grids within int64 that read them). Used by from_value, raw_lo, the fast
// paths.
template <insidable B>
inline constexpr std::int64_t lower_int64 = static_cast<std::int64_t>(lower64<B>);

template <insidable B>
inline constexpr std::int64_t upper_int64 = static_cast<std::int64_t>(upper64<B>);

// The largest index, detail::max_index (overflow-safe: 0 when it doesn't fit uint64,
// for grids with a wide index, which take the exact wide paths).
template <insidable B>
inline constexpr std::uint64_t max_index_of = max_index(grid_of<B>);

// Every value — and, for index storage, every slot — fits int64. Gates the
// integer fast paths that work in int64 (raw_int64 / to_value / raw_lo /
// raw_hi); a grid reaching past int64 (e.g. {0, 2^64−1} in a uint64) takes
// the exact rational / uint64 paths instead.
template <insidable B>
inline constexpr bool values_fit_int64 = !wide_valued<B> && fits_int64(interval_of<B>) &&
                                         (value_storage<B> || std::in_range<std::int64_t>(max_index_of<B>));

// Every value's reduced numerator and denominator fit int64 too: a value
// within int64 can still have a numerator past it ((2^63 + 1)/2).
template <insidable B>
inline constexpr bool fraction_fits_int64 = [] {
    if constexpr (!values_fit_int64<B>)
        return false;
    else
        return !(grid_int{std::numeric_limits<std::int64_t>::max()} < num_bound(grid_of<B>));
}();

// Notch a non-zero integer and Lower an integer — every value is an
// integer ({{0.5, 10.5}, 1} has an integer notch but not integer values).
template <grid G>
inline constexpr bool integer_notch =
    grid_denominator(G.Notch) == grid_int{1} && G.Notch != 0 && grid_denominator(G.Interval.Lower) == grid_int{1};

// An index grid: every value an integer from 0 up, within int64.
template <grid G>
inline constexpr bool index_shaped = (integer_notch<G> || (G.Interval.Lower == G.Interval.Upper &&
                                                           grid_denominator(G.Interval.Lower) == grid_int{1})) &&
                                     !(G.Interval.Lower < 0) && fits_int64(G.Interval);

// Every value an integer: an integer lattice or an integer point (see
// integer_limits below).
template <insidable B>
inline constexpr bool integer_lattice =
    integer_notch<grid_of<B>> || (lower_of<B> == upper_of<B> && grid_denominator(lower_of<B>) == grid_int{1});

// ONLY type conversion, NO value representation conversion calculation
template <insidable B>
[[nodiscard]] constexpr raw_t<B> raw_cast(auto value) noexcept {
    return static_cast<raw_t<B>>(value);
}

// Widen raw storage to int64. Distinct from `to_value(b)` for notch-stored
// grids where raw is an index rather than a value; the names keep the two
// intents apart although both are a `static_cast<std::int64_t>`.
template <insidable B>
    requires(!wide_index_storage<B>) // a wide raw does not fit int64: use the exact wide path
constexpr std::int64_t raw_int64(B b) noexcept {
    return static_cast<std::int64_t>(b.raw());
}

//-------------------------------------------------------------------------
// Q-format integer fast path: for grids with integer Lower, unit-numerator
// Notch 1/nd, and every value·nd within int64, value↔raw is pure integer
// arithmetic. Shared
// by operator rational(), from_value, and assignment::store.
//-------------------------------------------------------------------------
template <insidable B>
inline constexpr bool q_format_fits = [] {
    if constexpr (abs_den(lower64<B>.Den) != 1 || notch64<B>.Num != 1 || !values_fit_int64<B>)
        return false;
    else { // every value·nd (so Lower·nd and raw + Lower·nd) in int64
        constexpr grid_rational nd{grid_denominator(notch_of<B>)};
        return !(lower_of<B> * nd < grid_rational{std::numeric_limits<std::int64_t>::min()}) &&
               !(grid_rational{std::numeric_limits<std::int64_t>::max()} < upper_of<B> * nd);
    }
}();

// value → raw, integer math only.
template <insidable B>
    requires q_format_fits<B>
constexpr raw_t<B> q_format_encode(std::int64_t value) noexcept {
    constexpr std::int64_t nd = abs_den(notch64<B>.Den);
    return raw_cast<B>((value - lower_int64<B>)*nd);
}

// raw → rational, integer math only.
template <insidable B>
    requires q_format_fits<B>
constexpr rational q_format_decode(B b) noexcept {
    constexpr std::int64_t nd = abs_den(notch64<B>.Den);
    const std::int64_t     n  = raw_int64(b) + lower_int64<B> * nd;
    if constexpr (std::has_single_bit(static_cast<std::uint64_t>(nd))) {
        // A power-of-two denominator reduces by the common trailing zeros
        // (0 reduces to 0/1).
        const std::uint64_t m =
            n < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(n) : static_cast<std::uint64_t>(n);
        const int s = std::countr_zero(m | static_cast<std::uint64_t>(nd));
        rational  r;
        r.Num = m >> s;
        r.Den = n < 0 ? -(nd >> s) : nd >> s;
        return r;
    } else
        return rational{n, nd};
}

// The value of an inside on an integer lattice as int64, which the caller
// proves holds it (user code reads b.to<T>(), which reports overflow).
template <insidable B>
    requires integer_lattice<B>
[[nodiscard]] constexpr std::int64_t to_value(B b) noexcept {
    if constexpr (value_storage<B>)
        return raw_int64(b);
    else // in uint64: raw·Notch can pass int64 although the value fits
        return static_cast<std::int64_t>(static_cast<std::uint64_t>(lower_int64<B>) +
                                         static_cast<std::uint64_t>(raw_int64(b)) * notch64<B>.Num);
}

// The callers store an integer of B (a fold count, a native quotient or
// remainder) on an integer lattice with a nonzero notch (a carry point
// {2, 2} has notch 1).
template <insidable B>
constexpr void from_value(B& b, std::int64_t val) {
    static_assert(notch64<B>.Num != 0 && abs_den(notch64<B>.Den) == 1 && abs_den(lower64<B>.Den) == 1);
    if constexpr (value_storage<B>)
        b = B::from_raw(raw_cast<B>(val));
    else
        b = B::from_raw(raw_cast<B>((static_cast<std::uint64_t>(val) - static_cast<std::uint64_t>(lower_int64<B>)) /
                                    notch64<B>.Num));
}

//-------------------------------------------------------------------------
// raw_lo / raw_hi / raw_from_offset — an integer raw's endpoints. An index
// raw is 0-based (raw_lo == 0); a value raw IS the value (raw_lo == Lower),
// so an offset needs raw_lo<L> added back before storing.
//-------------------------------------------------------------------------
// (if constexpr: naming lower_int64 instantiates it, and an index raw's
// grid may have limits past 64 bits.)
template <insidable B>
inline constexpr std::int64_t raw_lo = [] {
    if constexpr (integer_value_storage<B>)
        return lower_int64<B>;
    else
        return std::int64_t{0};
}();

template <insidable B>
inline constexpr std::int64_t raw_hi = [] {
    if constexpr (integer_value_storage<B>)
        return upper_int64<B>;
    else
        return static_cast<std::int64_t>(max_index_of<B>);
}();

// The exact raw range: 0 .. step count for index storage, Lower .. Upper
// for value storage (integers there). Sizes the work types below.
template <insidable B>
inline constexpr grid_int raw_lo_exact = index_storage<B> ? grid_int{0} : grid_numerator(lower_of<B>);
template <insidable B>
inline constexpr grid_int raw_hi_exact = index_storage<B> ? grid_of<B>.step_count() : grid_numerator(upper_of<B>);

// Value bits for every value in a list (their min .. max).
constexpr int signed_value_bits(std::initializer_list<grid_int> vals) noexcept {
    const auto [mn, mx] = std::minmax(vals);
    return signed_value_bits(mn, mx);
}

// Signed work type for an exact intermediate of Bits value bits: int64 for
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
template <raw_integer W>
[[nodiscard]] constexpr floored<W> floor_divmod(const W& t, const W& m) noexcept {
    floored<W> f;
    if constexpr (wide_integer<W>) {
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

// Offset (uint64 or int64) → raw. Adds in uint64: the bits are the same, but a
// value raw of a grid reaching past int64 (offset + Lower ≥ 2^63) must not
// overflow int64.
template <insidable L, std::integral W>
constexpr raw_t<L> raw_from_offset(W offset) noexcept {
    return raw_cast<L>(static_cast<std::uint64_t>(offset) +
                       static_cast<std::uint64_t>(raw_lo<L>)); // raw_lo: 0 for an index raw
}

// Raw → offset (slot 0 at Lower), raw_from_offset's inverse, in uint64.
template <insidable L, std::integral R>
constexpr std::uint64_t raw_offset(R raw) noexcept {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(raw)) - static_cast<std::uint64_t>(raw_lo<L>);
}

// The raw of L's Lower (low) or Upper endpoint: the exact constant for a
// rational raw, slot 0 or max_index_of for an integer one.
template <insidable L>
constexpr raw_t<L> endpoint_raw(bool low) noexcept {
    if constexpr (rational_storage<L>)
        return low ? lower64<L> : upper64<L>;
    else
        return raw_from_offset<L>(low ? std::uint64_t{0} : max_index_of<L>);
}

//-------------------------------------------------------------------------
// integer_limits vs integer_lattice — easy to confuse, both needed.
//   integer_limits<B>: Lower and Upper integer (Notch may be fractional,
//     e.g. inside<{0,100}, 1/10>). Lets Lower/Upper be used as int64 constants.
//   integer_lattice<B>: every value an integer — an integer Lower and an
//     integer non-zero Notch, or a point ⇒ integer_limits (not the converse).
//     A continuous grid (Notch 0) is not one, whatever its limits.
//-------------------------------------------------------------------------
template <insidable B>
inline constexpr bool integer_limits =
    grid_denominator(lower_of<B>) == grid_int{1} && grid_denominator(upper_of<B>) == grid_int{1};

// Whether a store into B under the call policy P uses flag F: B's own flags
// or P's. Composite flags (e.g. round_nearest = bit5 | snap) require all
// their bits set — having a subset like just `snap` does NOT match.
template <insidable B, typename P, policy_flag F>
inline constexpr bool uses_flag = has_flag(flags_of<B>, F) || plain_t<P>::test(F);

// rounding_of (policy_flag.hpp) over L's type policy and the call's policy P.
template <insidable L, typename P>
inline constexpr round_mode rounding_for = uses_flag<L, P, round_floor>       ? round_mode::floor
                                           : uses_flag<L, P, round_ceil>      ? round_mode::ceil
                                           : uses_flag<L, P, round_half_even> ? round_mode::half_even
                                           : uses_flag<L, P, round_nearest>   ? round_mode::nearest
                                                                              : round_mode::trunc;

// ⌊a / b⌋ for grid numbers (b > 0).
consteval grid_int floor_quotient(const grid_rational& a, const grid_rational& b) {
    const grid_int n = grid_numerator(a) * grid_denominator(b), d = grid_denominator(a) * grid_numerator(b);
    const grid_int q = n / d;
    return (n % d != grid_int{0} && n.negative()) ? q - grid_int{1} : q;
}

// The lattice index of Lower, ⌊Lower/Notch⌋ — slot_base on an anchored grid.
// Its parity plus an offset's says which lattice points are even.
template <insidable B>
inline constexpr grid_int lower_index_wide = notched<B> ? floor_quotient(lower_of<B>, notch_of<B>) : grid_int{0};
template <insidable B>
inline constexpr bool lower_index_odd = (static_cast<std::uint64_t>(lower_index_wide<B>) & 1) != 0;

// The rounding step onto L's lattice by rounding_for<L, P>: the exact
// lattice offset q (v/Notch when anchored, (v − Lower)/Notch otherwise) as
// a base index and whether to step one unit up from it. `negative`: v < 0.
// A rational index never wraps, however far past int64 it lies.
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
        // q − k = r/d in [0, 1): the remainder of q's numerator, by integers.
        const std::uint64_t d = abs_den(q.Den), t = q.Num % d;
        const std::uint64_t r = q.Den < 0 && t != 0 ? d - t : t;
        return {k, rounds_up(m, negative, classify_remainder(m, r, d), ((k.Num & 1) != 0) != lower_index_odd<L>)};
    }
}

// v rounded onto L's lattice {Lower + k·Notch} by rounding_for<L, P> (value
// space, ties half away from zero), for any v — not limited to [Lower,
// Upper], so wrap can round first and fold an on-lattice value after: a
// result or step past the rational range is errc::overflow.
template <insidable L, typename P>
[[nodiscard]] constexpr std::expected<rational, errc> try_round_to_lattice(rational v) {
    if constexpr (!notched<L>) // a point: apply_wrap's wrap arm is formed, never run (dispatch rejects it)
        return v;
    else {
        const auto q = anchored<L>
                           ? try_div(v, notch64<L>)
                           : try_sub(v, lower64<L>).and_then([](rational off) { return try_div(off, notch64<L>); });
        if (!q)
            return q;
        const auto [base, step] = lattice_step_of<L, P>(*q, v < 0);
        const auto j            = step ? try_add(base, rational{1}) : std::expected<rational, errc>{base};
        if (!j)
            return j;
        const auto rel = try_mul(*j, notch64<L>);
        return (rel && !anchored<L>) ? try_add(lower64<L>, *rel) : rel;
    }
}

// Round, then range-check. Lower and Upper are lattice points, so rounding
// an in-range value keeps it in range; only an out-of-range value can change
// outcome. When the policy may round (snap), round_into_range rounds such a
// value and, when it lands inside the interval, returns its raw. Only values
// within one notch of the interval can, so the exact rounding there stays
// small for huge sources. A bounded continuous grid rounds onto its bound the
// same way: its limits are values, and fractions k/D lie 1/D apart.
template <insidable L, typename P>
inline constexpr bool rounds_before_range_check =
    (notched<L> || grid_of<L>.MaxDenominator != 0) && uses_flag<L, P, snap>;

// The raw a value rounds to, when it rounds inside the limits (Ok).
template <insidable L>
struct rounded_raw {
    raw_t<L> Raw;
    bool     Ok;
};

// The rounding itself, exact (wide_value.hpp): the slot of a notched L, or
// the fraction on a continuous L's bound.
template <insidable L, typename P>
constexpr rounded_raw<L> exact_round_into_range(const rational& v);

template <insidable L, typename P>
[[nodiscard]] constexpr rounded_raw<L> round_into_range(rational v) {
    if constexpr (!rounds_before_range_check<L, P>)
        return {raw_t<L>{}, false};
    else {
        // A bound past int64: 1/int64 still widens the limits by at least a
        // step (only a prefilter: the exact rounding decides).
        constexpr rational step = [] {
            if constexpr (notched<L>)
                return notch64<L>;
            else {
                constexpr std::uint64_t D = to_rational(grid_of<L>.MaxDenominator).Num;
                constexpr std::uint64_t M = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
                return rational{1, static_cast<std::int64_t>(D < M ? D : M)};
            }
        }();
        constexpr rational lo = try_sub(lower64<L>, step).value_or(lower64<L>);
        constexpr rational hi = try_add(upper64<L>, step).value_or(upper64<L>);
        if (v <= lo || v >= hi)
            return {raw_t<L>{}, false};
        return exact_round_into_range<L, P>(v);
    }
}

// Store-side form for the assignment paths: when v rounds inside, the raw of
// the rounded value (an index or rational raw, no further rounding). Cold and
// out of line, and it returns the raw in registers instead of writing
// through the caller's inside:
//   - a second call site of the large store functions stops GCC inlining
//     them into the hot path (~40 instructions per in-range store);
//   - an escaping `lhs` address turns on the stack protector there (~3).
template <insidable L, typename P>
[[gnu::cold]] [[gnu::noinline]] constexpr rounded_raw<L> raw_if_rounds_inside(rational v) {
    return round_into_range<L, P>(v);
}

// The split offset quotient q + r/den of an unanchored grid rounded in
// value space: the value Lower + (q + r/den)·Notch is below zero exactly
// when the offset num = q·den + r is below c·den, c = −Lower/Notch,
// compared without overflow in wide integers (two limbs when c is a 64-bit
// rational).
template <insidable L, typename P>
[[nodiscard]] constexpr std::uint64_t
round_offset_unanchored(std::uint64_t q, std::uint64_t r, std::uint64_t den) noexcept {
    constexpr round_mode m        = rounding_for<L, P>;
    bool                 negative = false;
    if constexpr (lower_of<L> < 0) {
        constexpr grid_rational c  = -lower_index_of<L>; // > 0
        constexpr grid_int      cn = grid_numerator(c), cd = grid_denominator(c);
        using W  = wide_uint<limbs_for_bits(64 + std::max(cn.bit_width(), cd.bit_width()))>;
        negative = W{q * den + r} * static_cast<W>(cd) < static_cast<W>(cn) * W{den};
    }
    return q + rounds_up(m, negative, classify_remainder(m, r, den), ((q & 1) != 0) != lower_index_odd<L>);
}

// Round the non-negative offset quotient q + r/den (q, r from one division
// of the offset by den ≥ 1, r < den) to an integer notch index per L's
// rounding policy.
//
// Tie/sign rules are in VALUE space, not offset space, so assigning a value
// rounds it the same way dividing down to it does (div_rounded is the
// reference). The offset is >= 0 (sign lost by subtracting Lower); with m =
// Lower/Notch, the floor's value index is F = m + q, the value is below zero
// exactly when F is, and the rounding moves F up by one or not: no second,
// signed division. m is integral on every anchored grid, of any size; an
// unanchored one rounds the offset against the value's sign.
template <insidable L, typename P>
[[nodiscard]] constexpr std::uint64_t round_split(std::uint64_t q, std::uint64_t r, std::uint64_t den) noexcept {
    if constexpr (!anchored<L>)
        return round_offset_unanchored<L, P>(q, r, den);
    else {
        // F < 0 exactly when m < 0 and q < −m: every q when −m passes 64 bits.
        constexpr round_mode    mode      = rounding_for<L, P>;
        constexpr grid_int      mneg      = -lower_index_wide<L>;
        constexpr bool          far_below = grid_int{std::numeric_limits<std::uint64_t>::max()} < mneg;
        constexpr std::uint64_t bound =
            mneg.negative() || far_below ? std::uint64_t{0} : static_cast<std::uint64_t>(mneg);
        const bool negative = far_below || q < bound;
        return q + rounds_up(mode, negative, classify_remainder(mode, r, den), ((q & 1) != 0) != lower_index_odd<L>);
    }
}

// The offset quotient num/den rounded: round_split of its one division.
template <insidable L, typename P>
[[nodiscard]] constexpr std::uint64_t round_quotient(std::uint64_t num, std::uint64_t den) noexcept {
    return round_split<L, P>(num / den, num % den, den);
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
    else
        // Every R value on L's lattice: R's notch a multiple of L's, and R's
        // lattice anchored on L's.
        return grid_divides_evenly(notch_of<R>, notch_of<L>) &&
               grid_same_lattice(lower_of<R>, lower_of<L>, notch_of<L>);
}();

// Tail of the policy cascade: checked reports.
// Returns true if a policy handled the failure (caller should return).
// Cheap default — reports through the static category message (no string).
template <typename P>
constexpr bool range_fail(P&& policy) {
    if (policy.checks_range()) {
        policy.report(errc::overflow);
        return true;
    }
    return false;
}
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_INSIDE_DETAIL_STORAGE_HPP
