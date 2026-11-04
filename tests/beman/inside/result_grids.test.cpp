// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Result grids that hold exactly the values an operation can produce: the
// remainder by every rounding mode, min/max, |x|, even powers and lerp.
// Result types freeze at 1.0, so these pin the tight forms.

#include <beman/inside/inside.hpp>
#include <beman/inside/math.hpp>
#include <beman/inside/units.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <expected>
#include <type_traits>

using namespace beman::inside;

namespace {
template <class T>
struct unwrap {
    using type = T;
};
template <class T, class E>
struct unwrap<std::expected<T, E>> {
    using type = T;
};

template <class T>
constexpr imax lo() {
    return detail::trunc(detail::to_rational(lower_of<typename unwrap<T>::type>));
}
template <class T>
constexpr imax hi() {
    return detail::trunc(detail::to_rational(upper_of<typename unwrap<T>::type>));
}

// r = a − round(a/b)·b on plain integers.
enum class mode { trunc, floor, ceil, nearest };
constexpr imax reference_remainder(imax a, imax b, mode m) {
    imax       q   = a / b;
    const imax r   = a - q * b;
    const bool neg = (a < 0) != (b < 0);
    if (r != 0) {
        if (m == mode::floor && neg)
            --q;
        else if (m == mode::ceil && !neg)
            ++q;
        else if (m == mode::nearest && 2 * (r < 0 ? -r : r) >= (b < 0 ? -b : b))
            q += neg ? -1 : 1;
    }
    return a - q * b;
}

// Every remainder of every pair lies in the result type's range, and the
// range's ends are reached (no slack) except at nearest-mode ties.
template <mode M, auto Flag, grid GA, grid GB>
void expect_tight_remainder(bool ends_reached) {
    using R            = decltype(mod(inside<GA>{}, inside<GB>{}, Flag));
    imax       seen_lo = hi<R>() + 1;
    imax       seen_hi = lo<R>() - 1;
    const imax a_lo = lo<inside<GA>>(), a_hi = hi<inside<GA>>();
    const imax b_lo = lo<inside<GB>>(), b_hi = hi<inside<GB>>();
    for (imax a = a_lo; a <= a_hi; ++a)
        for (imax b = b_lo; b <= b_hi; ++b) {
            if (b == 0)
                continue;
            const imax r = reference_remainder(a, b, M);
            ASSERT_LE(lo<R>(), r) << "a " << a << " b " << b;
            ASSERT_LE(r, hi<R>()) << "a " << a << " b " << b;
            seen_lo = std::min(seen_lo, r);
            seen_hi = std::max(seen_hi, r);
        }
    if (ends_reached) {
        EXPECT_EQ(seen_lo, lo<R>());
        EXPECT_EQ(seen_hi, hi<R>());
    }
}

template <grid GA, grid GB>
void expect_all_modes() {
    expect_tight_remainder<mode::trunc, round_toward_zero, GA, GB>(true);
    expect_tight_remainder<mode::floor, round_floor, GA, GB>(true);
    expect_tight_remainder<mode::ceil, round_ceil, GA, GB>(true);
    expect_tight_remainder<mode::nearest, round_nearest, GA, GB>(false);
}
} // namespace

TEST(ResultGridsTest, remainder_holds_exactly_what_each_mode_produces) {
    expect_all_modes<grid{-12, 12}, grid{-7, 5}>();
    expect_all_modes<grid{0, 30}, grid{1, 9}>();
    expect_all_modes<grid{-30, 0}, grid{1, 9}>();
    expect_all_modes<grid{-30, -2}, grid{-9, -1}>();
    expect_all_modes<grid{0, 3}, grid{1, 100}>();
    expect_all_modes<grid{-3, 40}, grid{2, 8}>();
    expect_all_modes<grid{5, 9}, grid{-4, -2}>();
    expect_all_modes<grid{-50, 50}, grid{-3, 3}>();
}

TEST(ResultGridsTest, remainder_is_bounded_by_the_dividend) {
    using small = inside<{0, 3}>;
    using big   = inside<{1, 100}>;
    static_assert(std::is_same_v<decltype(small{} % big{}), inside<{0, 3}>>);
    static_assert(std::is_same_v<decltype(inside<{-100, 0}>{} % big{}), inside<{-99, 0}>>);
    static_assert(std::is_same_v<decltype(mod(inside<{-100, 100}>{}, big{}, round_floor)), inside<{0, 99}>>);
    static_assert(std::is_same_v<decltype(divmod(small{}, big{}).Remainder), inside<{0, 3}>>);
}

