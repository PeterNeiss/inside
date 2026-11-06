// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_COMPARE_HPP
#define BEMAN_INSIDE_INSIDE_COMPARE_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/detail/core.hpp>

#include <compare>
#include <limits>
#include <utility>

//---------------------------------------------------------------------------
// Comparisons of insides: across grids (by value index when both fit imax,
// else exactly) and against integers and rationals. A floating point is a
// deleted overload (0.1 is not 1/10 in binary). The inside type itself comes
// with it, so this is the smallest public header that declares an inside.
//---------------------------------------------------------------------------
namespace beman::inside {
//---------------------------------------------------------------------------
// comparison
//---------------------------------------------------------------------------
namespace detail {
// Integer value-index comparison eligibility: an integer-backed anchored
// inside whose value indices (value/Notch, integers) fit imax, so it
// compares with a scalar as `bias + raw` without a rational decode.
template <insidable B>
inline constexpr bool index_cmp_fits = [] {
    if constexpr (!notched<B> || !anchored<B> || !values_fit_imax<B> || !value_indices_fit64<B>)
        return false;
    else
        return std::in_range<imax>(lower_index64<B>.Num) && std::in_range<imax>(upper_index64<B>.Num);
}();

// Signed value index of Raw == 0: Lower/Notch for offset (index) storage,
// 0 for a value raw (raw is already the value == the index at notch 1).
template <insidable B>
inline constexpr imax index_cmp_bias = [] {
    if constexpr (index_storage<B>)
        return signed_numerator(lower_index64<B>);
    else
        return imax{0};
}();

// Inside values are finite and exact, so they are always ordered: <=> of two
// insides, or of an inside and an integer or rational, is a strong_ordering
// whatever the storage.
// Integers of mixed signedness compare by value (std::cmp_*), not by the
// usual arithmetic conversions; same-signed pairs keep the plain operator.
template <typename A, typename B>
concept mixed_sign_integers     = std::integral<std::remove_cvref_t<A>> && std::integral<std::remove_cvref_t<B>> &&
                                  std::is_signed_v<std::remove_cvref_t<A>> != std::is_signed_v<std::remove_cvref_t<B>>;
inline constexpr auto three_way = [](const auto& a, const auto& b) -> std::strong_ordering {
    if constexpr (mixed_sign_integers<decltype(a), decltype(b)>)
        return std::cmp_less(a, b)    ? std::strong_ordering::less
               : std::cmp_equal(a, b) ? std::strong_ordering::equal
                                      : std::strong_ordering::greater;
    else
        return a <=> b;
};
inline constexpr auto equal_to = [](const auto& a, const auto& b) {
    if constexpr (mixed_sign_integers<decltype(a), decltype(b)>)
        return std::cmp_equal(a, b);
    else
        return a == b;
};

// A point P that is one of X's slots, with X storing a builtin index: X's
// raw orders like its value, so X ⋈ P is its raw ⋈ that constant slot
// (`t != end(t)`, `x <= 4_ins`).
template <insidable X, insidable P>
inline constexpr bool point_is_slot =
    integer_index_storage<X> && !point_grid<X> && point_grid<P> && grid_of<X>.representable(lower_of<P>);
template <insidable X, insidable P>
inline constexpr raw_t<X> point_slot_of =
    static_cast<raw_t<X>>(exact_quotient(lower_of<P> - lower_of<X>, notch_of<X>));

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
    // a wide-valued operand (wide index, big grid numbers, fraction raws): exact wide fractions
    else if constexpr (wide_valued<L> || wide_valued<R>)
        return cmp(exact_of(lhs), exact_of(rhs));
    // integer raws: both values as exact counts of a unit dividing them both
    // (the gcd of their value units), one integer compare — any notches and
    // offsets, without the rational decode.
    else if constexpr (units_cmp_fits<L, R>) {
        constexpr grid_rational U = gcd(unit_of<L>, unit_of<R>);
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
        constexpr umax notch_num = notch64<B>.Num;
        constexpr umax notch_den = static_cast<umax>(notch64<B>.Den); // Notch > 0
        constexpr umax index_mag =
            lower_index64<B>.Num > upper_index64<B>.Num ? lower_index64<B>.Num : upper_index64<B>.Num;
        // The largest |c| of A: |min| = max + 1 for a signed type.
        constexpr umax scalar_mag = static_cast<umax>(std::numeric_limits<A>::max()) + std::signed_integral<A>;
        umax           product;
        return !mul_overflow(index_mag, notch_num, &product) && std::in_range<imax>(product) &&
               !mul_overflow(scalar_mag, notch_den, &product) && std::in_range<imax>(product);
    }
}();

// inside ⋈ integer or rational. Integer storage compares as integers when
// the scalar's type fits imax; everything else goes through rational.
template <insidable B, class A, class Cmp>
    requires(std::integral<A> || std::same_as<A, rational>)
constexpr auto compare_scalar(const B& lhs, A rhs, Cmp cmp) {
    constexpr bool imax_scalar = std::signed_integral<A> || (std::unsigned_integral<A> && sizeof(A) < sizeof(imax));
    if constexpr (wide_valued<B>)
        return cmp(exact_of(lhs), exact_of(as_rational(rhs)));
    else if constexpr (integer_value_storage<B> && values_fit_imax<B> && imax_scalar)
        return cmp(raw_imax(lhs), static_cast<imax>(rhs));
    else if constexpr (integer_value_storage<B> && values_fit_imax<B> && std::unsigned_integral<A>)
        return cmp(raw_imax(lhs), rhs); // a 64-bit unsigned: sign-safe, by the cmp_* forms
    else if constexpr (scalar_index_cmp_fits<B, A>)
        return cmp((index_cmp_bias<B> + raw_imax(lhs)) * static_cast<imax>(notch64<B>.Num),
                   static_cast<imax>(rhs) * notch64<B>.Den);
    else
        return cmp(as_rational(lhs), rational{rhs});
}
} // namespace detail

template <insidable B, class A>
    requires(std::integral<A> || std::same_as<A, rational>)
[[nodiscard]] constexpr auto operator<=>(const B& lhs, A rhs) {
    return detail::compare_scalar(lhs, rhs, detail::three_way);
}

template <insidable B, class A>
    requires(std::integral<A> || std::same_as<A, rational>)
[[nodiscard]] constexpr bool operator==(const B& lhs, A rhs) {
    return detail::compare_scalar(lhs, rhs, detail::equal_to);
}

// No comparison with a floating-point value: 0.1 is not 1/10 in binary, so
// an inside holding 1/10 would compare unequal to 0.1. Compare with a
// rational (0.1_r), a point (just<1, 10>) or an inside.
#define BEMAN_INSIDE_WHY_float_compare                                                                    \
    "inside: no comparison with a floating-point value - 0.1 is not 1/10 in binary; compare with 0.1_r, " \
    "just<1, 10> or an inside"
template <insidable B, std::floating_point A>
auto operator<=>(const B&, A) = delete (BEMAN_INSIDE_WHY_float_compare);
template <insidable B, std::floating_point A>
bool operator==(const B&, A) = delete (BEMAN_INSIDE_WHY_float_compare);
#undef BEMAN_INSIDE_WHY_float_compare
} // namespace beman::inside

#endif // BEMAN_INSIDE_INSIDE_COMPARE_HPP
