// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_DETAIL_ASSIGN_EXACT_HPP
#define BEMAN_INSIDE_INSIDE_DETAIL_ASSIGN_EXACT_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/generic.hpp>
#include <beman/inside/inside/detail/wide_value.hpp>
#include <beman/inside/inside/grid.hpp>
#include <beman/inside/inside/detail/call_policy.hpp>

#include <optional>

namespace beman::inside::detail {
//---------------------------------------------------------------------------
// The exact store (assign_exact) and what every assignment path shares: the
// out-of-range cascade, failure reporting, the on_wrap carry grid and the
// same-notch wide raw mapping. The source-type specializations are in
// assignment.hpp.
//---------------------------------------------------------------------------
// needs_runtime_range_check<L, P, A>: true iff any out-of-range handler would
// fire (an action, a clamp/wrap bit, or default-throw under checked).
// When false (typically `unsafe`, no action) the runtime range branch in
// `assign` is dead code and skipped, letting the autovectorizer kick in.
//---------------------------------------------------------------------------
template <insidable L, typename P, typename A>
inline constexpr bool needs_runtime_range_check =
    clamp_action<plain_t<A>> || wrap_action<plain_t<A>> || error_action<plain_t<A>> ||
    range_handled(flags_of<L> | plain_t<P>::Flags);

// Shared out-of-range policy cascade. Order: clamp/wrap *actions*, then
// clamp/wrap *policy* bits, then an on_error action, then `range_fail`. The
// callers say how clamp and wrap store; `Wrappable` is false on the
// fractional path (no wrap *action* branch). Returns true when a handler
// resolved the write.
template <bool Wrappable, insidable L, typename P, typename A, typename DoClamp, typename DoWrap>
constexpr bool dispatch_out_of_range(L& lhs, P&& policy, A&& action, DoClamp do_clamp, DoWrap do_wrap) {
    using PA = plain_t<A>;
    // A store that may leave a point grid cannot wrap: the period, Upper −
    // Lower + Notch, is 0. (A point type with wrap still comes out of
    // arithmetic, `x * 0_ins`; storing the point itself needs no range check.)
    static_assert(!point_grid<L> || clamp_action<PA> ||
                      !(wrap_action<PA> || (uses_flag<L, P, wrap> && !uses_flag<L, P, clamp>)),
                  "inside: wrap onto a point grid - a point has no period to fold by; store the point itself, or "
                  "use clamp or the default policy");
    if constexpr (clamp_action<PA>) {
        do_clamp();
        return true;
    } else if constexpr (Wrappable && wrap_action<PA>) {
        do_wrap();
        return true;
    } else if constexpr (uses_flag<L, P, clamp>) {
        do_clamp();
        return true;
    } else if constexpr (uses_flag<L, P, wrap>) {
        do_wrap();
        return true;
    } else if constexpr (error_action<PA>) { // on_error replaces the failure, not a clamp or wrap
        action.Fn(lhs, errc::overflow, errc_message(errc::overflow));
        return true;
    } else
        return range_fail(policy);
}

// A failure goes to an on_error action when there is one, else the policy.
template <typename L, typename P, typename A>
constexpr void report_failure(L& lhs, P& policy, A& action, errc code) {
    if constexpr (error_action<plain_t<A>>)
        action.Fn(lhs, code, errc_message(code));
    else
        policy.report(code);
}

//---------------------------------------------------------------------------
// The on_wrap carry is always an inside whose grid holds every carry the
// source kind can produce: an inside source uses its own range, an integral
// source its type's limits, a fractional (double / rational) source the whole
// imax range. So `minutes += carry` compiles for every source, and
// `as<imax>(carry)` reads it as a number. The carries are computed on grid
// numbers, so a grid past 64 bits has them too; each saturates at the imax
// range, as the carry itself does.
//---------------------------------------------------------------------------
template <insidable L, typename R>
constexpr grid wrap_carry_grid() {
    constexpr grid_rational imax_lo{std::numeric_limits<imax>::min()}, imax_hi{std::numeric_limits<imax>::max()};
    constexpr grid          G = grid_of<L>;
    // The period: the span plus one notch, the span alone on a continuous grid.
    constexpr grid_rational period = G.Interval.Upper - G.Interval.Lower + G.Notch;
    if constexpr ((std::integral<R> || insidable<R>) && period != grid_rational{0}) {
        const auto carry_of = [&](const grid_rational& v) {
            const grid_rational q = grid_round<round_mode::floor>((v - G.Interval.Lower) / period);
            return q < imax_lo ? imax_lo : imax_hi < q ? imax_hi : q;
        };
        if constexpr (insidable<R>)
            return grid{carry_of(lower_of<R>), carry_of(upper_of<R>)};
        else
            return grid{carry_of(grid_rational{std::numeric_limits<R>::min()}),
                        carry_of(grid_rational{std::numeric_limits<R>::max()})};
    } else
        return grid{imax_lo, imax_hi};
}

template <insidable L, typename R>
constexpr auto make_wrap_carry(imax q) {
    beman::inside::inside<wrap_carry_grid<L, R>()> carry;
    from_value(carry, q); // in the carry grid by construction
    return carry;
}

//---------------------------------------------------------------------------
// The on_clamp overshoot: the source minus the limit it was clamped to,
// exactly, as an inside for every source kind (as the wrap carry is). Its
// grid holds every value of the source kind minus either limit: the source's
// grid minus L's limits as a two-point grid {Lower, Upper} (so a floating
// source minus a continuous L stays a lattice). The source's grid: an
// inside's own, an integer type's limits, a float or double's whole range on
// its least subnormal (about 2100 bits for a double), a long double's
// stand-in range (exact_of_long_double: ±2^1100 on 2^-1130), a rational's
// 64-bit range with denominators up to 2^63. ±∞ reports the grid's end.
//---------------------------------------------------------------------------
template <typename R>
constexpr grid overshoot_source_grid() {
    if constexpr (insidable<R>)
        return grid_of<R>;
    else if constexpr (std::integral<R>)
        return grid{grid_rational{std::numeric_limits<R>::min()}, grid_rational{std::numeric_limits<R>::max()}};
    else if constexpr (wide_float<R>) {
        const grid_rational big{grid_int{1} << 1100}, tiny = grid_rational{1} / grid_rational{grid_int{1} << 1130};
        return grid{interval{-big, big}, tiny};
    } else if constexpr (std::floating_point<R>) {
        const grid_rational top{static_cast<double>(std::numeric_limits<R>::max())};
        return grid{interval{-top, top}, grid_rational{static_cast<double>(std::numeric_limits<R>::denorm_min())}};
    } else { // rational: |Num| < 2^64, |Den| ≤ 2^63
        const grid_rational top{grid_int{~umax{0}}};
        return grid{interval{-top, top}, max_denominator{grid_rational{grid_int{1} << 63}}};
    }
}

template <insidable L, typename R>
constexpr grid clamp_overshoot_grid() {
    constexpr grid limits = lower_of<L> == upper_of<L>
                                ? grid{lower_of<L>}
                                : grid{interval{lower_of<L>, upper_of<L>}, upper_of<L> - lower_of<L>};
    return overshoot_source_grid<R>() - limits;
}
template <insidable L, typename R>
using clamp_overshoot_t = beman::inside::inside<clamp_overshoot_grid<L, R>()>;

// An exact value of Result's grid as Result, for any storage.
template <insidable Result, std::size_t K>
constexpr Result exact_value_result(const wide_rational<K>& v) noexcept {
    if constexpr (fraction_storage<Result>)
        return Result::from_raw(*continuous_raw_of<raw_t<Result>>(v));
    else if constexpr (rational_storage<Result>)
        return Result::from_raw(*try_rational(v));
    else if constexpr (point_storage<Result>)
        return Result::from_raw(raw_t<Result>{});
    else
        return exact_result<Result>(v);
}

// The overshoot from its exact value (the source minus the limit).
template <insidable L, typename R, std::size_t K>
constexpr auto make_clamp_overshoot(const wide_rational<K>& over) {
    return exact_value_result<clamp_overshoot_t<L, R>>(over);
}
// The overshoot of ±∞: the grid's end of that sign.
template <insidable L, typename R>
constexpr auto infinite_clamp_overshoot(bool negative) {
    using T = clamp_overshoot_t<L, R>;
    return exact_value_result<T>(exact_of_grid<exact_limbs<T>>(negative ? lower_of<T> : upper_of<T>));
}

// The exact value of a scalar source.
template <typename R>
constexpr auto exact_of_source(const R& rhs) {
    if constexpr (std::floating_point<R>)
        return exact_of_double(static_cast<double>(rhs)); // a float or double: exact
    else
        return exact_of(rational{rhs});
}

// d saturated to imax: a clamp overshoot or a wrap carry, from a work type W
// (imax or a wide integer).
template <typename W>
constexpr imax saturate_imax(const W& d) noexcept {
    constexpr W lo = static_cast<W>(std::numeric_limits<imax>::min());
    constexpr W hi = static_cast<W>(std::numeric_limits<imax>::max());
    return static_cast<imax>(d < lo ? lo : hi < d ? hi : d);
}

//---------------------------------------------------------------------------
// unit_fold — clamp/wrap of an integer value v in [SrcLo, SrcHi] onto a
// unit-notch integer grid L, exactly. v − Lower and the range
// Upper − Lower + 1 may need 65 bits (a grid can span 2^64 values), so the
// arithmetic runs in a work type sized from all of them: imax when that
// suffices, else a wide_int.
//---------------------------------------------------------------------------
template <insidable L, grid_int SrcLo, grid_int SrcHi>
struct unit_fold {
    static constexpr grid_int lo = grid_numerator(lower_of<L>);
    static constexpr grid_int hi = grid_numerator(upper_of<L>);
    using W = work_int_t<signed_value_bits({SrcLo, SrcHi, lo, hi, SrcLo - hi, SrcHi - lo, hi - lo + grid_int{1}})>;

