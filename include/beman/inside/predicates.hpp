// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_PREDICATES_HPP
#define BEMAN_INSIDE_PREDICATES_HPP

#include <beman/inside/generic.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy.hpp>
#include <beman/inside/detail/wide_value.hpp>

//---------------------------------------------------------------------------
// predicates — pure inspection (no conversion, no state change) to branch
// before a construction that might throw or report an error:
//   conversion_overflows<B>(v) — v falls outside B's interval.
//   conversion_rounds<B>(v)    — v is in-range but off-notch (would round).
//   conversion_is_lossy<B>(v)  — either of the two.
//---------------------------------------------------------------------------
namespace beman::inside {
namespace detail {
// Whether B's limits and notch are 64-bit rationals (always, without big
// grids): the predicates' 64-bit paths need them.
template <insidable B>
inline constexpr bool predicates_fit64 = fits_rational(grid_of<B>.Interval.Lower) &&
                                         fits_rational(grid_of<B>.Interval.Upper) && fits_rational(grid_of<B>.Notch);

// A double the 64-bit paths cannot take exactly: |d| ≥ 2^64, or finer than
// 2^-62 (as_rational would truncate it).
constexpr bool past_rational(double d) noexcept { return !(d < 0x1p64 && d > -0x1p64) || !abs_fraction_exact(d); }

// What converting the exact value x into B does: overflow (out of range,
// and B's rounding does not bring it in) and rounding (in range off the
// grid, or brought in by B's rounding). For values past the 64-bit paths.
struct lossy_result {
    bool Overflows;
    bool Rounds;
};
template <insidable B, std::size_t K>
constexpr lossy_result exact_lossy(const exact_frac<K>& x) noexcept {
    constexpr std::size_t KK = exact_max<K, exact_limbs<B>>;
    const exact_frac<KK>  v{x};
    const bool            in = !(v < exact_of_grid<KK>(lower_of<B>)) && !(exact_of_grid<KK>(upper_of<B>) < v);
    if constexpr (notched<B>) {
        const auto j = exact_index<B, rounding_for<B, policy<>>>(v);
        if (in)
            return {false, !j.Exact};
        if constexpr (!rounds_before_range_check<B, policy<>>)
            return {true, false};
        else {
            using I         = std::remove_cvref_t<decltype(j.Index)>;
            const bool fits = !j.Index.negative() && !(static_cast<I>(grid_of<B>.slot_count()) < j.Index);
            return {!fits, fits};
        }
    } else {
        constexpr bool bounded = grid_of<B>.MaxDenominator != 0;
        if (in) {
            if constexpr (bounded)
                return {false, static_cast<wide_sint<KK>>(wide_numerator(grid_of<B>.MaxDenominator)) < reduced(v).Den};
            else
                return {false, false};
        }
        if constexpr (!rounds_before_range_check<B, policy<>>)
            return {true, false};
        else {
            const exact_frac<KK> r    = round_to_bound<B, policy<>>(v);
            const bool           fits = !(r < exact_of_grid<KK>(lower_of<B>)) && !(exact_of_grid<KK>(upper_of<B>) < r);
            return {!fits, fits};
        }
    }
}

// exact_lossy for a value, when the 64-bit paths cannot take it.
template <insidable B, numeric A>
constexpr std::optional<lossy_result> exact_lossy_of(A value) noexcept {
    if constexpr (std::floating_point<A>) {
        if (!predicates_fit64<B> || past_rational(static_cast<double>(value)))
            return exact_lossy<B>(exact_of_double(static_cast<double>(value)));
    } else if constexpr (!predicates_fit64<B>)
        return exact_lossy<B>(exact_of(as_rational(value)));
    return std::nullopt;
}
} // namespace detail

template <insidable B, numeric A>
[[nodiscard]] constexpr bool conversion_overflows(A value) noexcept {
    if constexpr (std::floating_point<A>)
        if (!(value - value == 0))
            return true; // NaN / ±inf fit no grid (and must not raise here)
    // The 64-bit paths below do not compile for a grid past them.
    if constexpr (!detail::predicates_fit64<B>)
        return detail::exact_lossy_of<B>(value)->Overflows;
    else {
        if (const auto e = detail::exact_lossy_of<B>(value)) [[unlikely]]
            return e->Overflows;
        const detail::rational r = detail::as_rational(value);
        if (includes(interval_of<B>, r))
            return false;
        // B's policy rounds before it range-checks: a value that rounds onto the
        // grid does not overflow.
        detail::rational rounded;
        return !detail::rounds_into_range<B, policy<>>(r, rounded);
    }
}

template <insidable B, numeric A>
[[nodiscard]] constexpr bool conversion_rounds(A value) noexcept {
    if constexpr (std::floating_point<A>)
        if (!(value - value == 0))
            return false; // non-finite — overflow, not rounding
    // The 64-bit paths below do not compile for a grid past them.
    if constexpr (!detail::predicates_fit64<B>)
        return detail::exact_lossy_of<B>(value)->Rounds;
    else {
        if (const auto e = detail::exact_lossy_of<B>(value)) [[unlikely]]
            return e->Rounds;
        const detail::rational r = detail::as_rational(value);
        if constexpr (!::beman::inside::detail::notched<B>) {
            // Continuous: nothing to miss, unless the grid bounds denominators
            // (an in-range value with a larger one is off the grid).
            if constexpr (grid_of<B>.MaxDenominator == 0)
                return false;
            else {
                if (!includes(interval_of<B>, r)) {
                    // Out of range: rounding onto the bound may bring it in
                    // (a bounded grid rounds before the range check).
                    detail::rational rounded;
                    return detail::rounds_into_range<B, policy<>>(r, rounded);
                }
                return detail::abs_den(r.Denominator) > detail::to_rational(grid_of<B>.MaxDenominator).Numerator;
            }
        } else {
            if (!includes(interval_of<B>, r)) {
                // Out of range: a rounding policy that brings it onto the grid
                // rounds; anything else is overflow, not rounding.
                detail::rational rounded;
                return detail::rounds_into_range<B, policy<>>(r, rounded);
            }
            // In range: rounding occurs iff (value − Lower) / Notch is not an
            // integer — exactly when the 64-bit steps overflow.
            if (const auto d = try_sub(r, ::beman::inside::detail::lower64<B>))
                if (const auto q = try_div(*d, ::beman::inside::detail::notch64<B>))
                    return detail::abs_den(q->Denominator) != 1;
            return !detail::exact_index<B, detail::round_mode::floor>(detail::exact_of(r)).Exact;
        }
    }
}

template <insidable B, numeric A>
[[nodiscard]] constexpr bool conversion_is_lossy(A value) noexcept {
    return conversion_overflows<B>(value) || conversion_rounds<B>(value);
}
} // namespace beman::inside

#endif // BEMAN_INSIDE_PREDICATES_HPP
