// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_ASSIGNMENT_HPP
#define BEMAN_INSIDE_DETAIL_ASSIGNMENT_HPP

#include <beman/inside/generic.hpp>
#include <beman/inside/detail/wide_value.hpp>
#include <beman/inside/grid.hpp>

namespace beman::inside::detail {
//---------------------------------------------------------------------------
// assignment — narrowing/coercion between bounded and arithmetic types. Three
// specialisations dispatch on the source (integral / fractional / insidable),
// each routing through `store` (in-range) and `handle_out_of_range` /
// `apply_clamp` / `apply_wrap` (policy). The insidable path also exposes
// `is_integer_mapping` / `map_raw` — a pure-integer formula in the hot path.
//---------------------------------------------------------------------------
// needs_runtime_range_check<L, P, A>: true iff any out-of-range handler would
// fire (an action, a clamp/wrap bit, or default-throw under checked).
// When false (typically `unsafe`, no action) the runtime range branch in
// `assign` is dead code and skipped, letting the autovectorizer kick in.
//---------------------------------------------------------------------------
template <insidable L, typename P, typename A>
inline constexpr bool needs_runtime_range_check =
    clamp_action<plain_t<A>> || wrap_action<plain_t<A>> || error_action<plain_t<A>> ||
    range_handled(policy_of<L> | plain_t<P>::Flags);

// Shared out-of-range policy cascade. Order: clamp/wrap/error *actions*, then
// clamp/wrap *policy* bits, then `range_fail`. The callers say how clamp and
// wrap store; `Wrappable` is false on the fractional path (no wrap *action*
// branch). Returns true when a handler resolved the write.
template <bool Wrappable, insidable L, typename P, typename A, typename DoClamp, typename DoWrap>
constexpr bool dispatch_out_of_range(L& lhs, P&& policy, A&& action, DoClamp do_clamp, DoWrap do_wrap) {
    using PA = plain_t<A>;
    if constexpr (clamp_action<PA>) {
        do_clamp();
        return true;
    } else if constexpr (Wrappable && wrap_action<PA>) {
        do_wrap();
        return true;
    } else if constexpr (error_action<PA>) {
        action.Fn(lhs, errc::overflow, errc_message(errc::overflow));
        return true;
    } else if constexpr (has_policy<L, P, clamp>) {
        do_clamp();
        return true;
    } else if constexpr (has_policy<L, P, wrap>) {
        do_wrap();
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
    constexpr imax kMin = std::numeric_limits<imax>::min();
    constexpr imax kMax = std::numeric_limits<imax>::max();
    // A range past the rational range (a grid spanning 2^64 or more) has
    // carries in {−1, 0, 1} at most: the full imax grid holds them.
    constexpr auto span_r = try_sub(detail::upper64<L>, detail::lower64<L>);
    if constexpr (!span_r || !try_add(*span_r, detail::notch64<L>))
        return grid{kMin, kMax};
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
            if (!q || *q < rational{kMin} || *q > rational{kMax})
                return fallback;
            return floor(*q);
        };
        return grid{carry_of(src_lo, kMin), carry_of(src_hi, kMax)};
    } else
        return grid{kMin, kMax};
}

template <insidable L, typename R>
constexpr auto make_wrap_carry(imax q) {
    beman::inside::inside<wrap_carry_grid<L, R>()> carry;
    from_value(carry, q); // in the carry grid by construction
    return carry;
}

//---------------------------------------------------------------------------
// unit_fold — clamp/wrap of an integer value v in [SrcLo, SrcHi] onto a
// unit-notch integer grid L, exactly. v − Lower and the range
// Upper − Lower + 1 may need 65 bits (a grid can span 2^64 values), so the
// arithmetic runs in a work type sized from all of them: imax when that
// suffices, else a wide_int.
//---------------------------------------------------------------------------
template <insidable L, grid_wide SrcLo, grid_wide SrcHi>
struct unit_fold {
    static constexpr grid_wide lo = wide_numerator(lower_of<L>);
    static constexpr grid_wide hi = wide_numerator(upper_of<L>);
    using W = work_int_t<signed_value_bits_of({SrcLo, SrcHi, lo, hi, SrcLo - hi, SrcHi - lo, hi - lo + grid_wide{1}})>;

    static constexpr W lower = static_cast<W>(lo);
    static constexpr W upper = static_cast<W>(hi);
    static constexpr W span  = static_cast<W>(hi - lo);

    // d saturated to imax (a clamp overshoot or a wrap carry).
    static constexpr imax saturate(const W& d) noexcept {
        constexpr W kMin = static_cast<W>(std::numeric_limits<imax>::min());
        constexpr W kMax = static_cast<W>(std::numeric_limits<imax>::max());
        return static_cast<imax>(d < kMin ? kMin : kMax < d ? kMax : d);
    }

    // v = Lower + carry·(span + 1) + offset with 0 ≤ offset ≤ span.
    struct folded {
        imax Carry;
        W    Offset;
    };
    static constexpr folded fold(const W& v) noexcept {
        const auto [q, w] = floor_divmod(v - lower, span + W{1});
        return {saturate(q), w};
    }
};

// The integer range of a source: its type's limits, or ±2^64 (every
// integer a 64-bit rational or a grid value can be).
template <typename R>
inline constexpr grid_wide source_lo = [] {
    if constexpr (std::integral<R>)
        return grid_wide{std::numeric_limits<R>::min()};
    else
        return -(grid_wide{1} << 64);
}();
template <typename R>
inline constexpr grid_wide source_hi = [] {
    if constexpr (std::integral<R>)
        return grid_wide{std::numeric_limits<R>::max()};
    else
        return grid_wide{1} << 64;
}();

//---------------------------------------------------------------------------
// assignment
//---------------------------------------------------------------------------
template <typename L, typename R>
struct assignment;