    static constexpr W lower = static_cast<W>(lo);
    static constexpr W upper = static_cast<W>(hi);
    static constexpr W span  = static_cast<W>(hi - lo);

    // v = Lower + carry·(span + 1) + offset with 0 ≤ offset ≤ span.
    struct folded {
        imax Carry;
        W    Offset;
    };
    static constexpr folded fold(const W& v) noexcept {
        const auto [q, w] = floor_divmod(v - lower, span + W{1});
        return {saturate_imax(q), w};
    }
};

// The integer range of a source: its type's limits, or ±2^64 (every
// integer a 64-bit rational or a grid value can be).
template <typename R>
inline constexpr grid_int source_lo = [] {
    if constexpr (std::integral<R>)
        return grid_int{std::numeric_limits<R>::min()};
    else
        return -(grid_int{1} << 64);
}();
template <typename R>
inline constexpr grid_int source_hi = [] {
    if constexpr (std::integral<R>)
        return grid_int{std::numeric_limits<R>::max()};
    else
        return grid_int{1} << 64;
}();

// A value off a bounded continuous grid (its reduced denominator past
// MaxDenominator) rounded onto it by P's rounding, kept within [Lower,
// Upper]: a limit itself off the bound moves to the nearest fraction inside.
// Empty when no fraction of the bound lies in range.
template <insidable L, typename P, std::size_t K>
constexpr std::optional<wide_rational<std::max(K, exact_limbs<L>)>> onto_bound(const wide_rational<K>& v) noexcept {
    constexpr std::size_t KK = std::max(K, exact_limbs<L>);
    using F                  = wide_rational<KK>;
    using W                  = wide_sint<KK>;
    const W D                = static_cast<W>(grid_numerator(grid_of<L>.MaxDenominator));
    const F lo = exact_of_grid<KK>(lower_of<L>), hi = exact_of_grid<KK>(upper_of<L>);
    F       r = round_to_bound<L, P>(F{v});
    if (r < lo)
        r = round_to_denominator<round_mode::ceil>(lo, D);
    else if (hi < r)
        r = round_to_denominator<round_mode::floor>(hi, D);
    if (r < lo || hi < r)
        return std::nullopt;
    return r;
}

//---------------------------------------------------------------------------
// assign_exact — store an exact value into L: the path wherever neither imax
// nor the 64-bit rational holds every value (a wide raw on either side, fine
// doubles, mul_into/div_into, parse, sum, wrap_exact). Rounds first, then range-checks,
// like the builtin paths; out of range runs the usual policy cascade.
//---------------------------------------------------------------------------
// R is the source type: an on_clamp action gets the overshoot and an
// on_wrap action the carry in the same shape as on the builtin paths.
template <typename R, insidable L, typename P, typename A, std::size_t K>
constexpr L& assign_exact(L& lhs, const wide_rational<K>& v, P&& policy, A&& action) {
    auto fail = [&](errc code) { report_failure(lhs, policy, action, code); };
    if constexpr (fraction_storage<L>) {
        // A continuous grid past 64 bits: the value itself, reduced, when it
        // lies within the limits and fits the raw's limbs. (The raw is sized
        // from the limits and the bound, so an in-range value on the bound
        // always fits; the overflows below guard that invariant.)
        constexpr std::size_t   KK = std::max(K, exact_limbs<L>);
        const wide_rational<KK> lo = exact_of_grid<KK>(lower_of<L>), hi = exact_of_grid<KK>(upper_of<L>);
        auto                    store = [&](const wide_rational<KK>& x) {
            // A denominator past the bound is off the grid — rounded onto it
            // under a rounding policy, else rounding_error.
            constexpr wide_rational<KK> bound = exact_of_grid<KK>(grid_of<L>.MaxDenominator);
            if (bound.Num < reduced(x).Den) [[unlikely]] {
                if constexpr (uses_flag<L, P, snap>) {
                    const auto o = onto_bound<L, plain_t<P>>(x);
                    if (!o)
                        return fail(errc::rounding_error);
                    if (const auto r = continuous_raw_of<raw_t<L>>(*o)) [[likely]]
                        lhs = L::from_raw(*r);
                    else
                        fail(errc::overflow);
                    return;
                } else if (std::is_constant_evaluated() || is_checked(flags_of<L> | plain_t<P>::Flags))
                    return fail(errc::rounding_error);
            }
            if (const auto r = continuous_raw_of<raw_t<L>>(x)) [[likely]]
                lhs = L::from_raw(*r);
            else
                fail(errc::overflow);
        };
        wide_rational<KK> x{v};
        // Round first, as the builtin stores do: a value just past a limit
        // may round onto it.
        if constexpr (uses_flag<L, P, snap>)
            x = round_to_bound<L, plain_t<P>>(x);
        if (x < lo || hi < x) [[unlikely]] {
            const bool low = x < lo;
            dispatch_out_of_range<true>(
                lhs,
                policy,
                action,
                [&] {
                    store(low ? lo : hi);
                    if constexpr (clamp_action<plain_t<A>>) // the source minus the limit
                        action.Fn(lhs, make_clamp_overshoot<L, R>(wide_rational<KK>{v} + -(low ? lo : hi)));
                },
                [&] {
                    // x − q·span with q = ⌊(x − lo)/span⌋: into [lo, hi).
                    const wide_rational<KK> span = hi + -lo, t = (x + -lo) / span;
                    const auto              q = floor_divmod(t.Num, t.Den).Quot;
                    store(x + -(wide_rational<KK>{q, wide_sint<KK>{1}} * span));
                    if constexpr (wrap_action<plain_t<A>>)
                        action.Fn(lhs, make_wrap_carry<L, R>(saturate_imax(q)));
                });
            return lhs;
        }
        store(x);
        return lhs;
    } else if constexpr (rational_storage<L>) {
        // L holds 64-bit values: narrow through the rational. A value that does
        // not fit (a double finer than 2^-62, an exact sum) may still lie
        // within the limits: a rounding policy rounds it onto the bound first.
        // Past a limit it runs the policy cascade on the wide value: clamp
        // stores the limit, wrap folds by the span; else it is off the grid
        // (rounding_error) or does not fit (overflow).
        auto r = try_rational(v);
        if (!r) [[unlikely]] {
            constexpr std::size_t KK = std::max(K, exact_limbs<L>);
            wide_rational<KK>     x{v};
            if constexpr (uses_flag<L, P, snap>) {
                x = round_to_bound<L, plain_t<P>>(x);
                r = try_rational(x);
            }
            if (!r) {
                const wide_rational<KK> lo = exact_of_grid<KK>(lower_of<L>), hi = exact_of_grid<KK>(upper_of<L>);
                const bool              low = x < lo;
                if (!low && !(hi < x)) {
                    fail(errc::rounding_error);
                    return lhs;
                }
                dispatch_out_of_range<true>(
                    lhs,
                    policy,
                    action,
                    [&] {
                        assignment<L, rational>::assign(lhs, low ? lower64<L> : upper64<L>, policy, no_action{});
                        if constexpr (clamp_action<plain_t<A>>) // the source minus the limit
                            action.Fn(lhs, make_clamp_overshoot<L, R>(wide_rational<KK>{v} + -(low ? lo : hi)));
                    },
                    [&] {
                        // x − q·span with q = ⌊(x − lo)/span⌋: into [lo, hi),
                        // within 64 bits unless x is off the bound.
                        const wide_rational<KK> span = hi + -lo, t = (x + -lo) / span;
                        const auto              q = floor_divmod(t.Num, t.Den).Quot;
                        const auto              f = try_rational(x + -(wide_rational<KK>{q, wide_sint<KK>{1}} * span));
                        if (!f)
                            return fail(errc::rounding_error);
                        assignment<L, rational>::assign(lhs, *f, policy, no_action{});
                        if constexpr (wrap_action<plain_t<A>>)
                            action.Fn(lhs, make_wrap_carry<L, R>(saturate_imax(q)));
                    });
                return lhs;
            }
        }
        return assignment<L, rational>::assign(lhs, *r, policy, std::forward<A>(action));
    } else {
        const auto slot = [&] {
            if constexpr (point_storage<L>) {
                // One value and no lattice to divide by: the slot below it,
                // its own, or the one above (the step count is 0).
                constexpr std::size_t KP = std::max(K, exact_limbs<L>);
                using W                  = wide_sint<KP>;
                const auto c             = wide_rational<KP>{v} <=> exact_of_grid<KP>(lower_of<L>);
                return exact_index_result<KP>{c < 0 ? W{-1} : W{c > 0 ? 1 : 0}, true};
            } else
                return exact_index<L, rounding_for<L, plain_t<P>>>(v);
        }();
        using I                       = decltype(slot.Index);
        constexpr std::size_t KK      = sizeof(I) / sizeof(umax);
        const I               index   = slot.Index;
        const bool            inexact = !slot.Exact && !uses_flag<L, P, snap> && policy.checks_rounding();
        constexpr I           count   = static_cast<I>(grid_of<L>.step_count());
        // Without a rounding flag the range test reads the value itself,
        // before the notch, as the builtin stores do: an off-notch value past
        // a limit is out of range (it clamps, or reports overflow).
        bool low = index.negative(), out = low || index > count;
        if constexpr (!uses_flag<L, P, snap>)
            if (!slot.Exact) {
                const wide_rational<KK> x{v};
                low = x < exact_of_grid<KK>(lower_of<L>);
                out = low || exact_of_grid<KK>(upper_of<L>) < x;
            }
        if (out) [[unlikely]] {
            if (dispatch_out_of_range<true>(
                    lhs,
                    policy,
                    action,
                    [&] {
                        lhs = L::from_raw(raw_of_slot<L>(low ? I{0} : count));
                        if constexpr (clamp_action<plain_t<A>>) {
                            // The overshoot rhs − bound, shaped like the builtin paths'.
                            const auto over = v + wide_rational<KK>{I{-1}, I{1}} *
                                                      exact_of_grid<KK>(low ? lower_of<L> : upper_of<L>);
                            action.Fn(lhs, make_clamp_overshoot<L, R>(over));
                        }
                    },
                    [&] {
                        if (inexact)
                            fail(errc::rounding_error); // folds off the notch too
                        else {
                            const auto [q, w] = floor_divmod(index, count + I{1});
                            lhs               = L::from_raw(raw_of_slot<L>(w));
                            if constexpr (wrap_action<plain_t<A>>)
                                action.Fn(lhs, make_wrap_carry<L, R>(saturate_imax(q)));
                        }
                    }))
                return lhs;
        }
        if (inexact) [[unlikely]] {
            fail(errc::rounding_error);
            return lhs;
        }
        lhs = L::from_raw(raw_of_slot<L>(index));
        return lhs;
    }
}

//---------------------------------------------------------------------------
// Same-notch raw mapping for wide assignments: R's raw plus a constant is L's
// raw (value index J = raw + slot base for an index raw, J = raw for a value
// raw).
//---------------------------------------------------------------------------
template <insidable L, insidable R>
inline constexpr bool same_notch_raws =
    notched<L> && notch_of<L> == notch_of<R> && !point_storage<L> && !point_storage<R> && // ⇒ integer raws
    grid_divides_evenly(lower_of<R> - lower_of<L>, notch_of<L>);                          // one lattice
// The shift is the exact offset between the Lowers in notches, plus the raws
// of the Lowers (0 for an index raw, Lower for a value raw): exact on
// unanchored lattices too.
template <insidable L, insidable R>
inline constexpr grid_int same_notch_shift =
    exact_quotient(lower_of<R> - lower_of<L>, notch_of<L>) + raw_lo_exact<L> - raw_lo_exact<R>;
template <insidable L, insidable R>
inline constexpr int same_notch_bits = signed_value_bits({raw_lo_exact<R>,
                                                          raw_hi_exact<R>,
                                                          raw_lo_exact<R> + same_notch_shift<L, R>,
                                                          raw_hi_exact<R> + same_notch_shift<L, R>,
                                                          raw_lo_exact<L>,
                                                          raw_hi_exact<L>});
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_INSIDE_DETAIL_ASSIGN_EXACT_HPP
