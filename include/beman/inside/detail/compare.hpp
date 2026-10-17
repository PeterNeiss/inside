// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Internal — include "beman/inside/inside.hpp" (the umbrella), not this directly.
#ifndef BEMAN_INSIDE_DETAIL_COMPARE_HPP
#define BEMAN_INSIDE_DETAIL_COMPARE_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <beman/inside/core.hpp>

#include <compare>
#include <limits>

//---------------------------------------------------------------------------
// Comparisons of insides: across grids (by value index when both fit imax,
// else exactly) and against scalars (integers, floating points of any width,
// rationals).
//---------------------------------------------------------------------------
namespace beman::inside {
//---------------------------------------------------------------------------
// comparison
//---------------------------------------------------------------------------
namespace detail {
// Integer value-index comparison eligibility: an integer-backed anchored
// inside whose value indices (value/Notch, integers) fit imax, so two
// same-notch insides compare as `bias + raw` without a rational decode.
template <insidable B>
inline constexpr bool index_cmp_fits = [] {
    if constexpr (!detail::notched<B> || !detail::anchored<B> || !values_fit_imax<B>)
        return false;
    else {
        constexpr auto lo  = detail::lower64<B> / detail::notch64<B>;
        constexpr auto hi  = detail::upper64<B> / detail::notch64<B>;
        constexpr umax cap = static_cast<umax>(std::numeric_limits<imax>::max());
        return lo.has_value() && hi.has_value() && (*lo).Numerator <= cap && (*hi).Numerator <= cap;
    }
}();

// Signed value index of Raw == 0: Lower/Notch for offset (index) storage,
// 0 for a value raw (raw is already the value == the index at notch 1).
template <insidable B>
inline constexpr imax index_cmp_bias = [] {
    if constexpr (index_storage<B>) {
        constexpr auto lo = *(detail::lower64<B> / detail::notch64<B>);
        return signed_numerator(lo);
    } else
        return imax{0};
}();
} // namespace detail

namespace detail {
// Inside values are finite and exact, so they are always ordered: <=> of two
// insides, or of an inside and an integer, is a strong_ordering whatever the
// storage; against a floating scalar it is a partial_ordering (NaN is
// unordered), three_way_partial.
inline constexpr auto three_way         = [](const auto& a, const auto& b) -> std::strong_ordering { return a <=> b; };
inline constexpr auto three_way_partial = [](const auto& a, const auto& b) -> std::partial_ordering {
    return a <=> b;
};
inline constexpr auto equal_to = [](const auto& a, const auto& b) { return a == b; };

// A point P that is one of X's slots, with X storing a builtin index: X's
// raw orders like its value, so X ⋈ P is its raw ⋈ that constant slot
// (`t != end(t)`, `x <= 4_ins`).
template <insidable X, insidable P>
inline constexpr bool point_is_slot =
    integer_index_storage<X> && !point_grid<X> && point_grid<P> && grid_of<X>.representable(lower_of<P>);
template <insidable X, insidable P>
inline constexpr raw_t<X> point_slot_of =
    static_cast<raw_t<X>>(exact_quotient(grid_sub(lower_of<P>, lower_of<X>), notch_of<X>));

// inside ⋈ inside (⋈ = `cmp`: <=> or ==) in the cheapest exact form the two
// storage shapes allow.
template <insidable L, insidable R, class Cmp>
constexpr auto compare(const L& lhs, const R& rhs, Cmp cmp) {
    // same grid, so the same encoding: Raw is monotonically ordered and comparable
    if constexpr (grid_of<L> == grid_of<R>)
        return cmp(lhs.raw(), rhs.raw());
    else if constexpr (point_is_slot<L, R>)
        return cmp(lhs.raw(), point_slot_of<L, R>);
    else if constexpr (point_is_slot<R, L>)
        return cmp(point_slot_of<R, L>, rhs.raw());
    // a wide-index operand: exact wide fractions
    else if constexpr (wide_valued<L> || wide_valued<R>)
        return cmp(exact_of(lhs), exact_of(rhs));
    // integer raws: both values as exact counts of a unit dividing them both
    // (the gcd of their value units), one integer compare — any notches and
    // offsets, without the rational decode.
    else if constexpr (units_cmp_fits<L, R>) {
        constexpr grid_rational U = grid_gcd_of(unit_of<L>, unit_of<R>);
        return cmp(value_in_units<imax, U>(lhs), value_in_units<imax, U>(rhs));
    } else
        return cmp(as_rational(lhs), as_rational(rhs));
}
} // namespace detail

template <insidable L, insidable R>
[[nodiscard]] constexpr auto operator<=>(const L& lhs, const R& rhs) {
    return detail::compare(lhs, rhs, detail::three_way);
}

template <insidable L, insidable R>
[[nodiscard]] constexpr bool operator==(const L& lhs, const R& rhs) {
    return detail::compare(lhs, rhs, detail::equal_to);
}

namespace detail {
// inside ⋈ integral scalar without the rational decode: with the positive
// notch n/d, value ⋈ c ⟺ (bias + raw)·n ⋈ c·d — both sides exact
// integers (bias + raw is the signed value index, exact ordering AND
// equality since c·d is exact too). Eligible when both cross terms
// provably fit imax for every representable c of type A.
template <insidable B, typename A>
inline constexpr bool scalar_index_cmp_fits = [] {
    if constexpr (!std::integral<A> || value_storage<B> || !index_cmp_fits<B>)
        return false;
    else {
        constexpr umax cap       = static_cast<umax>(std::numeric_limits<imax>::max());
        constexpr umax notch_num = detail::notch64<B>.Numerator;
        constexpr umax notch_den = static_cast<umax>(detail::notch64<B>.Denominator); // Notch > 0
        constexpr umax index_mag = [] {
            constexpr auto lo = *(detail::lower64<B> / detail::notch64<B>);
            constexpr auto hi = *(detail::upper64<B> / detail::notch64<B>);
            return lo.Numerator > hi.Numerator ? lo.Numerator : hi.Numerator;
        }();
        constexpr umax scalar_mag = [] {
            umax mag = static_cast<umax>(std::numeric_limits<A>::max());
            if constexpr (std::signed_integral<A>) {
                umax min_mag = static_cast<umax>(-(std::numeric_limits<A>::min() + 1)) + 1;
                if (min_mag > mag)
                    mag = min_mag;
            }
            return mag;
        }();
        umax product;
        return !mul_overflow(index_mag, notch_num, &product) && product <= cap &&
               !mul_overflow(scalar_mag, notch_den, &product) && product <= cap;
    }
}();
} // namespace detail

namespace detail {
// inside ⋈ arithmetic scalar. Integer storage compares as integers when the
// scalar's type fits imax, and in double when it is floating point and the
// grid's values are exact in double; everything else goes through rational.
template <insidable B, plain_number A, class Cmp>
constexpr auto compare_scalar(const B& lhs, A rhs, Cmp cmp) {
    constexpr bool imax_scalar = std::signed_integral<A> || (std::unsigned_integral<A> && sizeof(A) < sizeof(imax));
    constexpr bool double_exact_values =
        lower_of<B> >= rational{-(imax{1} << 53)} && upper_of<B> <= rational{imax{1} << 53};
    // A long double with bits past the double's: the paths below read a double.
    if constexpr (wide_float<A>)
        if (is_finite(rhs) && !is_double_value(rhs)) [[unlikely]]
            return cmp(exact_of(lhs), exact_of_long_double(rhs));
    if constexpr (wide_valued<B>) {
        if constexpr (std::floating_point<A>) {
            if (rhs != rhs) // NaN: unordered, as the double comparison says
                return cmp(0.0, static_cast<double>(rhs));
            if (!is_finite(rhs)) // ±inf lies past every grid
                return cmp(exact_of(0), exact_of(rhs < 0 ? -1 : 1));
            return cmp(exact_of(lhs), exact_of_double(static_cast<double>(rhs)));
        } else
            return cmp(exact_of(lhs), exact_of(as_rational(rhs)));
    } else if constexpr (integer_value_storage<B> && values_fit_imax<B> && imax_scalar)
        return cmp(raw_imax(lhs), static_cast<imax>(rhs));
    else if constexpr (integer_value_storage<B> && values_fit_imax<B> && std::floating_point<A> && double_exact_values)
        return cmp(static_cast<double>(raw_imax(lhs)), static_cast<double>(rhs));
    else if constexpr (integer_index_storage<B> && std::floating_point<A> && double_exact<grid_of<B>> &&
                       index_double<B>.Ok)
        return cmp(as_double(lhs), static_cast<double>(rhs)); // exact: every value is a double
    else if constexpr (scalar_index_cmp_fits<B, A>)
        return cmp((index_cmp_bias<B> + raw_imax(lhs)) * static_cast<imax>(detail::notch64<B>.Numerator),
                   static_cast<imax>(rhs) * detail::notch64<B>.Denominator);
    else {
        // |rhs| ≥ 2^64 (or infinite) has no rational form, and every grid value
        // lies strictly inside ±2^64: the sign of rhs decides.
        if constexpr (std::floating_point<A>) {
            if (rhs != rhs) // NaN: unordered, as the double comparison says
                return cmp(0.0, static_cast<double>(rhs));
            if (past_64_bits(rhs))
                return cmp(rational{0}, rational{rhs < 0 ? -1 : 1});
            if (!abs_fraction_exact(static_cast<double>(rhs))) [[unlikely]] // finer than 2^-62
                return cmp(exact_of(lhs), exact_of_double(static_cast<double>(rhs)));
        }
        return cmp(as_rational(lhs), rational{rhs});
    }
}
} // namespace detail

template <insidable B, detail::plain_number A>
[[nodiscard]] constexpr auto operator<=>(const B& lhs, A rhs) {
    if constexpr (std::floating_point<A>)
        return detail::compare_scalar(lhs, rhs, detail::three_way_partial);
    else
        return detail::compare_scalar(lhs, rhs, detail::three_way);
}

template <insidable B, detail::plain_number A>
[[nodiscard]] constexpr bool operator==(const B& lhs, A rhs) {
    return detail::compare_scalar(lhs, rhs, detail::equal_to);
}
} // namespace beman::inside

BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_DETAIL_COMPARE_HPP
