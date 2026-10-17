// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// mul_into<Out> / div_into<Out>: the exact product or quotient rounded once
// onto Out, without forming the product or quotient grid.

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <expected>
#include <type_traits>

using namespace beman::inside;

namespace {
inline constexpr rational wei{1, 1'000'000'000'000'000'000};
using eth   = inside<{{0, 1'000'000'000}, wei}>;              // a wide raw
using price = inside<{{0, 100'000'000}, per<100>}>;           // dollars per ETH, to the cent
using usd   = inside<{{0, 1'000'000'000'000'000}, per<100>}>; // dollars to the cent
} // namespace

TEST(IntoTest, mul_into_rounds_the_exact_product_once) {
    // eth × price has a 10^-20 notch: past the 64-bit grid numbers under C++23.
    const eth   balance = *parse<eth>("123456789.123456789123456789");
    const price p       = *parse<price>("3141.59");
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
    using small   = inside<{0, 100}>;
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
    using fine  = inside<{{0, 2}, per<(umax{1} << 62)>}, round_nearest>;
    const eth a = *parse<eth>("1.000000000000000001"), b = *parse<eth>("0.999999999999999999");
    EXPECT_EQ(div_into<fine>(a, b).value(), fine::from_raw((umax{1} << 62) + 9));
}

// The native paths (the exact product inside, or a 64-bit rational) round as
// the wide path does: every value pair of small grids, every Out policy, the
// same result or the same error.
namespace {
template <class Out, policy_flag F, class Fn>
std::expected<Out, errc> outcome(Fn&& f) {
    try {
        return f();
    } catch (const inside_error& e) {
        return std::unexpected{e.Code};
    }
}
template <class Out, policy_flag F, class A, class B>
void expect_mul_div_agree() {
    for (const A a : values<A>)
        for (const B b : values<B>) {
            const auto policy = make_policy<flags_of<Out> | F>();
            const auto m      = outcome<Out, F>([&] { return mul_into<Out, F>(a, b); });
            const auto mw     = outcome<Out, F>([&] { return detail::mul_into_wide<Out>(a, b, policy); });
            EXPECT_EQ(m, mw) << to_string(a) << " * " << to_string(b);
            if (b == 0)
                continue;
            const auto d  = outcome<Out, F>([&] {
                const auto r = div_into<Out, F>(a, b);
                if constexpr (detail::expected_like<std::remove_const_t<decltype(r)>>) {
                    if (!r)
                        throw inside_error(r.error());
                    return *r;
                } else
                    return r;
            });
            const auto dw = outcome<Out, F>([&] { return detail::div_into_wide<Out>(a, b, policy); });
            EXPECT_EQ(d, dw) << to_string(a) << " / " << to_string(b);
        }
}
using int_g  = inside<{-7, 9}>;
using quart  = inside<{{-3, 3}, per<4>}>;
using thirds = inside<{{rational{1, 3}, rational{13, 3}}, rational{1, 2}}>; // unanchored
using neg    = inside<{{-2, -1}, per<8>}>;
} // namespace

TEST(IntoTest, native_paths_agree_with_the_wide_path) {
    using round_out = inside<{{-50, 50}, per<3>}, round_nearest>;
    using floor_out = inside<{-40, 40}, round_floor>;
    using clamp_out = inside<{{-5, 5}, per<2>}, clamp | round_ceil>;
    using check_out = inside<{{-20, 20}, per<2>}>;
    // Both native paths are taken: the product inside, and the rational
    // product where the store would reject the product type at compile time.
    static_assert(detail::native_product<round_out, none, int_g, quart>);
    static_assert(detail::native_product<clamp_out, none, int_g, int_g>);
    static_assert(!detail::native_product<check_out, none, int_g, quart>);
    static_assert(detail::rational_operands<int_g, quart>);
    expect_mul_div_agree<round_out, none, int_g, quart>();
    expect_mul_div_agree<round_out, none, quart, thirds>();
    expect_mul_div_agree<floor_out, none, thirds, int_g>();
    expect_mul_div_agree<floor_out, none, neg, quart>();
    expect_mul_div_agree<clamp_out, none, int_g, int_g>();
    expect_mul_div_agree<clamp_out, none, quart, thirds>();
    expect_mul_div_agree<check_out, none, int_g, quart>();
    expect_mul_div_agree<check_out, none, thirds, thirds>();
    expect_mul_div_agree<check_out, round_nearest, quart, int_g>();
    expect_mul_div_agree<int_g, round_floor, quart, quart>();
}