TEST(ResultGridsTest, min_and_max_hold_the_values_they_can_take) {
    using a_t = inside<{0, 100}>;
    using c_t = inside<{0, 10}>;
    static_assert(std::is_same_v<decltype(min(a_t{}, c_t{})), inside<{0, 10}>>);
    static_assert(std::is_same_v<decltype(max(a_t{}, c_t{})), inside<{0, 100}>>);
    static_assert(std::is_same_v<decltype(min(inside<{-100, 0}>{}, inside<{5, 10}>{})), inside<{-100, 0}>>);
    static_assert(std::is_same_v<decltype(max(inside<{-100, 0}>{}, inside<{5, 10}>{})), inside<{5, 10}>>);
    static_assert(std::is_same_v<decltype(min(a_t{}, a_t{})), a_t>);

    EXPECT_TRUE(min(a_t{50}, c_t{3}) == 3_ins);
    EXPECT_TRUE(max(a_t{50}, c_t{3}) == 50_ins);
    EXPECT_TRUE(min(a_t{2}, c_t{3}) == 2_ins);
    EXPECT_TRUE(min(inside<{{0.5, 10.5}, 1}>{2.5}, c_t{3}) == 2.5_r);
    EXPECT_TRUE(max(inside<{{0.5, 10.5}, 1}>{2.5}, c_t{3}) == 3_ins);
}

TEST(ResultGridsTest, abs_and_even_powers_never_reach_below_their_least_magnitude) {
    static_assert(std::is_same_v<decltype(math::abs(inside<{5, 10}>{5})), inside<{5, 10}>>);
    static_assert(std::is_same_v<decltype(math::abs(inside<{-10, -5}>{-5})), inside<{5, 10}>>);
    static_assert(std::is_same_v<decltype(math::abs(inside<{-10, 10}>{})), inside<{0, 10}>>);
    static_assert(std::is_same_v<decltype(math::pown<2>(inside<{-10, 10}>{})), inside<{0, 100}>>);
    static_assert(std::is_same_v<decltype(math::pown<2>(inside<{-10, -2}>{-2})), inside<{4, 100}>>);
    static_assert(std::is_same_v<decltype(math::pown<3>(inside<{-10, 10}>{})), inside<{-1000, 1000}>>);

    EXPECT_TRUE(math::abs(inside<{-10, -5}>{-7}) == 7_ins);
    EXPECT_TRUE(math::pown<2>(inside<{-10, 10}>{-7}) == 49_ins);
    EXPECT_TRUE(math::pown<4>(inside<{-3, 2}>{-3}) == 81_ins);
}

TEST(ResultGridsTest, lerp_stays_between_its_ends) {
    using v_t = inside<{0, 100}>;
    using t_t = inside<{{0, 1}, per<4>}>;
    static_assert(std::is_same_v<decltype(lerp(v_t{}, v_t{}, t_t{})), inside<{{0, 100}, per<4>}>>);
    static_assert(std::is_same_v<decltype(lerp(inside<{0, 10}>{}, v_t{}, t_t{})), inside<{{0, 100}, per<4>}>>);
    // t past [0, 1] extrapolates: the wide grid stays.
    static_assert(lower_of<decltype(lerp(v_t{}, v_t{}, inside<{{0, 2}, per<4>}>{}))> < 0);

    EXPECT_TRUE(lerp(v_t{10}, v_t{30}, t_t{0.25}) == 15_ins);
    EXPECT_TRUE(lerp(v_t{30}, v_t{10}, t_t{1}) == 10_ins);
}

TEST(ResultGridsTest, quantities_take_the_numbers_tight_types) {
    using namespace beman::inside::units;
    const quantity<si::metre, inside<{0, 100}>>         a{10};
    const quantity<si::kilo(si::metre), inside<{0, 1}>> b{1};
    const inside<{{0, 1}, per<4>}>                      t{0.25};
    // In the common unit, metres: lerp between [0, 100] m and [0, 1000] m.
    static_assert(
        std::is_same_v<std::remove_cvref_t<decltype(lerp(a, b, t).numerical_value())>, inside<{{0, 1000}, per<4>}>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(min(a, b).numerical_value())>, inside<{0, 100}>>);
    EXPECT_TRUE((lerp(a, b, t) == just<2575, 10> * si::metre));
    EXPECT_TRUE(min(a, b) == just<10> * si::metre);
}
