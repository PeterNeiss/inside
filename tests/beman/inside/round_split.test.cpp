// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// round_quotient rounds an offset quotient from ONE division: the floor q and
// remainder r, the sign of the floor's value index m + q, its parity. The
// earlier code rebuilt the signed value index m·den + num and divided again
// (a second, signed division), with the one-division form only past imax.
// This test keeps that earlier code as the reference and checks the two
// agree for every rounding mode, on grids whose value index m is positive,
// zero and negative, on unanchored grids, and near the 64-bit limits.

#include <beman/inside/inside.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
template <insidable L, typename P>
umax reference_round_quotient(umax num, umax den) {
    constexpr auto zq = (!notched<L>) ? std::expected<rational, errc>{rational{0}} : try_div(lower64<L>, notch64<L>);
    constexpr bool vidx =
        zq.has_value() && abs_den(zq->Den) == 1 && zq->Num <= static_cast<umax>(std::numeric_limits<imax>::max()) + 1;
    constexpr imax       m    = vidx ? signed_numerator(*zq) : imax{0};
    constexpr round_mode mode = rounding_for<L, P>;
    if constexpr (!anchored<L>) {
        const umax q = num / den, r = num % den;
        bool       negative = false;
        if constexpr (lower64<L> < 0) {
            constexpr rational c = -*lower_index64<L>;
            using W              = wide_uint<2>;
            negative             = W{num} * W{abs_den(c.Den)} < W{c.Num} * W{den};
        }
        return q + rounds_up(mode, negative, classify_remainder(mode, r, den), ((q & 1) != 0) != lower_index_odd<L>);
    } else if constexpr (!vidx)
        return round_offset<L, P>(num / den, num % den, den);
    else {
        const imax di = static_cast<imax>(den);
        imax       mdi, NUM;
        if (!std::in_range<imax>(num) || mul_overflow(m, di, &mdi) || add_overflow(mdi, static_cast<imax>(num), &NUM)) {
            const umax q = num / den, r = num % den;
            const bool negative = m < 0 && q < umax{0} - static_cast<umax>(m);
            return q +
                   rounds_up(mode, negative, classify_remainder(mode, r, den), ((q ^ static_cast<umax>(m)) & 1) != 0);
        }
        const imax t   = NUM / di;
        const imax rr  = NUM % di;
        const bool neg = NUM < 0;
        const umax ar  = (rr < 0) ? ~static_cast<umax>(rr) + 1u : static_cast<umax>(rr);
        const imax J   = rounds_away(mode, neg, classify_remainder(mode, ar, static_cast<umax>(di)), (t & 1) != 0)
                             ? (neg ? t - 1 : t + 1)
                             : t;
        return static_cast<umax>(J - m);
    }
}

template <grid G, policy_flag F>
void check_grid() {
    using L = inside<G, F>;
    using P = decltype(make_policy<flags_of<L>>());
    for (umax den = 1; den <= 40; ++den)
        for (umax num = 0; num <= 600; ++num) {
            const umax want = reference_round_quotient<L, P>(num, den);
            ASSERT_EQ((round_quotient<L, P>(num, den)), want) << "num " << num << " den " << den;
            ASSERT_EQ((round_split<L, P>(num / den, num % den, den)), want) << "num " << num << " den " << den;
        }
    // Near the limits: a numerator past imax, m·den past imax.
    constexpr umax big = static_cast<umax>(std::numeric_limits<imax>::max());
    for (umax den : {umax{3}, umax{1} << 20, (umax{1} << 40) + 7, big / 5, big})
        for (umax num : {big - 11, big, big + 1, big + 12345, std::numeric_limits<umax>::max() - den})
            ASSERT_EQ((round_quotient<L, P>(num, den)), (reference_round_quotient<L, P>(num, den)))
                << "num " << num << " den " << den;
}

template <grid G>
void check_modes() {
    check_grid<G, round_nearest>();
    check_grid<G, round_half_even>();
    check_grid<G, round_floor>();
    check_grid<G, round_ceil>();
    check_grid<G, round_toward_zero>();
}
} // namespace

TEST(RoundSplitTest, anchored_value_index_positive) { check_modes<grid{{3, 50}, 0.5_r}>(); }
TEST(RoundSplitTest, anchored_value_index_zero) { check_modes<grid{{0, 255}, per<256>}>(); }
TEST(RoundSplitTest, anchored_value_index_negative_odd) { check_modes<grid{{-9, 9}, 1}>(); }
TEST(RoundSplitTest, anchored_value_index_negative_fine) { check_modes<grid{{-100, 100}, per<4>}>(); }
TEST(RoundSplitTest, anchored_coarse_notch) { check_modes<grid{{-9, 12}, 3}>(); }
TEST(RoundSplitTest, unanchored_lower_above_zero) { check_modes<grid{{0.5_r, 20.5_r}, 1}>(); }
TEST(RoundSplitTest, unanchored_lower_below_zero) { check_modes<grid{{-7.25_r, 3.75_r}, 0.5_r}>(); }
