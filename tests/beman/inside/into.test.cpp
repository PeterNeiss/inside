// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// mul_into<Out> / div_into<Out>: the exact product or quotient rounded once
// onto Out, without forming the product or quotient grid.

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <expected>
#include <type_traits>

using namespace beman::inside;
using beman::inside::detail::rational;

namespace {
inline constexpr rational wei{1, 1'000'000'000'000'000'000};
using eth   = inside<{{0, 1'000'000'000}, wei}>;              // a wide raw
using price = inside<{{0, 100'000'000}, per<100>}>;           // dollars per ETH, to the cent
using usd   = inside<{{0, 1'000'000'000'000'000}, per<100>}>; // dollars to the cent
} // namespace

TEST(IntoTest, mul_into_rounds_the_exact_product_once) {
    // eth × price has a 10^-20 notch: past the 64-bit grid numbers under C++23.
    const eth   balance = *from_chars<eth>("123456789.123456789123456789");
    const price p       = *from_chars<price>("3141.59");
    using usd_nearest   = inside<grid_of<usd>, round_nearest>;
    EXPECT_EQ(to_string(mul_into<usd_nearest>(balance, p)), "387850614142.36");
    EXPECT_EQ(to_string(mul_into<usd, round_floor>(balance, p)), "387850614142.36");
    EXPECT_EQ(to_string(mul_into<usd, round_ceil>(balance, p)), "387850614142.37");
    // Off the cents grid under a checked Out: reported, not rounded.
    EXPECT_THROW((void)mul_into<usd>(balance, p), inside_error);
    // On the grid: exact.
    EXPECT_EQ(mul_into<usd>(eth{2}, p), (usd{rational{628318, 100}}));
}

TEST(IntoTest, mul_into_range_follows_out_policy) {
    using small = inside<{0, 100}>;
    using clamped = inside<{0, 100}, clamp>;
    const small a{20}, b{30};
    EXPECT_THROW((void)mul_into<small>(a, b), inside_error);
    EXPECT_EQ(mul_into<clamped>(a, b), 100);
    EXPECT_EQ(mul_into<small>(small{5}, a), 100);
}

TEST(IntoTest, div_into) {
    using third = inside<{{0, 100}, per<3>}, round_nearest>;
    using pos   = inside<{1, 100}>;
    using any   = inside<{0, 100}>;
    // A zero-free divisor: a plain Out.
    const auto q = div_into<third>(any{7}, pos{3});
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(q)>, third>);
    EXPECT_EQ(q, (third{rational{7, 3}}));
    // Rounded once onto a coarser Out.
    using tenth = inside<{{0, 100}, per<10>}, round_nearest>;
    EXPECT_EQ(div_into<tenth>(any{7}, pos{3}), (tenth{rational{23, 10}}));
    // A divisor that may be 0: every failure is the expected's error.
    const auto r = div_into<third>(any{7}, any{3});
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(r)>, std::expected<third, errc>>);
    EXPECT_EQ(*r, (third{rational{7, 3}}));
    EXPECT_EQ(div_into<third>(any{7}, any{0}).error(), errc::division_by_zero);
    using one = inside<{0, 1}, round_nearest>;
    EXPECT_EQ(div_into<one>(any{7}, any{2}).error(), errc::overflow);
    using ten = inside<{0, 10}>;
    EXPECT_EQ(div_into<ten>(any{7}, any{2}).error(), errc::rounding_error);
    // A quotient with no 64-bit fraction, straight onto a grid.
    using fine = inside<{{0, 2}, per<(umax{1} << 62)>}, round_nearest>;
    const eth a = *from_chars<eth>("1.000000000000000001"), b = *from_chars<eth>("0.999999999999999999");
    EXPECT_EQ(div_into<fine>(a, b).value(), fine::from_raw((umax{1} << 62) + 9));
}
