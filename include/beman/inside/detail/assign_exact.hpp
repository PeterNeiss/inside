// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_ASSIGN_EXACT_HPP
#define BEMAN_INSIDE_DETAIL_ASSIGN_EXACT_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <beman/inside/generic.hpp>
#include <beman/inside/detail/wide_value.hpp>
#include <beman/inside/grid.hpp>

#include <optional>

// policy.hpp includes this header: the recording policy of the fine-double
// store (an errc& one) is named here and completed there.
namespace beman::inside {
namespace detail {
// An on_clamp's overshoot from an inside source is an inside on
// grid_of<R> − grid_of<L>: that grid must exist (with 64-bit grid numbers its
// limits must fit them).
template <typename L, typename R>
inline constexpr bool overshoot_grid_ok = [] {
#if BEMAN_INSIDE_BIG_GRIDS
    return true;
#else
    // (The grid difference itself stops the build past them.)
    return try_sub(grid_of<R>.Interval.Lower, grid_of<L>.Interval.Upper).has_value() &&
           try_sub(grid_of<R>.Interval.Upper, grid_of<L>.Interval.Lower).has_value();
#endif
}();

struct error_ref;
} // namespace detail
template <policy_flag W, typename E>
struct policy;
} // namespace beman::inside

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
    if constexpr (clamp_action<PA>) {
        do_clamp();
        return true;
    } else if constexpr (Wrappable && wrap_action<PA>) {
        do_wrap();
        return true;
    } else if constexpr (has_policy<L, P, clamp>) {
        do_clamp();
        return true;
    } else if constexpr (has_policy<L, P, wrap>) {
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
// source kind can produce: an inside source uses its own range, an
// integral source its type's limits, a fractional (double / rational) source the whole imax range. So
// `minutes += carry` compiles for every source, and a callback taking `imax`
// still binds through the implicit operator imax(). A bound whose exact
// computation leaves imax falls back to that side of the imax range.
//---------------------------------------------------------------------------
template <insidable L, typename R>
constexpr grid wrap_carry_grid() {
    constexpr imax imax_lo = std::numeric_limits<imax>::min();
    constexpr imax imax_hi = std::numeric_limits<imax>::max();
    // A range past the rational range (a grid spanning 2^64 or more) has
    // carries in {−1, 0, 1} at most: the full imax grid holds them.
    constexpr auto span_r = try_sub(detail::upper64<L>, detail::lower64<L>);
    if constexpr (!span_r || !try_add(*span_r, detail::notch64<L>))
        return grid{imax_lo, imax_hi};
    else if constexpr (std::integral<R> || insidable<R>) {
        // An integral source spans its type's limits, an inside its interval.
        constexpr rational src_lo = [] {
            if constexpr (insidable<R>)
                return detail::lower64<R>;
            else
                return rational{std::numeric_limits<R>::min()};
        }();
        constexpr rational src_hi = [] {
            if constexpr (insidable<R>)
                return detail::upper64<R>;
            else
                return rational{std::numeric_limits<R>::max()};
        }();
        const rational range    = *try_add(*span_r, detail::notch64<L>);
        auto           carry_of = [&](rational v, imax fallback) -> imax {
            const auto off = try_sub(v, detail::lower64<L>);
            if (!off)
                return fallback;
            const auto q = try_div(*off, range);
            if (!q || *q < rational{imax_lo} || *q > rational{imax_hi})
                return fallback;
            return floor(*q);
        };
        return grid{carry_of(src_lo, imax_lo), carry_of(src_hi, imax_hi)};
    } else
        return grid{imax_lo, imax_hi};
}

template <insidable L, typename R>
constexpr auto make_wrap_carry(imax q) {
    beman::inside::inside<wrap_carry_grid<L, R>()> carry;
    from_value(carry, q); // in the carry grid by construction
    return carry;
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
    static constexpr grid_int lo = wide_numerator(lower_of<L>);
    static constexpr grid_int hi = wide_numerator(upper_of<L>);
    using W = work_int_t<signed_value_bits_of({SrcLo, SrcHi, lo, hi, SrcLo - hi, SrcHi - lo, hi - lo + grid_int{1}})>;

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
constexpr std::optional<wide_rational<exact_max<K, exact_limbs<L>>>> onto_bound(const wide_rational<K>& v) noexcept {
    constexpr std::size_t KK = exact_max<K, exact_limbs<L>>;
    using F                  = wide_rational<KK>;
    using W                  = wide_sint<KK>;
    const W D                = static_cast<W>(wide_numerator(grid_of<L>.MaxDenominator));
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
        // lies within the limits and fits the raw's limbs.
        if constexpr (clamp_action<plain_t<A>> || wrap_action<plain_t<A>>)
            static_assert(dependent_false<A>, "on_clamp / on_wrap: not supported on a continuous grid past 64 bits");
        constexpr std::size_t   KK = exact_max<K, exact_limbs<L>>;
        const wide_rational<KK> lo = exact_of_grid<KK>(lower_of<L>), hi = exact_of_grid<KK>(upper_of<L>);
        auto                    store = [&](const wide_rational<KK>& x) {
            // A bounded grid: a denominator past its bound is off the grid —
            // rounded onto it under a rounding policy, else rounding_error.
            if constexpr (grid_of<L>.MaxDenominator != 0) {
                constexpr wide_rational<KK> bound = exact_of_grid<KK>(grid_of<L>.MaxDenominator);
                if (bound.Num < reduced(x).Den) [[unlikely]] {
                    if constexpr (has_policy<L, P, snap>) {
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
            }
            if (const auto r = continuous_raw_of<raw_t<L>>(x)) [[likely]]
                lhs = L::from_raw(*r);
            else
                fail(errc::overflow);
        };
        wide_rational<KK> x{v};
        // Round first, as the builtin stores do: a value just past a limit
        // may round onto it.
        if constexpr (grid_of<L>.MaxDenominator != 0 && has_policy<L, P, snap>)
            x = round_to_bound<L, plain_t<P>>(x);
        if (x < lo || hi < x) [[unlikely]] {
            dispatch_out_of_range<true>(
                lhs,
                policy,
                action,
                [&] { store(x < lo ? lo : hi); },
                [&] {
                    // x − q·span with q = ⌊(x − lo)/span⌋: into [lo, hi).
                    const wide_rational<KK> span = hi + -lo, t = (x + -lo) / span;
                    const auto              q = floor_divmod(t.Num, t.Den).Quot;
                    store(x + -(wide_rational<KK>{q, wide_sint<KK>{1}} * span));
                });
            return lhs;
        }
        store(x);
        return lhs;
    } else if constexpr (rational_storage<L>) {
        // L holds 64-bit values: narrow through the rational. A value that does
        // not fit (a double finer than 2^-62, an exact sum) may still lie
        // within the limits: a bounded grid rounds it onto its bound first,
        // clamp saturates it past a limit; else it is off a bounded grid
        // (rounding_error) or does not fit (overflow).
        auto r = try_rational(v);
        if (!r) [[unlikely]] {
            constexpr std::size_t   KK = exact_max<K, exact_limbs<L>>;
            const wide_rational<KK> x{v};
            if constexpr (grid_of<L>.MaxDenominator != 0 && has_policy<L, P, snap>)
                r = try_rational(round_to_bound<L, plain_t<P>>(x));
            if (!r) {
                const bool low = x < exact_of_grid<KK>(lower_of<L>);
                if (!low && !(exact_of_grid<KK>(upper_of<L>) < x)) {
                    fail(grid_of<L>.MaxDenominator != 0 ? errc::rounding_error : errc::overflow);
                    return lhs;
                }
                if constexpr (!has_policy<L, P, clamp>) {
                    fail(errc::overflow);
                    return lhs;
                } else
                    r = low ? detail::lower64<L> : detail::upper64<L>;
            }
        }
        if constexpr (clamp_action<plain_t<A>> || wrap_action<plain_t<A>>)
            static_assert(
                dependent_false<A>,
                "on_clamp / on_wrap: a source past the 64-bit rational into a rational inside is not supported");
        return assignment<L, rational>::assign(lhs, *r, policy, std::forward<A>(action));
    } else {
        // (Plain variables, not a structured binding: Clang rejects a binding
        // captured by the lambdas below in constant evaluation.)
        const auto slot = [&] {
            if constexpr (point_storage<L>) {
                // One value and no lattice to divide by: the slot below it,
                // its own, or the one above (the slot count is 0).
                using F      = wide_rational<exact_max<K, exact_limbs<L>>>;
                using W      = decltype(F{}.Num);
                const auto c = F{v} <=> exact_of_grid<exact_max<K, exact_limbs<L>>>(lower_of<L>);
                return exact_index_result<exact_max<K, exact_limbs<L>>>{c < 0 ? W{-1} : W{c > 0 ? 1 : 0}, true};
            } else
                return exact_index<L, rounding_for<L, plain_t<P>>>(v);
        }();
        using I                       = decltype(slot.Index);
        constexpr std::size_t KK      = sizeof(I) / sizeof(umax);
        const I               index   = slot.Index;
        const bool            inexact = !slot.Exact && !has_policy<L, P, snap> && policy.round_check();
        constexpr I           count   = static_cast<I>(grid_of<L>.slot_count());
        // Without a rounding flag the range test reads the value itself,
        // before the notch, as the builtin stores do: an off-notch value past
        // a limit is out of range (it clamps, or reports overflow).
        bool low = index.negative(), out = low || index > count;
        if constexpr (!has_policy<L, P, snap>)
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
                            if constexpr (insidable<R>) {
                                static_assert(overshoot_grid_ok<L, R>,
                                              "on_clamp: the overshoot grid (source minus target) passes the 64-bit "
                                              "grid numbers");
                                action.Fn(
                                    lhs, exact_result<beman::inside::inside<(grid_of<R> - grid_of<L>).value()>>(over));
                            } else if constexpr (std::integral<R>)
                                action.Fn(lhs, saturate_imax(trunc(over)));
                            else if constexpr (std::floating_point<R>)
                                action.Fn(lhs, static_cast<R>(static_cast<double>(over)));
                            else
                                action.Fn(lhs, try_rational(over).value_or(rational{0}));
                        }
                    },
                    [&] {
                        if constexpr (point_storage<L>)
                            fail(errc::overflow); // a point has no period to fold by
                        else if (inexact)
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
    grid_divides_evenly(grid_sub(lower_of<R>, lower_of<L>), notch_of<L>);                 // one lattice
// The shift is the exact offset between the Lowers in notches, plus the raws
// of the Lowers (0 for an index raw, Lower for a value raw): exact on
// unanchored lattices too.
template <insidable L, insidable R>
inline constexpr grid_int same_notch_shift =
    exact_quotient(grid_sub(lower_of<R>, lower_of<L>), notch_of<L>) + raw_lo_exact<L> - raw_lo_exact<R>;
template <insidable L, insidable R>
inline constexpr int same_notch_bits = signed_value_bits_of({raw_lo_exact<R>,
                                                             raw_hi_exact<R>,
                                                             raw_lo_exact<R> + same_notch_shift<L, R>,
                                                             raw_hi_exact<R> + same_notch_shift<L, R>,
                                                             raw_lo_exact<L>,
                                                             raw_hi_exact<L>});
} // namespace beman::inside::detail

BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_DETAIL_ASSIGN_EXACT_HPP
