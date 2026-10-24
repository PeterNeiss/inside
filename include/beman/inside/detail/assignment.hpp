// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_ASSIGNMENT_HPP
#define BEMAN_INSIDE_DETAIL_ASSIGNMENT_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <beman/inside/detail/assign_exact.hpp> // the exact store and the out-of-range cascade
#include <beman/inside/generic.hpp>
#include <beman/inside/detail/wide_value.hpp>
#include <beman/inside/grid.hpp>

#include <bit>

namespace beman::inside::detail {
//---------------------------------------------------------------------------
// assignment — narrowing/coercion between bounded and arithmetic types. Three
// specializations dispatch on the source (integral / fractional / insidable);
// each stores an in-range value and hands an out-of-range one to the policy
// (`apply_clamp` / `apply_wrap`, or a report). The insidable path also exposes
// `is_integer_mapping` / `map_raw` — a pure-integer formula in the hot path.
//---------------------------------------------------------------------------

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
            action.Fn(lhs, saturate_imax(v - (low ? fold::lower : fold::upper)));
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
            lhs = L::from_raw(0); // a point grid: 0 is the only offset
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
    if constexpr (anchored<L> || point_storage<L> || !values_fit_imax<L>) // unanchored ⇒ index raw
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
            return past_64_bits(rhs);
        else
            return false;
    }

    // (Out of line, by value, for a double: its exact comparison has a cold
    // tail whose call would otherwise cost the inlined store registers.)
    BEMAN_INSIDE_NOINLINE static constexpr bool below_lower(R rhs)
        requires std::floating_point<R>
    {
        return huge(rhs) ? rhs < 0 : rhs < detail::lower64<L>;
    }
    static constexpr bool below_lower(const R& rhs)
        requires(!std::floating_point<R>)
    {
        return rhs < detail::lower64<L>;
    }

    template <typename P, typename A>
    static constexpr void apply_clamp(L& lhs, R rhs, P&&, A&& action) {
        const bool low   = below_lower(rhs);
        R          bound = low ? static_cast<R>(detail::lower64<L>) : static_cast<R>(detail::upper64<L>);
        R          overshoot;
        if constexpr (std::same_as<R, rational>)
            overshoot = try_sub(rhs, bound).value_or(rational{0});
        else
            overshoot = rhs - bound;

        // The clamp target is an interval endpoint — a grid point, no rounding.
        lhs = L::from_raw(endpoint_raw<L>(low));

        if constexpr (clamp_action<plain_t<A>>)
            action.Fn(lhs, overshoot);
    }

  public:
    // Exposed (not private) so the insidable-rhs wrap path can reuse the
    // rational specialization's modular wrap on fractional/notch grids, and
    // so the wrap path can reuse store_checked after computing the wrapped
    // value.
    //
    // apply_wrap for a fractional R — rounding onto the lattice, then modular
    // reduction into [Lower, Lower + range), range = Upper - Lower + Notch.
    template <typename P, typename A>
    static constexpr void apply_wrap(L& lhs, R rhs, P&& policy, A&& action) {
        // Round onto the lattice first (by the policy, like every other store),
        // then fold: an on-lattice value folds onto a grid point, so rounding
        // can never carry it past Upper.
        // A floating source of 2^63 or more has no rational form: it folds
        // exactly, like every value whose rational steps overflow below.
        if constexpr (std::floating_point<R>)
            if (!(rhs < 0x1p63 && rhs > -0x1p63)) [[unlikely]]
                return wrap_exact(lhs, exact_of_double(static_cast<double>(rhs)), policy, action);
        rational rhs_r{rhs};
        if constexpr (!notched<L> && grid_of<L>.MaxDenominator != 0 && has_policy<L, P, snap>) {
            // A bounded continuous grid rounds onto its bound first, too: a
            // fold keeps the denominator, so the folded value stays on it.
            const auto r = try_rational(round_to_bound<L, plain_t<P>>(exact_of(rhs_r)));
            if (!r) [[unlikely]]
                return wrap_exact(lhs, exact_of(rhs_r), policy, action);
            rhs_r = *r;
        } else if constexpr (has_policy<L, P, snap>) {
            const auto r = try_round_to_lattice<L, P>(rhs_r);
            if (!r) [[unlikely]]
                return wrap_exact(lhs, exact_of(rhs_r), policy, action);
            rhs_r = *r;
        }
        imax     q;
        rational folded;
        if (abs_den(rhs_r.Denominator) == 1 && detail::notch64<L> == 1 &&
            abs_den(detail::lower64<L>.Denominator) == 1) {
            // Integer value on a unit lattice: fold exactly (the grid may span
            // 2^64 values, past the rational range).
            using fold         = unit_fold<L, source_lo<rational>, source_hi<rational>>;
            using W            = typename fold::W;
            const auto [qq, w] = fold::fold(static_cast<W>(wide_numerator(rhs_r)));
            const W v          = fold::lower + w;
            q                  = qq;
            folded             = v < W{0} ? -rational{static_cast<umax>(-v)} : rational{static_cast<umax>(v)};
        } else {
            // q = floor((rhs - lower) / range), wrapped = rhs - q * range. A step
            // past the 64-bit rational range, or a fold count past imax, folds
            // exactly instead.
            const auto span  = try_sub(detail::upper64<L>, detail::lower64<L>);
            const auto range = span ? try_add(*span, detail::notch64<L>) : span;
            const auto off   = try_sub(rhs_r, detail::lower64<L>);
            const auto quot  = (range && off) ? try_div(*off, *range) : off;
            if (!range || !quot || *quot < rational{std::numeric_limits<imax>::min()} ||
                *quot > rational{std::numeric_limits<imax>::max()}) [[unlikely]]
                return wrap_exact(lhs, exact_of(rhs_r), policy, action);
            q             = floor(*quot);
            const auto qr = try_mul(rational{q}, *range);
            const auto w  = qr ? try_sub(rhs_r, *qr) : qr;
            if (!w) [[unlikely]]
                return wrap_exact(lhs, exact_of(rhs_r), policy, action);
            folded = *w;
        }

        // Re-enter the rational-rhs specialization for the actual store so the
        // notch / rounding policy logic is exercised once.
        assignment<L, rational>::store_checked(lhs, folded, policy, action);

        if constexpr (wrap_action<plain_t<A>>)
            action.Fn(lhs, make_wrap_carry<L, R>(q));
    }

    // The fold done exactly when its rational steps overflow (a double past
    // 2^63, a far value on a fine grid): the exact store's wrap, which rounds
    // by the policy and folds the wide lattice index (a wrap action's carry
    // saturates, as on the wide paths). A continuous L has no lattice to
    // fold in wide integers, and a point no period: still overflow.
    template <typename P, typename A, std::size_t K>
    BEMAN_INSIDE_COLD BEMAN_INSIDE_NOINLINE static constexpr void
    wrap_exact(L& lhs, const wide_rational<K>& v, P&& policy, A&& action) {
        if constexpr (integer_storage<L> && !point_storage<L>)
            assign_exact<R>(lhs, v, policy, action);
        else
            report_failure(lhs, policy, action, errc::overflow);
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
    BEMAN_INSIDE_COLD BEMAN_INSIDE_NOINLINE static constexpr wide_slot_result wide_slot(const rational& v) {
        const exact_index_result r = exact_index<L, rounding_for<L, P>>(exact_of(v));
        return {static_cast<umax>(r.Index), r.Exact}; // in range: fits 64 bits
    }

    // The source as a rational, converted once. A double finer than 2^-62
    // has no exact 64-bit rational (rational{d} truncates it toward zero):
    // `exact` comes back false, and the store takes its exact value through
    // store_fine_double, so it rounds, or reports rounding_error, by its true
    // value.
    static constexpr rational source_of(const R& rhs, bool& exact) {
        if constexpr (std::floating_point<R>)
            return rational_of_double(static_cast<double>(rhs), exact);
        else
            return rational{rhs};
    }
    // fine_double_value's result, sized to return in registers (a larger
    // one goes through memory, and the stack protector into the hot path):
    // a rational raw marks a failure by a denominator no value has (0), with
    // the errc as numerator; an integer raw carries the errc beside it.
    struct fine_result {
        raw_t<L>       Raw;
        errc           Code;
        constexpr errc code() const { return Code; }
    };
    struct fine_rational_result {
        rational       Raw;
        constexpr errc code() const { return Raw.Denominator == 0 ? static_cast<errc>(Raw.Numerator) : errc{}; }
    };
    using fine_result_t = std::conditional_t<rational_storage<L>, fine_rational_result, fine_result>;

    // P's flags, recording its first failure into an errc instead.
    template <typename P>
    using recording_policy = ::beman::inside::policy<P::Flags, error_ref>;

    // Out of line, and by value (an empty action too): a reference to a
    // caller's local would make it escape the hot path's frame. lhs may be
    // uninitialized (a trivial default ctor), so the store runs on a copy of
    // the lower limit, and a failure is returned, not stored: a failed store
    // leaves lhs intact. The policy hears of it here; an on_error action,
    // which gets lhs, from store_fine_double.
    template <typename P, typename A>
    BEMAN_INSIDE_COLD BEMAN_INSIDE_NOINLINE static constexpr fine_result_t
    fine_double_value(double rhs, P policy, A action) {
        L r = [] {
            if constexpr (rational_storage<L>)
                return L::from_raw(detail::lower64<L>);
            else if constexpr (point_storage<L>)
                return L::from_raw(raw_t<L>{});
            else
                return L::from_raw(static_cast<raw_t<L>>(raw_lo_exact<L>));
        }();
        errc code{};
        // A recording on_error stands in for one (dispatch_out_of_range ranks
        // it below clamp and wrap): the order holds.
        if constexpr (error_action<plain_t<A>>)
            assign_exact<R>(r, exact_of_double(rhs), recording_policy<P>{code}, on_error([&code](L&, errc c, auto&&) {
                                code = c;
                            }));
        else {
            assign_exact<R>(r, exact_of_double(rhs), recording_policy<P>{code}, std::forward<A>(action));
            if (code != errc{})
                policy.report(code);
        }
        if constexpr (rational_storage<L>) {
            if (code == errc{})
                return {r.raw()};
            rational failed{};
            failed.Numerator   = static_cast<umax>(code);
            failed.Denominator = 0;
            return {failed};
        } else
            return {r.raw(), code};
    }
    // Whether a failure returns (errc&, on_error): else it raised, and lhs
    // takes the result unconditionally — no branch on the hot path's tail.
    template <typename P, typename A>
    static constexpr bool fine_failure_returns = std::is_same_v<P, recording_policy<P>> || error_action<A>;
    template <typename P, typename A>
    static constexpr void store_fine_double(L& lhs, double rhs, P&& policy, A&& action) {
        using Act    = std::conditional_t<std::is_empty_v<plain_t<A>>, plain_t<A>, A&&>;
        const auto r = fine_double_value<plain_t<P>, Act>(rhs, policy, std::forward<A>(action));
        if constexpr (!fine_failure_returns<plain_t<P>, plain_t<A>>)
            lhs = L::from_raw(r.Raw);
        else if (r.code() == errc{}) [[likely]]
            lhs = L::from_raw(r.Raw);
        else if constexpr (error_action<plain_t<A>>)
            action.Fn(lhs, r.code(), errc_message(r.code()));
    }

    template <typename P, typename A = no_action>
    static constexpr void store_checked(L& lhs, R rhs, P&& policy, A&& action = {}) {
        if constexpr (rational_storage<L>) {
            bool           exact = true;
            const rational rv    = source_of(rhs, exact);
            if constexpr (std::floating_point<R>)
                if (!exact) [[unlikely]]
                    return store_fine_double(lhs, static_cast<double>(rhs), policy, action);
            // Continuous: store verbatim. A bounded grid's values have
            // denominators ≤ its bound: one past it is off the grid — rounded
            // onto it under a rounding policy, else reported (but unsafe).
            if constexpr (grid_of<L>.MaxDenominator != 0)
                if (abs_den(rv.Denominator) > to_rational(grid_of<L>.MaxDenominator).Numerator) [[unlikely]] {
                    if constexpr (has_policy<L, P, snap>) {
                        const auto o = onto_bound<L, plain_t<P>>(exact_of(rv));
                        const auto r = o ? try_rational(*o)
                                         : std::expected<rational, errc>{std::unexpected{errc::rounding_error}};
                        if (!r)
                            return report_failure(lhs, policy, action, errc::rounding_error);
                        lhs = L::from_raw(*r);
                        return;
                    } else if (std::is_constant_evaluated() || is_checked(flags_of<L> | plain_t<P>::Flags))
                        return report_failure(lhs, policy, action, errc::rounding_error);
                }
            lhs = L::from_raw(rv);
        } else if constexpr (detail::point_grid<L>)
            lhs = L::from_raw({}); // a point: its value is the type
        else {
            // Store the k-th notch slot: raw_from_offset<L> covers index and
            // value raws.
            auto store_slot = [&](auto k) { lhs = L::from_raw(raw_from_offset<L>(k)); };

            constexpr bool has_round_flag = has_policy<L, P, snap>; // every round_* flag contains snap
            // The slot onum/den when it is exact or P rounds; false otherwise
            // (strict policy, off-notch: the rational path below reports it).
            auto try_slot = [&](umax onum, umax den) {
                if (onum % den == 0) {
                    store_slot(onum / den);
                    return true;
                }
                if constexpr (has_round_flag) {
                    store_slot(round_quotient<L, P>(onum, den));
                    return true;
                }
                return false;
            };

            // A floating source on an anchored grid with a power-of-two notch
            // whose values double holds: v/Notch is exact, so round it in
            // double (the integer storage's rule) and read the slot off the
            // value index. No rational round trip.
            if constexpr (std::floating_point<R> && anchored<L> && double_exact<grid_of<L>> &&
                          std::has_single_bit(detail::notch64<L>.Numerator)) {
                constexpr imax lo = signed_numerator((detail::lower64<L> / detail::notch64<L>).value());
                const double   v  = static_cast<double>(rhs);
                const imax     j  = snap_double_index<grid_of<L>, rounding_for<L, P>>(v);
                if constexpr (!has_round_flag)
                    if (static_cast<double>(j) * static_cast<double>(detail::notch64<L>) != v && policy.round_check())
                        [[unlikely]]
                        return report_failure(lhs, policy, action, errc::rounding_error);
                store_slot(static_cast<umax>(j - lo));
                return;
            }
            bool           exact = true;
            const rational rv    = source_of(rhs, exact);
            if constexpr (std::floating_point<R>)
                if (!exact) [[unlikely]]
                    return store_fine_double(lhs, static_cast<double>(rhs), policy, action);

            // Q-format integer shortcut: with integer Lower and notch 1/K the offset is
            // (num − Lo·aden)·K / aden — integer ops instead of two rational ops.
            // round_quotient is invariant under reduction, so the slot is
            // bit-identical to the rational path. Oversized denominators
            // fall through (the max_den guard keeps every product inside imax).
            if constexpr (qformat_codec_fits<L> && detail::notched<L>) {
                constexpr imax K      = abs_den(detail::notch64<L>.Denominator);
                constexpr imax Lo     = lower_imax<L>;
                constexpr umax two_km = [] {
                    // 2 · K · M with saturation (M bounds |value| and the offset span)
                    // ceil in umax: a bound's magnitude may reach 2^63 (past imax)
                    constexpr umax umax_hi = std::numeric_limits<umax>::max();
                    const rational a       = detail::abs(detail::lower64<L>) > detail::abs(detail::upper64<L>)
                                                 ? detail::abs(detail::lower64<L>)
                                                 : detail::abs(detail::upper64<L>);
                    const umax     d       = abs_den(a.Denominator);
                    const umax     c       = a.Numerator / d + (a.Numerator % d != 0 ? 1 : 0);
                    if (c > (umax_hi - 2) / 2)
                        return umax_hi;
                    umax k = static_cast<umax>(K);
                    umax m = c * 2 + 2;
                    if (k > umax_hi / m)
                        return umax_hi;
                    umax prod = k * m;
                    return prod > umax_hi / 2 ? umax_hi : prod * 2;
                }();
                constexpr umax max_den = static_cast<umax>(std::numeric_limits<imax>::max()) / two_km;

                const umax aden = abs_den(rv.Denominator);
                if (max_den != 0 && aden <= max_den) {
                    // The offset (num − Lo·aden)·K / aden, unreduced: round_quotient
                    // rounds it as the reduced fraction would, and max_den keeps the
                    // product in imax.
                    const imax num  = signed_numerator(rv);
                    const umax onum = // ≥ 0: rhs ≥ Lower (in range)
                        static_cast<umax>((num - Lo * static_cast<imax>(aden)) * K);
                    if (try_slot(onum, aden))
                        return;
                }
            }

            // The same shortcut for an unanchored lattice: with K the common
            // denominator of Notch and Lower, Notch = sN/K and Lower = m/K, the
            // offset is (num·K − m·aden)/(aden·sN).
            if constexpr (unanchored_codec<L>.Ok) {
                constexpr auto c    = unanchored_codec<L>;
                const umax     aden = abs_den(rv.Denominator);
                if (aden <= c.MaxDen) {
                    // Unreduced: MaxDen bounds these products already, and the
                    // unanchored rounding and the exactness test do not change
                    // under reduction.
                    const imax num  = signed_numerator(rv);
                    const umax onum = // ≥ 0: rhs ≥ Lower (in range)
                        static_cast<umax>(num * c.K - c.M * static_cast<imax>(aden));
                    const umax den2 = aden * static_cast<umax>(c.S);
                    if (try_slot(onum, den2))
                        return;
                }
            }

            // The exact quotient can overflow the 64-bit rational range (huge
            // source denominator × fine notch): take the slot from the exact wide
            // index instead (rhs is in range, so it fits L's raw). Rounding is
            // the value-space rule, as everywhere else.
            const auto rel      = try_sub(rv, detail::lower64<L>);
            const auto quotient = rel ? try_div(*rel, detail::notch64<L>) : rel;
            if (!quotient.has_value()) [[unlikely]] {
                const wide_slot_result slot = wide_slot<P>(rv);
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
    // (A limit near 2^64 rounds to a double of 2^64, which no rational holds:
    // it is no exact double.)
    static constexpr bool exact_double(const rational& r) {
        const double d = static_cast<double>(r);
        return !past_64_bits(d) && rational{d} == r;
    }
    static constexpr bool double_bounds_exact =
        std::floating_point<R> && exact_double(detail::lower64<L>) && exact_double(detail::upper64<L>);

    // A rational source on integer endpoints compares by multiplying the
    // endpoint by the denominator (n/d ≤ m ⇔ n ≤ m·d; an overflowing m·d
    // exceeds any n) — exact, and no division.
    static constexpr bool integer_bounds = std::same_as<R, rational> && abs_den(detail::lower64<L>.Denominator) == 1 &&
                                           abs_den(detail::upper64<L>.Denominator) == 1 && fits_imax(interval_of<L>);

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
        } else if constexpr (std::floating_point<R>) {
            // A limit no double holds (INT64_MAX): compare exactly, without
            // grid numbers, whose runtime ops stop at 64 bits under C++26.
            // Below 2^64 a rational holds the value unless it is finer than 2^-62.
            const double d = static_cast<double>(rhs);
            if (abs_fraction_exact(d)) [[likely]] {
                const rational r{d};
                return r < detail::lower64<L> || detail::upper64<L> < r;
            }
            const auto v = exact_of_double(d);
            return v < exact_of(detail::lower64<L>) || exact_of(detail::upper64<L>) < v;
        } else
            return not includes(interval_of<L>, rhs);
    }

  public:
    template <typename P, typename A = no_action>
    static constexpr L& assign(L& lhs, const R& rhs, P&& policy, A&& action = {}) {
        // A long double that is a double stores as that double; one with bits
        // past it stores exactly (the double paths would drop them).
        if constexpr (wide_float<R>) {
            const double d = static_cast<double>(rhs);
            if (is_finite(rhs) && static_cast<R>(d) != rhs) [[unlikely]] {
                // Past the double range: a stand-in past every limit, which
                // clamps or overflows, but a fold needs the exact value.
                if constexpr (has_policy<L, P, wrap>)
                    if (!is_finite(d)) {
                        report_failure(lhs, policy, action, errc::overflow);
                        return lhs;
                    }
                return assign_exact<R>(lhs, exact_of_long_double(rhs), policy, std::forward<A>(action));
            }
            return assignment<L, double>::assign(lhs, d, policy, std::forward<A>(action));
        } else if constexpr (wide_valued<L>) {
            // NaN / ±inf first, as below; a finite |rhs| ≥ 2^64 is an integer,
            // taken exactly (or by its side, past the grid: exact_of_large).
            if constexpr (std::floating_point<R>)
                if (!is_finite(rhs) || huge(rhs)) [[unlikely]] {
                    if (rhs != rhs) {
                        report_failure(lhs, policy, action, errc::not_finite);
                        return lhs;
                    }
                    if (!is_finite(rhs) && !has_policy<L, P, clamp>) {
                        report_failure(lhs, policy, action, errc::not_finite);
                        return lhs;
                    }
                    return assign_exact<R>(
                        lhs, exact_of_large<L>(static_cast<double>(rhs)), policy, std::forward<A>(action));
                }
            if constexpr (std::floating_point<R>)
                if (!abs_fraction_exact(static_cast<double>(rhs))) [[unlikely]] // finer than 2^-62
                    return assign_exact<R>(
                        lhs, exact_of_double(static_cast<double>(rhs)), policy, std::forward<A>(action));
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
            if (!is_finite(rhs)) [[unlikely]] {
                if constexpr (has_policy<L, P, clamp>)
                    if (rhs == rhs) {
                        // Saturated like any value past a limit: an on_clamp
                        // hears of it, with an overshoot of ±inf.
                        apply_clamp(lhs, rhs, policy, action);
                        return lhs;
                    }
                report_failure(lhs, policy, action, errc::not_finite);
                return lhs;
            }

        if (out_of_interval(rhs)) [[unlikely]] {
            if constexpr (std::floating_point<R>)
                if (!abs_fraction_exact(static_cast<double>(rhs))) {
                    store_fine_double(lhs, static_cast<double>(rhs), policy, action);
                    return lhs;
                }
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
    // Offset/Factor map rhs.Raw → lhs.Raw via `lhs.Raw = Factor·rhs.Raw + Offset`
    // between two integer raws. A continuous side stores through the rational
    // path instead (both 0, unused), and a point L holds one value (both 0).
    static constexpr bool integer_raws = integer_storage<L> && integer_storage<R>;

    static constexpr std::expected<rational, errc> calc_offset() {
        if constexpr (!integer_raws || !detail::notched<L>)
            return rational{0};
        else
            return try_sub(detail::lower64<R>, detail::lower64<L>).and_then([](const rational& d) {
                return try_div(d, detail::notch64<L>);
            });
    }

    static constexpr std::expected<rational, errc> calc_factor() {
        if constexpr (!integer_raws || !detail::notched<L> || point_storage<R>)
            return rational{0}; // a point R's raw is always 0: the mapping is Offset alone
        else
            return try_div(detail::notch64<R>, detail::notch64<L>);
    }

    // A mapping past the 64-bit rational (a full-int64 source into a fine
    // grid) stores through the rational path, as a continuous side does.
    static constexpr bool maps_raws = integer_raws && calc_offset().has_value() && calc_factor().has_value();

  public:
    static constexpr rational Offset = calc_offset().value_or(rational{0});
    static constexpr rational Factor = calc_factor().value_or(rational{0});

    // Raw-space integer-only mapping — requires integer raw storage on both
    // sides (not rational).
    // It also needs every raw and every mapped raw in imax: map_raw's L-raw
    // range is [Offset, Offset + Factor·max_index<R>] (+ Lower for value storage).
    static constexpr bool is_integer_mapping = [] {
        if constexpr (!maps_raws || abs_den(Factor.Denominator) != 1 || abs_den(Offset.Denominator) != 1 ||
                      !values_fit_imax<L> || !values_fit_imax<R>)
            return false;
        else {
            const rational base = index_storage<L> ? rational{0} : detail::lower64<L>;
            const auto     lo   = Offset + base;
            const auto     span = Factor * rational{max_index_of<R>};
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
        if constexpr (!maps_raws || !detail::notched<L> || is_integer_mapping || !values_fit_imax<L> ||
                      !values_fit_imax<R>)
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
            if (max_index_of<R> > cap)
                return no;
            const imax rmax = static_cast<imax>(max_index_of<R>);
            imax       term, num;
            if (mul_overflow(rmax, m.Mul, &term) || add_overflow(term, m.Add < 0 ? -m.Add : m.Add, &num))
                return no;
            // round_quotient equivalence: rounding is reduction-invariant, but
            // its value-index-vs-offset branch CHOICE keys on m·di + num fitting
            // imax — mirror those checks for the unreduced den so both forms
            // take the same branch (ties on negatives differ across branches).
            constexpr auto zl = try_div(detail::lower64<L>, detail::notch64<L>).value_or(rational{0});
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

    // L's raw for R's offset 0: Offset + raw_lo<L>, which is_integer_mapping
    // proves fits imax (Offset alone may not, e.g. 2^63 into a full int64).
    static constexpr imax map_bias = [] {
        if constexpr (is_integer_mapping)
            return signed_numerator((Offset + (index_storage<L> ? rational{0} : detail::lower64<L>)).value());
        else
            return imax{0};
    }();

    // Map rhs.Raw into L's raw space (requires is_integer_mapping): Factor
    // scales R's 0-based offset, map_bias adds L's raw for offset 0. Every
    // mapped raw fits imax, so the wrapping umax multiply-add is exact even
    // where an intermediate (the offset, or a signed partial sum) would not be.
    static constexpr imax map_raw(auto rhs_raw) {
        const umax rel = static_cast<umax>(static_cast<imax>(rhs_raw)) - static_cast<umax>(raw_lo<R>);
        return static_cast<imax>(static_cast<umax>(map_bias) + static_cast<umax>(Factor.Numerator) * rel);
    }

  private:
    template <typename A>
    static constexpr void apply_clamp(L& lhs, const R& rhs, A&& action) {
        lhs = L::from_raw(endpoint_raw<L>(as_rational(rhs) < detail::lower64<L>));
        // Overshoot (rhs − clamped) as an inside, via the result-grid inference of normal
        // inside arithmetic: both operands are insides, so the overshoot is too. It is always
        // in-grid and on-notch for grid_of<R> − grid_of<L>, so the construction is exact.
        if constexpr (clamp_action<plain_t<A>> && !overshoot_grid_ok<L, R>)
            static_assert(dependent_false<A>,
                          "on_clamp: the overshoot grid (source minus target) passes the 64-bit grid numbers");
        else if constexpr (clamp_action<plain_t<A>>) {
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
        if constexpr (unit_lattice(grid_of<L>) && integer_lattice<R>) {
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

    // R's value outside L's interval: for integer raws an integer compare of
    // counts in a common unit against L's endpoints, else the exact value.
    static constexpr bool out_of_interval(const R& rhs) {
        if constexpr (units_cmp_fits<L, R>) {
            constexpr grid_rational U  = grid_gcd_of(unit_of<L>, unit_of<R>);
            constexpr imax          lo = static_cast<imax>(units_lo<L, U>), hi = static_cast<imax>(units_hi<L, U>);
            const imax              c = value_in_units<imax, U>(rhs);
            return c < lo || c > hi;
        } else
            return not includes(interval_of<L>, as_rational(rhs));
    }

    // (The value is in range here: of the actions only on_error can fire.)
    template <typename P, typename A>
    static constexpr void store(L& lhs, const R& rhs, P&& policy, A&& action) {
        if constexpr (!maps_raws)
            // A continuous side or no raw mapping: store the source's value through
            // the rational store, which rounds it by the policy or reports rounding_error.
            assignment<L, rational>::store_checked(lhs, as_rational(rhs), policy, action);
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
            const imax r_offset = static_cast<imax>(rhs.raw()) - raw_lo<R>; // 0 for an index raw
            const imax num      = affine_map.Add + r_offset * affine_map.Mul;
            const umax q =
                round_quotient<L, P>(static_cast<umax>(num < 0 ? -num : num), static_cast<umax>(affine_map.Den));
            lhs = L::from_raw(num < 0 ? raw_from_offset<L>(-static_cast<imax>(q)) : raw_from_offset<L>(q));
        } else {
            // Offset/Factor map R's 0-based offset (a value raw counts from Lower).
            // A step past the 64-bit rational takes the exact wide store.
            const auto r_offset = [&] {
                if constexpr (point_storage<R>) // a point's raw holds nothing: the mapping is Offset alone
                    return std::expected<rational, errc>{rational{0}};
                else if constexpr (raw_lo<R> == 0)
                    return std::expected<rational, errc>{rational{rhs.raw()}};
                else
                    return try_sub(rational{rhs.raw()}, detail::lower64<R>);
            }();
            const auto scaled = r_offset ? try_mul(Factor, *r_offset) : r_offset;
            const auto mapped = scaled ? try_add(Offset, *scaled) : scaled;
            if (!mapped) [[unlikely]] {
                assign_exact<R>(lhs, exact_of(rhs), policy, action);
                return;
            }
            const rational rat = *mapped;
            umax           ad  = static_cast<umax>(abs_den(rat.Denominator));
            // Round the L-offset to a notch index in VALUE space via round_quotient
            // (same as the scalar path), honouring every rounding mode.
            umax q = round_quotient<L, P>(rat.Numerator, ad);
            // rat is the L-offset; raw_from_offset<L> adds detail::lower64<L> back for value storage.
            lhs =
                L::from_raw((rat.Denominator < 0) ? raw_from_offset<L>(-static_cast<imax>(q)) : raw_from_offset<L>(q));
        }
    }

  public:
    template <typename P, typename A = no_action>
    static constexpr L& assign(L& lhs, const R& rhs, P&& policy, A&& action = {}) {
        // A wide raw on either side: the exact wide path.
        if constexpr (wide_valued<L> || wide_valued<R>) {
            static_checks<P>();
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
    // wrap/clamp bring any value into range, so a disjoint rhs interval is fine
    // for them (matches the integral-rhs path); only strict policies reject it.
    template <typename P>
    static constexpr void static_checks() {
        static_assert(has_policy<L, P, wrap> || has_policy<L, P, clamp> ||
                          not excludes(interval_of<L>, interval_of<R>),
                      "rhs interval lies entirely outside lhs interval and the policy cannot bring it into range");
        static_assert(notches_compatible<L, R> || has_policy<L, P, snap>,
                      "incompatible notches: use with_snap() or policy<snap>() to allow rounding");
    }

    template <typename P, typename A>
    static constexpr L& assign_builtin(L& lhs, const R& rhs, P&& policy, A&& action) {
        static_checks<P>();

        if constexpr (not includes(interval_of<L>, interval_of<R>)) {
            if constexpr (needs_runtime_range_check<L, plain_t<P>, plain_t<A>>) {
                // (A point L maps every raw onto its one raw: only the value
                // tells whether it is in range.)
                if constexpr (is_integer_mapping && !point_storage<L>) {
                    if (imax mapped = map_raw(rhs.raw()); mapped < raw_lo<L> || mapped > raw_hi<L>)
                        if (try_clamp_or_fail(lhs, rhs, policy, action))
                            return lhs;
                } else if (out_of_interval(rhs)) {
                    // Round first: a value just outside may round onto an endpoint.
                    // (The integer mapping above lands on the lattice: nothing to round.)
                    if constexpr (rounds_before_range_check<L, plain_t<P>>)
                        if (const auto rr = raw_if_rounds_inside<L, plain_t<P>>(as_rational(rhs)); rr.Ok) {
                            lhs = L::from_raw(rr.Raw);
                            return lhs;
                        }
                    if (try_clamp_or_fail(lhs, rhs, policy, action))
                        return lhs;
                }
            }
        }

        store(lhs, rhs, policy, action);
        return lhs;
    }
};
} // namespace beman::inside::detail

BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_DETAIL_ASSIGNMENT_HPP