//---------------------------------------------------------------------------
// assign_exact — store an exact value into L. The path for a wide raw (more
// than 2^64 slots) on either side of an assignment, where neither imax nor
// the 64-bit rational holds every value. Rounds first, then range-checks,
// like the builtin paths; out of range runs the usual policy cascade.
//---------------------------------------------------------------------------
// R is the source type: an on_clamp action gets the overshoot and an
// on_wrap action the carry in the same shape as on the builtin paths.
template <typename R, insidable L, typename P, typename A, std::size_t K>
constexpr L& assign_exact(L& lhs, const exact_frac<K>& v, P&& policy, A&& action) {
    auto fail = [&](errc code) { report_failure(lhs, policy, action, code); };
    if constexpr (fraction_storage<L>) {
        // A continuous grid past 64 bits: the value itself, reduced, when it
        // lies within the limits and fits the raw's limbs.
        if constexpr (clamp_action<plain_t<A>> || wrap_action<plain_t<A>>)
            static_assert(dependent_false<A>, "on_clamp / on_wrap: not supported on a continuous grid past 64 bits");
        constexpr std::size_t KK = exact_max<K, exact_limbs<L>>;
        const exact_frac<KK>  lo = exact_of_grid<KK>(lower_of<L>), hi = exact_of_grid<KK>(upper_of<L>);
        auto                  store = [&](const exact_frac<KK>& x) {
            if (const auto r = frac_raw_of<raw_t<L>>(x)) [[likely]]
                lhs = L::from_raw(*r);
            else
                fail(errc::overflow);
        };
        const exact_frac<KK> x{v};
        if (x < lo || hi < x) [[unlikely]] {
            dispatch_out_of_range<true>(
                lhs,
                policy,
                action,
                [&] { store(x < lo ? lo : hi); },
                [&] {
                    // x − q·span with q = ⌊(x − lo)/span⌋: into [lo, hi).
                    const exact_frac<KK> span = hi + -lo, t = (x + -lo) / span;
                    const auto           q = floor_divmod(t.Num, t.Den).Quot;
                    store(x + -(exact_frac<KK>{q, wide_sint<KK>{1}} * span));
                });
            return lhs;
        }
        store(x);
        return lhs;
    } else if constexpr (rational_storage<L>) {
        // L holds 64-bit values: narrow through the rational (a value that does
        // not fit lies outside every such grid).
        const auto r = try_rational(v);
        if (!r) [[unlikely]] {
            fail(errc::overflow);
            return lhs;
        }
        if constexpr (clamp_action<plain_t<A>> || wrap_action<plain_t<A>>)
            static_assert(
                dependent_false<A>,
                "on_clamp / on_wrap: a source past the 64-bit rational into a rational inside is not supported");
        return assignment<L, rational>::assign(lhs, *r, policy, std::forward<A>(action));
    } else {
        // (Plain variables, not a structured binding: Clang rejects a binding
        // captured by the lambdas below in constant evaluation.)
        const auto slot             = exact_index<L, rounding_for<L, plain_t<P>>>(v);
        using I                     = decltype(slot.Index);
        constexpr std::size_t KK    = sizeof(I) / sizeof(umax);
        const I               index = slot.Index;
        if (!slot.Exact && !has_policy<L, P, snap> && policy.round_check()) [[unlikely]] {
            fail(errc::rounding_error);
            return lhs;
        }
        constexpr I count = static_cast<I>(grid_of<L>.slot_count());
        if (index.negative() || index > count) [[unlikely]] {
            auto saturate = [](const I& d) -> imax {
                constexpr imax kMin = std::numeric_limits<imax>::min(), kMax = std::numeric_limits<imax>::max();
                return d < I{kMin} ? kMin : I{kMax} < d ? kMax : static_cast<imax>(d);
            };
            if (dispatch_out_of_range<true>(
                    lhs,
                    policy,
                    action,
                    [&] {
                        const bool low = index.negative();
                        lhs            = L::from_raw(raw_of_slot<L>(low ? I{0} : count));
                        if constexpr (clamp_action<plain_t<A>>) {
                            // The overshoot rhs − bound, shaped like the builtin paths'.
                            const auto over =
                                v + exact_frac<KK>{I{-1}, I{1}} * exact_of_grid<KK>(low ? lower_of<L> : upper_of<L>);
                            if constexpr (insidable<R>)
                                action.Fn(
                                    lhs, exact_result<beman::inside::inside<(grid_of<R> - grid_of<L>).value()>>(over));
                            else if constexpr (std::integral<R>)
                                action.Fn(lhs, saturate(trunc(over)));
                            else if constexpr (std::floating_point<R>)
                                action.Fn(lhs, static_cast<R>(static_cast<double>(over)));
                            else
                                action.Fn(lhs, try_rational(over).value_or(rational{0}));
                        }
                    },
                    [&] {
                        const auto [q, w] = floor_divmod(index, count + I{1});
                        lhs               = L::from_raw(raw_of_slot<L>(w));
                        if constexpr (wrap_action<plain_t<A>>)
                            action.Fn(lhs, make_wrap_carry<L, R>(saturate(q)));
                    }))
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
inline constexpr bool same_notch_raws = integer_storage<L> && integer_storage<R> && !point_storage<L> &&
                                        !point_storage<R> && notched<L> && notch_of<L> == notch_of<R>;
template <insidable L, insidable R>
inline constexpr grid_wide same_notch_shift =
    (index_storage<R> ? slot_base<R> : grid_wide{0}) - (index_storage<L> ? slot_base<L> : grid_wide{0});
template <insidable L, insidable R>
inline constexpr int same_notch_bits = signed_value_bits_of({raw_lo_exact<R> + same_notch_shift<L, R>,
                                                             raw_hi_exact<R> + same_notch_shift<L, R>,
                                                             raw_lo_exact<L>,
                                                             raw_hi_exact<L>});

//---------------------------------------------------------------------------
// assign(insidable, integral)
//---------------------------------------------------------------------------
template <insidable L, std::integral R>
struct assignment<L, R> {
  private:
    // A 64-bit unsigned source can exceed imax: `static_cast<imax>(rhs)` would
    // turn 2^64−1 into −1. Compare such a source in umax instead; every
    // narrower or signed source is exact in imax and keeps the plain cast.
    static constexpr bool wide_unsigned = std::is_unsigned_v<R> && sizeof(R) >= sizeof(imax);

    // Exact integer arithmetic for the bounds and the cold clamp/wrap paths:
    // an integer interval may reach past int64 on either side (e.g.
    // {0, 2^64−1}), and rhs − Lower may need 65 bits.
    using fold = unit_fold<L, source_lo<R>, source_hi<R>>;
    using W    = typename fold::W;

    // Hot-path range test. Bounds within imax compare as plain integers;
    // bounds past int64 compare exactly in the fold's work type.
    static constexpr bool out_of_range(R rhs) noexcept {
        if constexpr (fits_imax(interval_of<L>)) {
            constexpr imax lower = lower_imax<L>, upper = upper_imax<L>;
            if constexpr (wide_unsigned)
                return (lower > 0 && static_cast<umax>(rhs) < static_cast<umax>(lower)) || upper < 0 ||
                       static_cast<umax>(rhs) > static_cast<umax>(upper);
            else
                return static_cast<imax>(rhs) < lower || static_cast<imax>(rhs) > upper;
        } else {
            const W v = static_cast<W>(rhs);
            return v < fold::lower || fold::upper < v;
        }
    }

    template <typename A>
    static constexpr void apply_clamp(L& lhs, R rhs, A&& action) {
        // Pre: rhs is out of [Lower, Upper] (only called from handle_out_of_range),
        // so the two-way pick is the full clamp.
        const W    v   = static_cast<W>(rhs);
        const bool low = v < fold::lower;
        lhs            = L::from_raw(raw_of_slot<L>(low ? W{0} : fold::span));
        if constexpr (clamp_action<plain_t<A>>)
            action.Fn(lhs, fold::saturate(v - (low ? fold::lower : fold::upper)));
    }

    template <typename A>
    static constexpr void apply_wrap(L& lhs, R rhs, A&& action) {
        // Modular wrap on the exact offset rhs − Lower into span + 1 slots. The
        // carry saturates at imax, like the carry grid (wrap_carry_grid).
        const auto [carry, w] = fold::fold(static_cast<W>(rhs));
        lhs                   = L::from_raw(raw_of_slot<L>(w));
        if constexpr (wrap_action<plain_t<A>>)
            action.Fn(lhs, make_wrap_carry<L, R>(carry));
    }

    template <typename P, typename A>
    static constexpr bool handle_out_of_range(L& lhs, R rhs, P&& policy, A&& action) {
        return dispatch_out_of_range<true>(
            lhs, policy, action, [&] { apply_clamp(lhs, rhs, action); }, [&] { apply_wrap(lhs, rhs, action); });
    }

    static constexpr void store(L& lhs, R rhs) {
        if constexpr (value_storage<L>)
            lhs = L::from_raw(raw_cast<L>(rhs));
        else if constexpr (detail::point_grid<L>)
            lhs = L::from_raw(0); // notch_storage point grid: 0 is the only offset
        else if constexpr (qformat_codec_fits<L>)
            lhs = L::from_raw(q_format_encode<L>(static_cast<imax>(rhs)));
        else // index storage on a notch 1/K grid: the offset is an exact integer
        {
            rational raw = ((rhs - detail::lower64<L>) / detail::notch64<L>).value();
            lhs          = L::from_raw(raw_cast<L>(raw.Numerator));
        }
    }

  public:
    // An integer lands on L's grid whenever the notch is 1/K over an integer
    // Lower (or the grid is continuous); otherwise it may fall between notches
    // and must round or report exactly like the same value given as a rational.
    static constexpr bool integers_on_grid =
        !detail::notched<L> || (detail::notch64<L>.Numerator == 1 && abs_den(detail::lower64<L>.Denominator) == 1);

    template <typename P, typename A = no_action>
    static constexpr L& assign(L& lhs, const R& rhs, P&& policy, A&& action = {}) {
        // wrap/clamp bring any value into range (matches assign_intervals_ok).
        static_assert(has_policy<L, P, wrap> || has_policy<L, P, clamp> ||
                          not excludes(interval_of<L>, interval_of<R>),
                      "rhs type's range lies entirely outside lhs interval and the policy cannot bring it into range");

        if constexpr (wide_valued<L>)
            return assign_exact<R>(lhs, exact_of(rhs), policy, std::forward<A>(action));
        else if constexpr (!integers_on_grid)
            return assignment<L, rational>::assign(lhs, rational{rhs}, policy, std::forward<A>(action));
        else {
            // The out-of-range check runs unconditionally — clamp/wrap
            // policies handle it via apply_*, which is constexpr-clean. Only the
            // unhandled-checked path winds up calling `policy.report`, which
            // contains its own `std::is_constant_evaluated()` guard.
            if constexpr (not includes(interval_of<L>, interval_of<R>)) {
                if constexpr (integer_limits<L>) {
                    // Skip the runtime range branch entirely when every handler would
                    // be dead anyway — the dead branch otherwise inhibits autovec.
                    if constexpr (needs_runtime_range_check<L, plain_t<P>, plain_t<A>>) {
                        if (out_of_range(rhs)) [[unlikely]] {
                            // The integer clamp/wrap formulas need consecutive integers to be
                            // adjacent grid points (notch 1); a finer notch wraps modulo
                            // span + notch on the rational path.
                            if constexpr (detail::notch64<L> == 1) {
                                if (handle_out_of_range(lhs, rhs, policy, action))
                                    return lhs;
                            } else
                                return assignment<L, rational>::assign(lhs, rational{rhs}, policy, action);
                        }
                    }
                } else if (not includes(interval_of<L>, rhs)) {
                    // Non-integer L bounds: route through the rational path so fractional
                    // Lower/Upper drive clamp/error correctly.
                    return assignment<L, rational>::assign(lhs, rational{rhs}, policy, action);
                }
            }

            store(lhs, rhs);
            return lhs;
        }
    }
};

//---------------------------------------------------------------------------
// The integer store of an unanchored lattice: Notch = S/K and Lower = M/K
// over their common denominator K. MaxDen bounds a source denominator so
// that num·K, M·aden and aden·S stay in imax (|num| ≤ |value|·aden, and
// every in-range |value| is at most Mag); Ok when the grid allows any.
//---------------------------------------------------------------------------
struct unanchored_codec_t {
    imax K, S, M;
    umax MaxDen;
    bool Ok;
};
template <insidable L>
inline constexpr unanchored_codec_t unanchored_codec = [] {
    constexpr unanchored_codec_t no{0, 0, 0, 0, false};
    if constexpr (anchored<L> || !index_storage<L> || point_storage<L> || !values_fit_imax<L>)
        return no;
    else {
        constexpr umax cap = static_cast<umax>(std::numeric_limits<imax>::max());
        const rational n = detail::notch64<L>, lo = detail::lower64<L>, hi = detail::upper64<L>;
        const umax     dn = abs_den(n.Denominator), dl = abs_den(lo.Denominator);
        umax           k;
        if (mul_overflow(dn / std::gcd(dn, dl), dl, &k) || k > cap)
            return no;
        umax s, mm;
        if (mul_overflow(n.Numerator, k / dn, &s) || s > cap || mul_overflow(lo.Numerator, k / dl, &mm) || mm > cap)
            return no;
        // Mag: a bound on every in-range |value|, plus one.
        const umax mag = static_cast<umax>(ceil(abs(lo) > abs(hi) ? abs(lo) : abs(hi))) + 1;
        umax       km, kms;
        if (mul_overflow(k, mag, &km) || mul_overflow(km, umax{4}, &kms))
            return no;
        const umax by_num = cap / kms; // num·K and M·aden each below cap/2
        const umax by_den = cap / s;   // aden·S
        return unanchored_codec_t{static_cast<imax>(k),
                                  static_cast<imax>(s),
                                  lo.Denominator < 0 ? -static_cast<imax>(mm) : static_cast<imax>(mm),
                                  by_num < by_den ? by_num : by_den,
                                  true};
    }
}();

//---------------------------------------------------------------------------
// assign(insidable, floating_point | rational)
//---------------------------------------------------------------------------
template <insidable L, typename R>
    requires fractional<R>
struct assignment<L, R> {
  private:
    // A floating source with |rhs| ≥ 2^64 lies outside every grid and has no
    // rational form (rational(double) would fail): decide such values by sign.
    static constexpr bool huge(const R& rhs) noexcept {
        if constexpr (std::floating_point<R>)
            return !(rhs < 0x1p64 && rhs > -0x1p64);
        else
            return false;
    }

    static constexpr bool below_lower(const R& rhs) { return huge(rhs) ? rhs < 0 : rhs < detail::lower64<L>; }

    template <typename P, typename A>
    static constexpr void apply_clamp(L& lhs, R rhs, P&&, A&& action) {
        const bool low     = below_lower(rhs);
        R          clamped = low ? static_cast<R>(detail::lower64<L>) : static_cast<R>(detail::upper64<L>);
        R          overshoot;
        if constexpr (std::same_as<R, rational>)
            overshoot = (rhs - clamped).value_or(rational{0});
        else
            overshoot = rhs - clamped;

        // The clamp target is an interval endpoint — a grid point — so the slot is 0
        // or max_index_v, no rounding. f64 takes the endpoint as a double, rational
        // the exact constant (a double round-trip would lose non-dyadic endpoints);
        // raw_from_offset<L> adds Lower back for direct-encoded storage.
        if constexpr (fp_storage<L>)
            lhs = L::from_raw(low ? static_cast<double>(detail::lower64<L>) : static_cast<double>(detail::upper64<L>));
        else if constexpr (rational_storage<L>)
            lhs = L::from_raw(low ? detail::lower64<L> : detail::upper64<L>);
        else
            lhs = L::from_raw(raw_from_offset<L>(low ? umax{0} : max_index_v<L>));

        if constexpr (clamp_action<plain_t<A>>)
            action.Fn(lhs, overshoot);
    }

  public:
    // Exposed (not private) so the insidable-rhs wrap path can reuse the
    // rational specialization's modular wrap on fractional/notch grids, and
    // so the wrap path can reuse store_checked after computing the wrapped
    // value.
    //
    // apply_wrap for a fractional R — modular reduction into [Lower, Lower + range)
    // followed by store_checked so the rounding policy still applies if rhs
    // doesn't land on a notch after wrapping. range = Upper - Lower + Notch.
    template <typename P, typename A>
    static constexpr void apply_wrap(L& lhs, R rhs, P&& policy, A&& action) {
        // Round onto the lattice first (by the policy, like every other store),
        // then fold: an on-lattice value folds onto a grid point, so rounding
        // can never carry it past Upper.
        // The fold's quotient q is an imax: a floating source of 2^63 or more
        // cannot be wrapped with a deliverable carry (nor converted exactly).
        if constexpr (std::floating_point<R>)
            if (!(rhs < 0x1p63 && rhs > -0x1p63)) [[unlikely]] {
                policy.report(errc::overflow);
                return;
            }
        rational rhs_r{rhs};
        if constexpr (has_policy<L, P, snap>) {
            const auto r = try_round_to_lattice<L, P>(rhs_r);
            if (!r) [[unlikely]] {
                policy.report(errc::overflow);
                return;
            }
            rhs_r = *r;
        }
        imax     q;
        rational wrapped;
        if (abs_den(rhs_r.Denominator) == 1 && detail::notch64<L> == 1 &&
            abs_den(detail::lower64<L>.Denominator) == 1) {
            // Integer value on a unit lattice: fold exactly (the grid may span
            // 2^64 values, past the rational range).
            using fold         = unit_fold<L, source_lo<rational>, source_hi<rational>>;
            using W            = typename fold::W;
            const auto [qq, w] = fold::fold(static_cast<W>(wide_numerator(rhs_r)));
            const W v          = fold::lower + w;
            q                  = qq;
            wrapped            = v < W{0} ? -rational{static_cast<umax>(-v)} : rational{static_cast<umax>(v)};
        } else {
            // q = floor((rhs - lower) / range), wrapped = rhs - q * range. A step
            // past the 64-bit rational range, or a fold count past imax, cannot
            // be computed exactly: report it rather than store a wrapped guess.
            const auto span  = try_sub(detail::upper64<L>, detail::lower64<L>);
            const auto range = span ? try_add(*span, detail::notch64<L>) : span;
            const auto off   = try_sub(rhs_r, detail::lower64<L>);
            const auto quot  = (range && off) ? try_div(*off, *range) : off;
            if (!range || !quot || *quot < rational{std::numeric_limits<imax>::min()} ||
                *quot > rational{std::numeric_limits<imax>::max()}) [[unlikely]] {
                policy.report(errc::overflow);
                return;
            }
            q             = floor(*quot);
            const auto qr = try_mul(rational{q}, *range);
            const auto w  = qr ? try_sub(rhs_r, *qr) : qr;
            if (!w) [[unlikely]] {
                policy.report(errc::overflow);
                return;
            }
            wrapped = *w;
        }

        // Re-enter the rational-rhs specialization for the actual store so the
        // notch / rounding policy logic is exercised once.
        assignment<L, rational>::store_checked(lhs, wrapped, policy, action);

        if constexpr (wrap_action<plain_t<A>>)
            action.Fn(lhs, make_wrap_carry<L, R>(q));
    }

    // Cold and out of line: the exact wide slot for a quotient past the
    // 64-bit rational, kept off the hot store path. It returns 16 bytes, not
    // the 72-byte exact_index_result: GCC charges the copy of a large return
    // value against inlining the whole store (+17% instructions measured).
    struct wide_slot_result {
        umax Slot;
        bool Exact;
    };
    template <typename P>
    [[gnu::cold, gnu::noinline]] static constexpr wide_slot_result wide_slot(const rational& v) {
        const exact_index_result r = exact_index<L, rounding_for<L, P>>(exact_of(v));
        return {static_cast<umax>(r.Index), r.Exact}; // in range: fits 64 bits
    }

    template <typename P, typename A = no_action>
    static constexpr void store_checked(L& lhs, R rhs, P&& policy, A&& action = {}) {
        if constexpr (rational_storage<L> && !detail::notched<L>)
            lhs = L::from_raw(rhs); // continuous: store verbatim
        else if constexpr (fp_storage<L>) {
            // f64 target: raw IS the value — snap to the dyadic grid (range handling
            // already ran in the assign cascade; finite guard mirrors store_f64's).
            const double v = static_cast<double>(rhs);
            if (!(v - v == 0)) [[unlikely]] // assign() screens these first
                return policy.report(errc::not_finite);
            // v rounds rhs; at a rounding boundary the exact value decides.
            const auto side = [&] {
                const auto c = rhs <=> rational{v};
                return c > 0 ? 1 : c < 0 ? -1 : 0;
            };
            // Off the grid without a rounding policy: rounding_error, as for
            // integer storage.
            if constexpr (!(has_policy<L, P, round_nearest> || has_policy<L, P, round_floor> ||
                            has_policy<L, P, round_ceil> || has_policy<L, P, round_half_even> ||
                            has_policy<L, P, snap>))
                if (policy.round_check() &&
                    (side() != 0 || snap_double<grid_of<L>, round_mode::trunc, /*AnySign=*/true>(v) != v)) [[unlikely]]
                    return report_failure(lhs, policy, action, errc::rounding_error);
            lhs = L::from_raw(snap_double_from<grid_of<L>, rounding_for<L, P>>(v, side));
        } else if constexpr (detail::point_grid<L>) {
            // Singleton grid: offset encoding → Raw=0; rational/direct → Raw = Lower.
            if constexpr (rational_storage<L>)
                lhs = L::from_raw(detail::lower64<L>);
            else if constexpr (value_storage<L>)
                lhs = L::from_raw(raw_cast<L>(raw_lo<L>));
            else
                lhs = L::from_raw(0);
        } else {
            // Store the k-th notch slot: rational storage holds the snapped value;
            // raw_from_offset<L> covers offset- and direct-encoded integers.
            auto store_slot = [&](auto k) {
                if constexpr (rational_storage<L>)
                    lhs = L::from_raw((detail::lower64<L> + (rational{k} * detail::notch64<L>).value()).value());
                else
                    lhs = L::from_raw(raw_from_offset<L>(k));
            };

            constexpr bool has_round_flag = has_policy<L, P, round_nearest> || has_policy<L, P, round_floor> ||
                                            has_policy<L, P, round_ceil> || has_policy<L, P, round_half_even> ||
                                            has_policy<L, P, snap>;

            // A floating source on a grid whose values double holds exactly:
            // round in double (snap_double, the integer storage's rule) and
            // read the slot off the value index, exact. No rational round trip.
            if constexpr (std::floating_point<R> && integer_storage<L> && !wide_valued<L> && anchored<L> &&
                          double_exact<grid_of<L>>) {
                constexpr double nd = static_cast<double>(detail::notch64<L>);
                constexpr imax   lo = signed_numerator((detail::lower64<L> / detail::notch64<L>).value());
                const double     v  = static_cast<double>(rhs);
                const double     s  = snap_double<grid_of<L>, rounding_for<L, P>>(v);
                if constexpr (!has_round_flag)
                    if (s != v && policy.round_check()) [[unlikely]]
                        return report_failure(lhs, policy, action, errc::rounding_error);
                store_slot(static_cast<umax>(static_cast<imax>(s / nd) - lo));
                return;
            }

            // Q-format integer shortcut: with integer Lower and notch 1/K the offset is
            // (num − Lo·aden)·(K/g) / (aden/g), g = gcd(aden, K) — one gcd + integer ops
            // instead of two rational ops. round_quotient is invariant under reduction,
            // so the slot is bit-identical to the rational path. Oversized denominators
            // fall through (the kMaxDen guard keeps every product inside imax).
            if constexpr (qformat_codec_fits<L> && !fp_storage<L> && detail::notched<L>) {
                constexpr imax K   = abs_den(detail::notch64<L>.Denominator);
                constexpr imax Lo  = lower_imax<L>;
                constexpr umax kKM = [] {
                    // 2 · K · M with saturation (M bounds |value| and the offset span)
                    umax k = static_cast<umax>(K);
                    umax m = static_cast<umax>(ceil(((detail::abs(detail::lower64<L>) > detail::abs(detail::upper64<L>)
                                                          ? detail::abs(detail::lower64<L>)
                                                          : detail::abs(detail::upper64<L>))))) *
                                 2 +
                             2;
                    if (k > std::numeric_limits<umax>::max() / m)
                        return std::numeric_limits<umax>::max();
                    umax km = k * m;
                    return (km > std::numeric_limits<umax>::max() / 2) ? std::numeric_limits<umax>::max() : km * 2;
                }();
                constexpr umax kMaxDen = static_cast<umax>(std::numeric_limits<imax>::max()) / kKM;

                const rational rv{rhs}; // exact (copy for rational R)
                const umax     aden = abs_den(rv.Denominator);
                if (kMaxDen != 0 && aden <= kMaxDen) {
                    const umax g    = std::gcd(aden, static_cast<umax>(K));
                    const umax den2 = aden / g;
                    const imax k2   = K / static_cast<imax>(g);
                    const imax num  = signed_numerator(rv);
                    const umax onum = // ≥ 0: rhs ≥ Lower (in range)
                        static_cast<umax>((num - Lo * static_cast<imax>(aden)) * k2);
                    if (den2 == 1) {
                        store_slot(onum);
                        return;
                    }
                    if constexpr (has_round_flag) {
                        store_slot(round_quotient<L, P>(onum, den2));
                        return;
                    }
                    // strict policy, off-notch: fall through to the rational path for
                    // the error message / action plumbing (cold).
                }
            }

            // The same shortcut for an unanchored lattice: with K the common
            // denominator of Notch and Lower, Notch = sN/K and Lower = m/K, the
            // offset is (num·K − m·aden)/(aden·sN), reduced by g = gcd(aden, K).
            if constexpr (unanchored_codec<L>.Ok) {
                constexpr auto c = unanchored_codec<L>;
                const rational rv{rhs};
                const umax     aden = abs_den(rv.Denominator);
                if (aden <= c.MaxDen) {
                    const umax g    = std::gcd(aden, static_cast<umax>(c.K));
                    const imax num  = signed_numerator(rv);
                    const umax onum = // ≥ 0: rhs ≥ Lower (in range)
                        static_cast<umax>(num * (c.K / static_cast<imax>(g)) - c.M * static_cast<imax>(aden / g));
                    const umax den2 = (aden / g) * static_cast<umax>(c.S);
                    if (onum % den2 == 0) {
                        store_slot(onum / den2);
                        return;
                    }
                    if constexpr (has_round_flag) {
                        store_slot(round_quotient<L, P>(onum, den2));
                        return;
                    }
                }
            }

            // The exact quotient can overflow the 64-bit rational range (huge
            // source denominator × fine notch): take the slot from the exact wide
            // index instead (rhs is in range, so it fits L's raw). Rounding is
            // the value-space rule, as everywhere else.
            const auto quotient = (rhs - detail::lower64<L>) / detail::notch64<L>;
            if (!quotient.has_value()) [[unlikely]] {
                const wide_slot_result slot = wide_slot<P>(rational{rhs});
                if constexpr (!has_round_flag)
                    if (!slot.Exact && policy.round_check()) [[unlikely]]
                        return report_failure(lhs, policy, action, errc::rounding_error);
                store_slot(slot.Slot);
                return;
            }
            rational raw = *quotient;
            umax     den = static_cast<umax>(raw.Denominator);
            if (den == 1) {
                store_slot(raw.Numerator);
                return;
            }

            if constexpr (!has_round_flag)
                if (policy.round_check()) [[unlikely]]
                    return report_failure(lhs, policy, action, errc::rounding_error);
            store_slot(round_quotient<L, P>(raw.Numerator, den));
        }
    }

  private:
    // Range test for the source value. A floating source compares in double when
    // both endpoints are exact doubles (then the comparison is exact), instead of
    // converting the value to a rational first.
    static constexpr bool double_bounds_exact =
        std::floating_point<R> && rational{static_cast<double>(detail::lower64<L>)} == detail::lower64<L> &&
        rational{static_cast<double>(detail::upper64<L>)} == detail::upper64<L>;

    // A rational source on integer endpoints compares by multiplying the
    // endpoint by the denominator (n/d ≤ m ⇔ n ≤ m·d; an overflowing m·d
    // exceeds any n) — exact, and no division.
    static constexpr bool integer_bounds = std::same_as<R, rational> && abs_den(detail::lower64<L>.Denominator) == 1 &&
                                           abs_den(detail::upper64<L>.Denominator) == 1;

    static constexpr bool out_of_interval(const R& rhs) {
        if (huge(rhs))
            return true;
        if constexpr (double_bounds_exact) {
            constexpr double lo = static_cast<double>(detail::lower64<L>);
            constexpr double hi = static_cast<double>(detail::upper64<L>);
            return rhs < lo || rhs > hi;
        } else if constexpr (integer_bounds) {
            constexpr imax lo = signed_numerator(detail::lower64<L>);
            constexpr imax hi = signed_numerator(detail::upper64<L>);
            const umax     n = rhs.Numerator, d = abs_den(rhs.Denominator);
            auto           le = [&](umax m) {
                umax p;
                return mul_overflow(m, d, &p) || n <= p;
            };
            auto ge = [&](umax m) {
                umax p;
                return !mul_overflow(m, d, &p) && n >= p;
            };
            if (rhs.Denominator < 0 && n != 0) // value −n/d < 0
            {
                bool in_lo, in_hi;
                if constexpr (lo >= 0)
                    in_lo = false;
                else
                    in_lo = le(safe_abs(lo));
                if constexpr (hi >= 0)
                    in_hi = true;
                else
                    in_hi = ge(safe_abs(hi));
                return !(in_lo && in_hi);
            }
            bool in_lo, in_hi; // value n/d ≥ 0
            if constexpr (lo <= 0)
                in_lo = true;
            else
                in_lo = ge(static_cast<umax>(lo));
            if constexpr (hi < 0)
                in_hi = false;
            else
                in_hi = le(static_cast<umax>(hi));
            return !(in_lo && in_hi);
        } else
            return not includes(interval_of<L>, rhs);
    }

  public:
    template <typename P, typename A = no_action>
    static constexpr L& assign(L& lhs, const R& rhs, P&& policy, A&& action = {}) {
        if constexpr (wide_valued<L>) {
            // NaN / ±inf first, as below; a finite |rhs| ≥ 2^64 is an integer,
            // taken exactly (or by its side, past the grid: exact_of_large).
            if constexpr (std::floating_point<R>)
                if (!(rhs - rhs == 0) || huge(rhs)) [[unlikely]] {
                    if (rhs != rhs) {
                        report_failure(lhs, policy, action, errc::not_finite);
                        return lhs;
                    }
                    if (!(rhs - rhs == 0) && !has_policy<L, P, clamp>) {
                        report_failure(lhs, policy, action, errc::not_finite);
                        return lhs;
                    }
                    return assign_exact<R>(
                        lhs, exact_of_large<L>(static_cast<double>(rhs)), policy, std::forward<A>(action));
                }
            return assign_exact<R>(lhs, exact_of(rational{rhs}), policy, std::forward<A>(action));
        } else
            return assign_builtin(lhs, rhs, policy, std::forward<A>(action));
    }

  private:
    template <typename P, typename A>
    static constexpr L& assign_builtin(L& lhs, const R& rhs, P&& policy, A&& action) {
        // NaN / ±inf: no rational value to round or range-check. clamp saturates
        // an infinity; everything else reports not_finite through the policy.
        if constexpr (std::floating_point<R>)
            if (!(rhs - rhs == 0)) [[unlikely]] {
                if constexpr (has_policy<L, P, clamp>)
                    if (rhs == rhs)
                        return assignment<L, rational>::assign(
                            lhs, rhs > 0 ? detail::upper64<L> : detail::lower64<L>, policy);
                report_failure(lhs, policy, action, errc::not_finite);
                return lhs;
            }

        if (out_of_interval(rhs)) [[unlikely]] {
            // Round first: a value just outside may round onto an endpoint.
            if constexpr (rounds_before_range_check<L, plain_t<P>>)
                if (!huge(rhs))
                    if (const auto rr = raw_if_rounds_inside<L, plain_t<P>>(rational{rhs}); rr.Ok) {
                        lhs = L::from_raw(rr.Raw);
                        return lhs;
                    }
            // Fractional path has no wrap *action* branch (Wrappable = false).
            if (dispatch_out_of_range<false>(
                    lhs,
                    policy,
                    action,
                    [&] { apply_clamp(lhs, rhs, policy, action); },
                    [&] { apply_wrap(lhs, rhs, policy, action); }))
                return lhs;
        }

        store_checked(lhs, rhs, policy, action);
        return lhs;
    }
};

//---------------------------------------------------------------------------
// assign(insidable, insidable)
//---------------------------------------------------------------------------
template <insidable L, insidable R>
struct assignment<L, R> {
  private:
    // Offset/Factor map rhs.Raw → lhs.Raw via `lhs.Raw = Factor·rhs.Raw + Offset`.
    // Branches: L rational (pass value through), R rational (pre-divide by
    // detail::notch64<L>), both integer (the hot path, collapses to integer math).
    static constexpr rational calcOffset() {
        if constexpr (rational_storage<L>)
            return detail::lower64<R>;
        else if constexpr (!detail::notched<L>)
            // A point L (notch 0): one value, mapping unused. 0 avoids the
            // divide-by-zero by the notch.
            return rational{0};
        else if constexpr (rational_storage<R>)
            return -(detail::lower64<L> / detail::notch64<L>).value();
        else
            return ((detail::lower64<R> - detail::lower64<L>) / detail::notch64<L>).value();
    }

    static constexpr rational calcFactor() {
        if constexpr (rational_storage<L>)
            return detail::notch64<R>;
        else if constexpr (!detail::notched<L>)
            // A point L (see calcOffset). A denominator-1 Factor also makes
            // assign_notch_ok vacuously true.
            return rational{0};
        else if constexpr (rational_storage<R>)
            return (rational{1} / detail::notch64<L>).value();
        else if constexpr (point_storage<R>)
            return rational{0}; // raw is always 0: the mapping is Offset alone
        else
            return (detail::notch64<R> / detail::notch64<L>).value();
    }

  public:
    static constexpr rational Offset = calcOffset();
    static constexpr rational Factor = calcFactor();

    // Raw-space integer-only mapping — requires integer raw storage on both
    // sides (not rational, not f64).
    // It also needs every raw and every mapped raw in imax: map_raw's L-raw
    // range is [Offset, Offset + Factor·max_index<R>] (+ Lower for value storage).
    static constexpr bool is_integer_mapping = [] {
        if constexpr (rational_storage<L> || rational_storage<R> || fp_storage<L> || fp_storage<R> ||
                      abs_den(Factor.Denominator) != 1 || abs_den(Offset.Denominator) != 1 || !values_fit_imax<L> ||
                      !values_fit_imax<R>)
            return false;
        else {
            const rational base = index_storage<L> ? rational{0} : detail::lower64<L>;
            const auto     lo   = Offset + base;
            const auto     span = Factor * rational{max_index_v<R>};
            if (!lo || !span)
                return false;
            const auto hi = *lo + *span;
            return hi.has_value() && *lo >= rational{std::numeric_limits<imax>::min()} &&
                   *hi <= rational{std::numeric_limits<imax>::max()};
        }
    }();

    // Non-integer mapping folded to one integer multiply-add:
    //   Offset + Factor·raw = (o_s·f_d + raw·f_n·o_d) / (o_d·f_d)
    // with every coefficient compile-time. round_quotient is invariant under
    // fraction reduction, so rounding the unreduced pair is bit-identical to
    // reducing through the two rational ops first. ok gates on every product
    // (including the worst-case runtime numerator over R's raw range)
    // provably fitting imax; mul/add/den are zeroed when not ok.
    struct affine_map_t {
        imax Mul;
        imax Add;
        imax Den;
        bool Ok;
    };
    static constexpr affine_map_t affine_map = [] {
        constexpr affine_map_t no{0, 0, 0, false};
        if constexpr (rational_storage<L> || rational_storage<R> || fp_storage<L> || fp_storage<R> ||
                      !detail::notched<L> || is_integer_mapping || !values_fit_imax<L> || !values_fit_imax<R>)
            return no;
        else {
            constexpr umax cap = static_cast<umax>(std::numeric_limits<imax>::max());
            if (Factor.Numerator > cap || Offset.Numerator > cap)
                return no;
            const imax   f_n = static_cast<imax>(Factor.Numerator); // Factor > 0
            const imax   f_d = abs_den(Factor.Denominator);
            const imax   o_s = signed_numerator(Offset);
            const imax   o_d = abs_den(Offset.Denominator);
            affine_map_t m{0, 0, 0, true};
            if (mul_overflow(f_n, o_d, &m.Mul) || mul_overflow(o_s, f_d, &m.Add) || mul_overflow(o_d, f_d, &m.Den))
                return no;
            // worst-case |numerator| over R's offset range [0, max_index]
            if (max_index_v<R> > cap)
                return no;
            const imax rmax = static_cast<imax>(max_index_v<R>);
            imax       term, num;
            if (mul_overflow(rmax, m.Mul, &term) || add_overflow(term, m.Add < 0 ? -m.Add : m.Add, &num))
                return no;
            // round_quotient equivalence: rounding is reduction-invariant, but
            // its value-index-vs-offset branch CHOICE keys on m·di + num fitting
            // imax — mirror those checks for the unreduced den so both forms
            // take the same branch (ties on negatives differ across branches).
            constexpr auto zl = (detail::lower64<L> / detail::notch64<L>).value_or(rational{0});
            if (abs_den(zl.Denominator) == 1) {
                if (zl.Numerator > cap)
                    return no;
                const imax mbias = signed_numerator(zl);
                imax       mdi, total;
                if (mul_overflow(mbias, m.Den, &mdi) || add_overflow(mdi, num, &total))
                    return no;
            }
            return m;
        }
    }();

    // Map rhs.Raw into L's raw space (requires is_integer_mapping). The
    // Offset/Factor formula assumes offset encoding both sides; for direct
    // storage, subtract detail::lower64<R> first (R-value → R-offset) and add detail::lower64<L>
    // after (raw_from_offset<L>). All integer (is_integer_mapping guarantees it).
    static constexpr imax map_raw(auto rhs_raw) {
        imax r_offset = rhs_raw;
        if constexpr (value_storage<R>)
            r_offset -= raw_lo<R>;

        // Offset is an exact integer here, so trunc(Offset) is a constexpr constant.
        imax l_offset = static_cast<imax>(Factor.Numerator) * r_offset + trunc(Offset);

        if constexpr (value_storage<L>)
            return l_offset + raw_lo<L>;
        else
            return l_offset;
    }

  private:
    template <typename A>
    static constexpr void apply_clamp(L& lhs, const R& rhs, A&& action) {
        // raw_lo/raw_hi are already the correct Raw (no raw_from_offset). Real storage
        // takes the endpoint as a double (raw_lo/Hi truncate fractional dyadic endpoints).
        if constexpr (fp_storage<L>)
            lhs = L::from_raw((as_rational(rhs) < detail::lower64<L>) ? static_cast<double>(detail::lower64<L>)
                                                                      : static_cast<double>(detail::upper64<L>));
        else
            lhs =
                L::from_raw((as_rational(rhs) < detail::lower64<L>) ? raw_cast<L>(raw_lo<L>) : raw_cast<L>(raw_hi<L>));
        // Overshoot (rhs − clamped) as an inside, via the result-grid inference of normal
        // inside arithmetic: both operands are insides, so the overshoot is too. It is always
        // in-grid and on-notch for grid_of<R> − grid_of<L>, so the construction is exact.
        if constexpr (clamp_action<plain_t<A>>) {
            constexpr grid            OG = (grid_of<R> - grid_of<L>).value();
            beman::inside::inside<OG> overshoot{(as_rational(rhs) - as_rational(lhs)).value()};
            action.Fn(lhs, overshoot);
        }
    }

    template <typename P, typename A>
    static constexpr void apply_wrap(L& lhs, const R& rhs, P&& policy, A&& action) {
        // The integer modular wrap (range = Upper - Lower + 1, integer values) is
        // only correct on a unit-integer grid — notch 1 with integer bounds, so
        // consecutive integers are adjacent grid points — and for a source whose
        // values are integers (no rounding to do). Anything else routes through
        // the rational modular wrap, which rounds by the policy first.
        if constexpr (integer_limits<L> && abs_den(detail::notch64<L>.Denominator) == 1 &&
                      detail::notch64<L>.Numerator == 1 && !fp_storage<R> && integer_lattice<R>) {
            // Unit-integer fast path: modular wrap on the integer value, exact
            // (either grid may reach past int64; the span can be 2^64−1).
            using fold             = unit_fold<L, wide_numerator(lower_of<R>), wide_numerator(upper_of<R>)>;
            using W                = typename fold::W;
            const auto [excess, w] = fold::fold(static_cast<W>(wide_numerator(as_rational(rhs))));
            lhs                    = L::from_raw(raw_of_slot<L>(w));
            if constexpr (wrap_action<plain_t<A>>)
                action.Fn(lhs, make_wrap_carry<L, R>(excess)); // carry as an inside
        } else if constexpr (wrap_action<plain_t<A>>) {
            // Fractional destination with a wrap action: reuse the rational modular-wrap
            // path for the store/rounding, but wrap its imax carry `q` into an inside before
            // handing it to the user action.
            assignment<L, rational>::apply_wrap(
                lhs, as_rational(rhs), policy, beman::inside::on_wrap([&](auto& self, imax q) {
                    action.Fn(self, make_wrap_carry<L, R>(q));
                }));
        } else {
            // Fractional destination, no wrap action: delegate unchanged.
            assignment<L, rational>::apply_wrap(lhs, as_rational(rhs), policy, action);
        }
    }

    template <typename P, typename A>
    static constexpr bool try_clamp_or_fail(L& lhs, const R& rhs, P&& policy, A&& action) {
        return dispatch_out_of_range<true>(
            lhs,
            policy,
            action,
            [&] { apply_clamp(lhs, rhs, action); },
            [&] { apply_wrap(lhs, rhs, policy, action); });
    }

    template <typename P>
    static constexpr void store(L& lhs, const R& rhs, P&& policy) {
        if constexpr (fp_storage<L> && (fp_storage<R> || wide_valued<R> || double_exact<grid_of<R>>))
            // f64 target: raw IS the value — decode the source (a double exactly)
            // and snap to the dyadic grid (the offset machinery below mis-encodes
            // a double raw).
            lhs = L::from_raw(snap_double<grid_of<L>, rounding_for<L, P>>(as_double(rhs)));
        else if constexpr (fp_storage<L>)
            // A source that is not a double exactly: round its exact value, not
            // the double nearest to it (two roundings can differ by a notch).
            assignment<L, rational>::store_checked(lhs, as_rational(rhs), policy, no_action{});
        else if constexpr (rational_storage<L> || rational_storage<R>)
            // rational target: raw IS the value — snap the decoded source through
            // the rational-rhs store (the offset machinery below would round the
            // VALUE to a notch index and store that number as the raw). A rational
            // source can lie between L's notches whatever its grid: the same
            // store rounds it by the policy, or reports rounding_error.
            assignment<L, rational>::store_checked(lhs, as_rational(rhs), policy, no_action{});
        else if constexpr (is_integer_mapping) {
            // exact: Factor and Offset have integer denominators, no rounding ambiguity
            if constexpr (Offset == 0 && Factor == 1 && same_storage<L, R>)
                lhs = L::from_raw(raw_cast<L>(rhs.raw()));
            else
                lhs = L::from_raw(raw_cast<L>(map_raw(rhs.raw())));
        } else if constexpr (affine_map.Ok) {
            // Folded non-integer mapping: one multiply-add, then the same
            // round_quotient (invariant under reduction — bit-identical to the
            // rational chain below).
            // Offset/Factor map R's 0-based offset: a value raw counts from Lower.
            const imax r_offset = static_cast<imax>(rhs.raw()) - (index_storage<R> ? imax{0} : raw_lo<R>);
            const imax num      = affine_map.Add + r_offset * affine_map.Mul;
            const umax q =
                round_quotient<L, P>(static_cast<umax>(num < 0 ? -num : num), static_cast<umax>(affine_map.Den));
            lhs = L::from_raw(num < 0 ? raw_from_offset<L>(-static_cast<imax>(q)) : raw_from_offset<L>(q));
        } else {
            // Offset/Factor map R's 0-based offset (a rational raw is the value,
            // which calcOffset/calcFactor already account for).
            const rational r_offset = [&] {
                if constexpr (rational_storage<R> || index_storage<R>)
                    return rational{rhs.raw()};
                else
                    return (rational{rhs.raw()} - detail::lower64<R>).value();
            }();
            rational rat = *(Offset + *(Factor * r_offset));
            umax     ad  = static_cast<umax>(abs_den(rat.Denominator));
            // Round the L-offset to a notch index in VALUE space via round_quotient
            // (same as the scalar path), honouring every rounding mode.
            umax q = round_quotient<L, P>(rat.Numerator, ad);
            // rat is the L-offset; raw_from_offset<L> adds detail::lower64<L> back for direct storage.
            lhs =
                L::from_raw((rat.Denominator < 0) ? raw_from_offset<L>(-static_cast<imax>(q)) : raw_from_offset<L>(q));
        }
    }

  public:
    template <typename P, typename A = no_action>
    static constexpr L& assign(L& lhs, const R& rhs, P&& policy, A&& action = {}) {
        // A wide raw on either side: the exact wide path.
        if constexpr (wide_valued<L> || wide_valued<R>) {
            static_assert(has_policy<L, P, wrap> || has_policy<L, P, clamp> ||
                              not excludes(interval_of<L>, interval_of<R>),
                          "rhs interval lies entirely outside lhs interval and the policy cannot bring it into range");
            static_assert(notches_compatible<L, R> || has_policy<L, P, snap>,
                          "incompatible notches: use with_snap() or policy<snap>() to allow rounding");
            if constexpr (same_notch_raws<L, R>) {
                // Equal notches: the raw maps by a constant shift. Out of range
                // takes the exact path's policy cascade below.
                using W     = work_int_t<same_notch_bits<L, R>>;
                const W raw = static_cast<W>(rhs.raw()) + static_cast<W>(same_notch_shift<L, R>);
                if (raw >= static_cast<W>(raw_lo_exact<L>) && raw <= static_cast<W>(raw_hi_exact<L>)) [[likely]] {
                    lhs = L::from_raw(static_cast<raw_t<L>>(raw));
                    return lhs;
                }
            }
            return assign_exact<R>(lhs, exact_of(rhs), policy, std::forward<A>(action));
        } else
            return assign_builtin(lhs, rhs, policy, std::forward<A>(action));
    }

  private:
    template <typename P, typename A>
    static constexpr L& assign_builtin(L& lhs, const R& rhs, P&& policy, A&& action) {
        // wrap/clamp bring any value into range, so a disjoint rhs interval is fine
        // for them (matches the integral-rhs path); only strict policies reject it.
        static_assert(has_policy<L, P, wrap> || has_policy<L, P, clamp> ||
                          not excludes(interval_of<L>, interval_of<R>),
                      "rhs interval lies entirely outside lhs interval and the policy cannot bring it into range");
        static_assert(notches_compatible<L, R> || has_policy<L, P, snap>,
                      "incompatible notches: use with_snap() or policy<snap>() to allow rounding");

        // A `f64` source holds its value as a double raw, which the raw-mapping
        // formulas below would misread as an index: take the double path.
        if constexpr (fp_storage<R>)
            return assignment<L, double>::assign(lhs, as_double(rhs), policy, std::forward<A>(action));
        else if constexpr (not includes(interval_of<L>, interval_of<R>)) {
            if constexpr (needs_runtime_range_check<L, plain_t<P>, plain_t<A>>) {
                if constexpr (is_integer_mapping) {
                    if (imax mapped = map_raw(rhs.raw()); mapped < raw_lo<L> || mapped > raw_hi<L>)
                        if (try_clamp_or_fail(lhs, rhs, policy, action))
                            return lhs;
                } else if (const rational v = as_rational(rhs); not includes(interval_of<L>, v)) {
                    // Round first: a value just outside may round onto an endpoint.
                    // (The integer mapping above lands on the lattice: nothing to round.)
                    if constexpr (rounds_before_range_check<L, plain_t<P>>)
                        if (const auto rr = raw_if_rounds_inside<L, plain_t<P>>(v); rr.Ok) {
                            lhs = L::from_raw(rr.Raw);
                            return lhs;
                        }
                    if (try_clamp_or_fail(lhs, rhs, policy, action))
                        return lhs;
                }
            }
        }

        store(lhs, rhs, policy);
        return lhs;
    }
};
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_ASSIGNMENT_HPP
