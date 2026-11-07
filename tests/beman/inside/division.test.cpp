// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Division and modulo: the rounding mode on every path (integer,
// Q-format, exact), round_quotient, and division by points.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/casts.hpp>
#include <beman/inside/inside/detail/rational.hpp>
#include <beman/inside/inside/formats.hpp>
#include <beman/inside/inside/grid.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/predicates.hpp>
#include <beman/inside/math.hpp>

#include <gtest/gtest.h>

#include "support.hpp"

using beman::inside::test::exact;

#include <cfloat>
#include <cmath>
#include <compare>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

// Integer / Q-format division and modulo honour the rounding-mode policy.
//
// The native div/mod paths once truncated toward zero whatever the mode
// (a `round_nearest` grid divided like C++ `/`). These pin the behaviour: each mode rounds the quotient accordingly,
// the remainder stays consistent (`(a/b)*b + a%b == a`), and the result lands inside its grid.
namespace div_round_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
// Reference: round the exact quotient a/b per each mode (integer math).
std::int64_t ref_trunc(std::int64_t a, std::int64_t b) { return a / b; }
std::int64_t ref_floor(std::int64_t a, std::int64_t b) {
    std::int64_t t = a / b, r = a % b;
    return (r != 0 && ((a < 0) != (b < 0))) ? t - 1 : t;
}
std::int64_t ref_ceil(std::int64_t a, std::int64_t b) {
    std::int64_t t = a / b, r = a % b;
    return (r != 0 && ((a < 0) == (b < 0))) ? t + 1 : t;
}
std::int64_t ref_near(std::int64_t a, std::int64_t b) {
    std::int64_t t = a / b, r = a % b;
    if (r == 0)
        return t;
    bool      neg = (a < 0) != (b < 0);
    long long ar = std::llabs(r), ab = std::llabs(b);
    return (2 * ar >= ab) ? (neg ? t - 1 : t + 1) : t;
}
std::int64_t ref_heven(std::int64_t a, std::int64_t b) {
    std::int64_t t = a / b, r = a % b;
    if (r == 0)
        return t;
    bool      neg = (a < 0) != (b < 0);
    long long ar = std::llabs(r), ab = std::llabs(b);
    if (2 * ar < ab)
        return t;
    if (2 * ar > ab)
        return neg ? t - 1 : t + 1;
    std::int64_t away = neg ? t - 1 : t + 1;
    return (t & 1) == 0 ? t : away;
}
} // namespace

// integer division honours each rounding mode
TEST(DivRoundTest, integer_division_honours_each_rounding_mode) {
    using Nn  = inside<{-100, 100}, round_nearest>;
    using Nnd = inside<{1, 10}, round_nearest>;
    ASSERT_TRUE(static_cast<rational>(Nn{-8} / Nnd{3}) == -3); // round to nearest, not -2
    ASSERT_TRUE(static_cast<rational>(Nn{8} / Nnd{3}) == 3);
    ASSERT_TRUE(static_cast<rational>(Nn{7} / Nnd{2}) == 4); // 3.5 → 4 (half away)
    ASSERT_TRUE(static_cast<rational>(Nn{-7} / Nnd{2}) == -4);

    using Ff  = inside<{-100, 100}, round_floor>;
    using Ffd = inside<{1, 10}, round_floor>;
    ASSERT_TRUE(static_cast<rational>(Ff{-8} / Ffd{3}) == -3); // floor(-2.667)
    ASSERT_TRUE(static_cast<rational>(Ff{8} / Ffd{3}) == 2);   // floor(2.667)

    using Cc  = inside<{-100, 100}, round_ceil>;
    using Ccd = inside<{1, 10}, round_ceil>;
    ASSERT_TRUE(static_cast<rational>(Cc{8} / Ccd{3}) == 3);   // ceil(2.667)
    ASSERT_TRUE(static_cast<rational>(Cc{-8} / Ccd{3}) == -2); // ceil(-2.667)

    using Tt  = inside<{-100, 100}, round_toward_zero>;
    using Ttd = inside<{1, 10}, round_toward_zero>;
    ASSERT_TRUE(static_cast<rational>(Tt{-8} / Ttd{3}) == -2); // round_toward_zero truncates
    ASSERT_TRUE(static_cast<rational>(Tt{8} / Ttd{3}) == 2);
}

// half-even division breaks ties to even
TEST(DivRoundTest, half_even_division_breaks_ties_to_even) {
    using H  = inside<{-100, 100}, round_half_even>;
    using Hd = inside<{1, 10}, round_half_even>;
    ASSERT_TRUE(static_cast<rational>(H{5} / Hd{2}) == 2);   // 2.5 → 2 (even)
    ASSERT_TRUE(static_cast<rational>(H{7} / Hd{2}) == 4);   // 3.5 → 4 (even)
    ASSERT_TRUE(static_cast<rational>(H{-5} / Hd{2}) == -2); // -2.5 → -2
    ASSERT_TRUE(static_cast<rational>(H{9} / Hd{2}) == 4);   // 4.5 → 4
    ASSERT_TRUE(static_cast<rational>(H{3} / Hd{2}) == 2);   // 1.5 → 2
}

// modulo remainder stays consistent with the rounded quotient
TEST(DivRoundTest, modulo_remainder_stays_consistent_with_the_rounded_quotient) {
    // For every mode, (a/b)*b + a%b == a.
    using N  = inside<{-100, 100}, round_nearest>;
    using Nd = inside<{1, 10}, round_nearest>;
    ASSERT_TRUE(static_cast<rational>(N{-8} % Nd{3}) == 1); // -8 = (-3)*3 + 1
    ASSERT_TRUE(static_cast<rational>(N{8} % Nd{3}) == -1); //  8 =   3 *3 - 1

    using F  = inside<{-100, 100}, round_floor>;
    using Fd = inside<{1, 10}, round_floor>;
    ASSERT_TRUE(static_cast<rational>(F{-8} % Fd{3}) == 1); // floored remainder ≥ 0 for b>0
    ASSERT_TRUE(static_cast<rational>(F{8} % Fd{3}) == 2);

    using T  = inside<{-100, 100}, round_toward_zero>;
    using Td = inside<{1, 10}, round_toward_zero>;
    ASSERT_TRUE(static_cast<rational>(T{-8} % Td{3}) == -2); // truncated remainder (sign of dividend)
}

// Q-format division honours the rounding mode
TEST(DivRoundTest, q_format_division_honours_the_rounding_mode) {
    using Qn = inside<{{0, 255}, per<256>}, round_nearest>;
    using Qt = inside<{{0, 255}, per<256>}, round_toward_zero>;
    // 200/3 = 66.6667 → on the 1/256 grid: nearest = 17067/256, truncate = 17066/256.
    ASSERT_TRUE((static_cast<rational>(*(Qn{200.0} / Qn{3.0})) == rational{17067u, 256}));
    ASSERT_TRUE((static_cast<rational>(*(Qt{200.0} / Qt{3.0})) == rational{17066u, 256}));

    // floor / ceil / half_even also honoured (round_uquotient — Lower == 0, so the
    // quotient is non-negative; these arms were previously untested).
    using Qf = inside<{{0, 255}, per<256>}, round_floor>;
    using Qc = inside<{{0, 255}, per<256>}, round_ceil>;
    using Qe = inside<{{0, 255}, per<256>}, round_half_even>;
    ASSERT_TRUE((static_cast<rational>(*(Qf{200.0} / Qf{3.0})) == rational{17066u, 256})); // floor
    ASSERT_TRUE((static_cast<rational>(*(Qc{200.0} / Qc{3.0})) == rational{17067u, 256})); // ceil
    // exact ties on the 1/256 grid: 1.5/256 → 2 (even), 2.5/256 → 2 (even).
    ASSERT_TRUE((static_cast<rational>(*(Qe{rational{3, 256}} / Qe{2.0})) == rational{2u, 256}));
    ASSERT_TRUE((static_cast<rational>(*(Qe{rational{5, 256}} / Qe{2.0})) == rational{2u, 256}));
    ASSERT_TRUE((static_cast<rational>(*(Qn{rational{5, 256}} / Qn{2.0})) == rational{3u, 256})); // nearest tie
}

namespace {
template <class A, class B, std::int64_t (*Ref)(std::int64_t, std::int64_t)>
void sweep_div_mod() {
    for (std::int64_t a = lower_int64<A>; a <= upper_int64<A>; ++a)
        for (std::int64_t b = lower_int64<B>; b <= upper_int64<B>; ++b) {
            if (b == 0)
                continue;
            A                  ba{a};
            B                  bb{b};
            const std::int64_t q = Ref(a, b);
            SCOPED_TRACE(::testing::Message() << "a=" << a << " b=" << b << " expected q=" << q);
            ASSERT_TRUE(static_cast<rational>(ba / bb) == q);         // quotient per mode
            ASSERT_TRUE(static_cast<rational>(ba % bb) == a - q * b); // remainder a − q·b
            const auto [dq, dr] = divmod(ba, bb);                     // divmod agrees with / and %
            ASSERT_TRUE(dq == ba / bb);
            ASSERT_TRUE(dr == ba % bb);
        }
}
} // namespace

// div/mod property sweep vs reference, every mode
TEST(DivRoundTest, div_mod_property_sweep_vs_reference_every_mode) {
    // Divisor grids exclude zero so results are plain (non-expected).
    using NA   = inside<{-40, 40}, round_nearest>;
    using ND   = inside<{-7, -1}, round_nearest>; // negative divisors too
    using NPos = inside<{1, 7}, round_nearest>;
    sweep_div_mod<NA, NPos, ref_near>();
    sweep_div_mod<NA, ND, ref_near>();

    using FA  = inside<{-40, 40}, round_floor>;
    using FD  = inside<{1, 7}, round_floor>;
    using FDn = inside<{-7, -1}, round_floor>;
    sweep_div_mod<FA, FD, ref_floor>();
    sweep_div_mod<FA, FDn, ref_floor>();

    using CA = inside<{-40, 40}, round_ceil>;
    using CD = inside<{1, 7}, round_ceil>;
    sweep_div_mod<CA, CD, ref_ceil>();

    using HA = inside<{-40, 40}, round_half_even>;
    using HD = inside<{1, 7}, round_half_even>;
    sweep_div_mod<HA, HD, ref_heven>();

    using TA  = inside<{-40, 40}, round_toward_zero>;
    using TD  = inside<{1, 7}, round_toward_zero>;
    using TDn = inside<{-7, -1}, round_toward_zero>;
    sweep_div_mod<TA, TD, ref_trunc>();
    sweep_div_mod<TA, TDn, ref_trunc>();
}

// divmod: the grids of / and %, expected only when the divisor may be zero
TEST(DivRoundTest, divmod_types_and_errors) {
    using val = inside<{0, 100}, round_toward_zero>;
    using pos = inside<{1, 10}, round_toward_zero>;

    const auto [q, r] = divmod(val{17}, pos{5});
    static_assert(std::same_as<decltype(q), const decltype(val{} / pos{1})>);
    static_assert(std::same_as<decltype(r), const decltype(val{} % pos{1})>);
    ASSERT_TRUE(q == 3_ins);
    ASSERT_TRUE(r == 2_ins);

    // A per-call rounding policy picks the quotient; the remainder follows it.
    using num          = inside<{-100, 100}>;
    using den          = inside<{1, 10}>;
    const auto [f, fr] = divmod(num{-8}, den{3}, round_floor);
    ASSERT_TRUE(f == -3_ins);
    ASSERT_TRUE(fr == 1_ins);

    // A divisor grid holding zero: expected, division_by_zero.
    auto e   = divmod(val{17}, val{0});
    using vq = decltype(val{} / val{1})::value_type;
    using vr = decltype(val{} % val{1})::value_type;
    static_assert(std::same_as<decltype(e), std::expected<divmod_result<vq, vr>, errc>>);
    ASSERT_FALSE(e.has_value());
    ASSERT_EQ(e.error(), errc::division_by_zero);
    auto ok = divmod(val{17}, val{4});
    ASSERT_TRUE(ok.has_value() && ok->Quotient == 4_ins && ok->Remainder == 1_ins);

    errc ec{};
    (void)divmod(val{1}, val{0}, ec);
    ASSERT_EQ(ec, errc::division_by_zero);

    static_assert([] {
        const auto [cq, cr] = divmod(val{99}, pos{10});
        return cq == 9_ins && cr == 9_ins;
    }());
}

// Without a rounding mode `/` stays exact, but `%` and divmod still round: truncation
// (C++ `%`) unless the policy names a mode.
TEST(DivRoundTest, modulo_and_divmod_without_a_rounding_mode_truncate) {
    using digit        = inside<{0, 9}>;
    using signed_digit = inside<{-9, 9}>;

    static_assert(std::same_as<decltype(digit{7} % 2_ins), inside<{0, 1}>>);
    static_assert(digit{7} % 2_ins == 1_ins);
    static_assert(signed_digit{-7} % 2_ins == -1_ins);
    static_assert(mod(signed_digit{-7}, 2_ins, round_floor) == 1_ins);
    static_assert(digit{7} / 2_ins == 3.5_ins); // `/` is unchanged: exact

    const auto [q, r] = divmod(signed_digit{-7}, 2_ins);
    ASSERT_TRUE(q == -3_ins);
    ASSERT_TRUE(r == -1_ins);
    ASSERT_TRUE(q * 2_ins + r == -7_ins);

    digit d{8};
    d %= 3_ins;
    ASSERT_TRUE(d == 2_ins);
}
} // namespace div_round_tests

// round_quotient rounds an offset quotient from ONE division: the floor q and
// remainder r, the sign of the floor's value index m + q, its parity. The
// earlier code rebuilt the signed value index m·den + num and divided again
// (a second, signed division), with the one-division form only past int64.
// This test keeps that earlier code as the reference and checks the two
// agree for every rounding mode, on grids whose value index m is positive,
// zero and negative, on unanchored grids, and near the 64-bit limits.
namespace round_split_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
template <insidable L, typename P>
std::uint64_t reference_round_quotient(std::uint64_t num, std::uint64_t den) {
    constexpr auto zq   = (!notched<L>) ? std::expected<rational, errc>{rational{0}} : try_div(lower64<L>, notch64<L>);
    constexpr bool vidx = zq.has_value() && abs_den(zq->Den) == 1 &&
                          zq->Num <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1;
    constexpr std::int64_t m    = vidx ? signed_numerator(*zq) : std::int64_t{0};
    constexpr round_mode   mode = rounding_for<L, P>;
    if constexpr (!anchored<L>) {
        const std::uint64_t q = num / den, r = num % den;
        bool                negative = false;
        if constexpr (lower64<L> < 0) {
            constexpr rational c = -lower_index64<L>;
            using W              = wide_uint<2>;
            negative             = W{num} * W{abs_den(c.Den)} < W{c.Num} * W{den};
        }
        return q + rounds_up(mode, negative, classify_remainder(mode, r, den), ((q & 1) != 0) != lower_index_odd<L>);
    } else if constexpr (!vidx)
        return round_offset<L, P>(num / den, num % den, den);
    else {
        const std::int64_t di = static_cast<std::int64_t>(den);
        std::int64_t       mdi, NUM;
        if (!std::in_range<std::int64_t>(num) || mul_overflow(m, di, &mdi) ||
            add_overflow(mdi, static_cast<std::int64_t>(num), &NUM)) {
            const std::uint64_t q = num / den, r = num % den;
            const bool          negative = m < 0 && q < std::uint64_t{0} - static_cast<std::uint64_t>(m);
            return q + rounds_up(mode,
                                 negative,
                                 classify_remainder(mode, r, den),
                                 ((q ^ static_cast<std::uint64_t>(m)) & 1) != 0);
        }
        const std::int64_t  t   = NUM / di;
        const std::int64_t  rr  = NUM % di;
        const bool          neg = NUM < 0;
        const std::uint64_t ar  = (rr < 0) ? ~static_cast<std::uint64_t>(rr) + 1u : static_cast<std::uint64_t>(rr);
        const std::int64_t  J =
            rounds_away(mode, neg, classify_remainder(mode, ar, static_cast<std::uint64_t>(di)), (t & 1) != 0)
                ? (neg ? t - 1 : t + 1)
                : t;
        return static_cast<std::uint64_t>(J - m);
    }
}

template <grid G, policy_flag F>
void check_grid() {
    using L = inside<G, F>;
    using P = decltype(make_policy<flags_of<L>>());
    for (std::uint64_t den = 1; den <= 40; ++den)
        for (std::uint64_t num = 0; num <= 600; ++num) {
            const std::uint64_t want = reference_round_quotient<L, P>(num, den);
            ASSERT_EQ((round_quotient<L, P>(num, den)), want) << "num " << num << " den " << den;
            ASSERT_EQ((round_split<L, P>(num / den, num % den, den)), want) << "num " << num << " den " << den;
        }
    // Near the limits: a numerator past int64, m·den past int64.
    constexpr std::uint64_t big = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    for (std::uint64_t den : {std::uint64_t{3}, std::uint64_t{1} << 20, (std::uint64_t{1} << 40) + 7, big / 5, big})
        for (std::uint64_t num :
             {big - 11, big, big + 1, big + 12345, std::numeric_limits<std::uint64_t>::max() - den})
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
} // namespace round_split_tests

// `%` and divmod on fractional lattices: a and b are whole numbers of g, the
// gcd of their value units, and the remainder a − round(a/b)·b lies on g·ℤ,
// exactly, under every rounding mode; q·b + r == a.
namespace lattice_modulo_tests {
using namespace beman::inside;

namespace {
template <class A, class B>
void check_all() {
    for (const A a : inside_range<grid_of<A>, flags_of<A>>{})
        for (const B b : inside_range<grid_of<B>, flags_of<B>>{}) {
            if (b == 0)
                continue;
            const exact ea{a}, eb{b};
            const auto  r     = a % b;
            const auto [q, m] = [&] {
                if constexpr (detail::expected_like<decltype(divmod(a, b))>)
                    return *divmod(a, b);
                else
                    return divmod(a, b);
            }();
            const exact got = [&] {
                if constexpr (detail::expected_like<decltype(r)>)
                    return exact{*r};
                else
                    return exact{r};
            }();
            constexpr auto mode = detail::rounding_of(flags_of<A> | flags_of<B> | detail::snap);
            const exact    t    = ea / eb;
            const exact    k    = mode == detail::round_mode::floor ? floor(t) : trunc(t); // the modes below
            ASSERT_EQ(got, ea - k * eb) << to_string(a) << " % " << to_string(b);
            ASSERT_EQ(exact{m}, got);
            ASSERT_EQ(exact{q} * eb + exact{m}, ea);
        }
}
} // namespace

TEST(LatticeModuloTest, remainders_are_exact) {
    using halves  = inside<{{0, 8}, per<2>}>;
    using thirds  = inside<{{1, 4}, per<3>}>;
    using fourths = inside<{{-3, 3}, per<4>}>;
    using shifted = inside<{{rational{-11, 4}, rational{9, 4}}, per<2>}>; // ±1/4 + k/2: unanchored
    check_all<halves, thirds>();
    check_all<fourths, thirds>();
    check_all<shifted, halves>();
    check_all<inside<{{-3, 3}, per<4>}, round_floor>, inside<{{-2, 2}, per<3>}, round_floor>>();
    // The remainder grid is the lattice g·ℤ, bounded as on integer grids.
    using r_t = decltype(inside<{{-3, 3}, per<4>}, round_floor>{} % thirds{1});
    static_assert(grid_of<r_t> == grid{{0, rational{47, 12}}, rational{1, 12}});
    using floored = inside<{{-3, 3}, per<4>}, round_floor>;
    EXPECT_EQ((rational{floored{-1.25} % thirds{rational{4, 3}}}), (rational{1, 12}));
    // A divisor grid holding 0: an expected, division_by_zero for 0.
    EXPECT_EQ((halves{2.5} % fourths{0}).error(), errc::division_by_zero);
    EXPECT_EQ(rational{*(halves{2.5} % fourths{0.75})}, (rational{1, 4}));
}
} // namespace lattice_modulo_tests

// Pinned defects: each case failed before its fix.
namespace pinned_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

template <class L, class R>
concept can_mod = requires(L l, R r) { l % r; };

// `%` under rounding: INT64_MIN % -1 divided natively (UB), and a quotient
// rounded away from zero made q·b pass int64. The remainder is now adjusted
// from C++'s `%` instead of formed as a − q·b.
TEST(DivisionTest, modulo_at_the_int64_limits) {
    constexpr std::int64_t i64_min = INT64_MIN;
    constexpr std::int64_t i64_max = INT64_MAX;
    static_assert([] {
        inside<{i64_min, i64_max}> a{i64_min};
        inside<{-3, -1}>           b{-1};
        return a % b == 0;
    }());
    static_assert([] {
        inside<{1, 2}> two{2};
        return inside<{0, i64_max}, round_ceil>{i64_max} % two == -1 &&
               inside<{0, i64_max}, round_nearest>{i64_max} % two == -1;
    }());
    static_assert([] {
        inside<{1, 3}> two{2};
        return inside<{-7, 7}, round_floor>{-7} % two == 1 && inside<{-7, 7}, round_ceil>{7} % two == -1;
    }());
}

// Quotient grid of a divisor spanning 0: the smallest divisor magnitude was
// taken as the notch, but an unanchored lattice comes closer to 0
// ({{-1.5, 1.5}, 1} holds ±0.5), so 10 / 0.5 = 20 escaped the [-10, 10] grid.
TEST(DivisionTest, quotient_grid_of_an_unanchored_divisor) {
    static_assert([] {
        auto q  = *(inside<{0, 10}>{10} / inside<{{-1.5, 1.5}, 1}>{0.5});
        using Q = decltype(q);
        return q == 20 && upper_of<Q> >= rational{20} && lower_of<Q> <= rational{-20};
    }());
    static_assert(!(grid{0, 10} / grid{{0, 1}, 0}).has_value()); // unbounded continuous divisor
}

// Fixed-point division on a 32-bit boundary: ja·K needs 32 value bits plus
// the sign, but the work type was int32.
TEST(DivisionTest, fixed_point_division_work_type) {
    using L = inside<{{0, rational{2147483647, 2}}, per<2>}, round_nearest>;
    using R = inside<{{rational{1, 2}, 1}, per<2>}>;
    static_assert(L{rational{2147483647, 2}} / R{rational{1, 2}} == 2147483647);
}

// INT64_MIN / -1 reaches 2^63: the quotient grid's endpoint wrapped in int64.
TEST(DivisionTest, quotient_grid_reaching_2_pow_63) {
    constexpr std::int64_t i64_min = INT64_MIN;
    static_assert([] {
        inside<{i64_min, 0}, round_toward_zero> a{i64_min};
        inside<{-2, -1}>                        b{-1};
        auto                                    q = a / b;
        using Q                                   = decltype(q);
        return rational(q) == rational{std::uint64_t{1} << 63} && upper_of<Q> == rational{std::uint64_t{1} << 63};
    }());
}

// Integer division by a divisor reaching INT64_MIN: the quotient grid was
// formed from 7/2^63, past the 64-bit rational (C++23 did not compile).
TEST(DivisionTest, integer_division_by_an_int64_min_divisor) {
    constexpr std::int64_t i64_min = INT64_MIN;
    constexpr std::int64_t i64_max = INT64_MAX;
    using full                     = inside<{i64_min, i64_max}>;
    using small                    = inside<{-7, 5}>;
    static_assert([] {
        auto q  = div(small{-7}, full{2}, round_toward_zero);
        auto qr = divmod(small{-7}, full{i64_min});
        return *q == -3_ins && qr->Quotient == 0_ins && qr->Remainder == -7_ins &&
               grid_of<decltype(*q)> == grid{-7, 7};
    }());
    // The range is the rounded corners, as the exact quotient grid gave it.
    static_assert(grid_of<decltype(div(small{}, std::declval<inside<{-3, 4}>>(), round_ceil))::value_type> ==
                  grid{-7, 7});
}

// A native integer quotient whose grid reaches 2^63 (INT64_MIN / −1): every
// non-negative quotient wrapped through from_value's int64 (10 / 2 gave
// 3.4e38), and INT64_MIN / −1 came back as INT64_MIN.
TEST(DivisionTest, integer_quotient_past_int64) {
    constexpr std::int64_t i64_min = INT64_MIN;
    constexpr std::int64_t i64_max = INT64_MAX;
    using full                     = inside<{i64_min, i64_max}>;
    static_assert(*div(full{10}, full{2}, round_toward_zero) == 5_ins);
    EXPECT_EQ(to_string(*div(full{i64_max}, full{1}, round_toward_zero)), "9223372036854775807");
    EXPECT_EQ(to_string(*div(full{i64_min}, full{-1}, round_floor)), "9223372036854775808");
    EXPECT_EQ(to_string(*div(full{i64_min}, inside<{-1, 1}>{-1}, round_toward_zero)), "9223372036854775808");
    const auto qr = divmod(full{i64_min}, full{-1}, round_ceil);
    EXPECT_EQ(to_string(qr->Quotient), "9223372036854775808");
    EXPECT_EQ(qr->Remainder, 0_ins);
}

// The Q-format path's 32-bit work type wrapped on its minimum over −Notch.
TEST(DivisionTest, q_format_division_of_the_minimum_by_minus_a_notch) {
    using q15_8  = inside<{{-32768, 32767}, per<256>}>;
    const auto r = div(q15_8{-32768}, q15_8{rational{-1, 256}}, round_toward_zero);
    EXPECT_EQ(to_string(*r), "8388608");
}

// An integer division past int64 ignored the rounding mode (the
// exact path returned a continuous quotient).
TEST(DivisionTest, integer_division_past_int64_rounds) {
    using I64             = std::int64_t;
    constexpr I64 I64_MAX = std::numeric_limits<I64>::max();
    constexpr I64 I64_MIN = std::numeric_limits<I64>::min();
    using u64             = inside<{0, UINT64_MAX}, round_toward_zero>;
    EXPECT_EQ(u64{7u} / u64{2u}, 3);
    EXPECT_EQ(div(uint64{7u}, uint64{2u}, round_floor), 3);
    EXPECT_EQ(div(uint64{UINT64_MAX}, inside<{-3, -1}>{-2}, round_ceil), rational(-I64_MAX));
    using i64     = inside<{I64_MIN, I64_MAX}, round_toward_zero>;
    const auto q1 = i64{-7} / i64{-1};
    using small   = inside<{1, 10}, round_toward_zero>;
    EXPECT_EQ(q1.value() / small{2}, 3);
    const auto d = uint64{0} - uint64{UINT64_MAX};
    EXPECT_EQ(div(d, inside<{1, 1000}>{7}, round_floor), rational(-2635249153387078803));
    using around_zero = inside<{-2, 3}, round_toward_zero>;
    EXPECT_EQ((u64{5u} / around_zero{0}).error(), errc::division_by_zero);
    // (a / b)·b + a % b == a, as for the native widths.
    const uint64                               a{18446744073709551557u};
    const inside<{1, 1000}, round_toward_zero> b{1000};
    const auto                                 quot = div(a, b, round_floor);
    EXPECT_EQ(exact{quot} * exact{1000} + exact{a % b}, exact{a});
}

TEST(DivisionTest, modulo_by_a_zero_grid_is_constrained) {
    using digit = inside<{0, 9}>;
    static_assert(!can_mod<digit, inside<{0}>>);
    static_assert(can_mod<digit, inside<{1, 9}>>);
}

//---------------------------------------------------------------------------
// division by zero.
//
// `÷0` reports through the error vocabulary: the return widens to
// expected<result, errc> when the divisor grid can be zero
// (errc::division_by_zero on a zero divisor), and the expected-lift carries
// that cause on through a chain.
//---------------------------------------------------------------------------
TEST(DivisionTest, division_by_zero_is_reported) {
    using N  = inside<{{1, 4}, per<1024>}>;
    using Dz = inside<{{0, 4}, per<1024>}>; // divisor grid spans zero

    auto q = N{3.0} / Dz{0.0};
    ASSERT_FALSE(q.has_value()); // an error — not inf
    ASSERT_EQ(q.error(), errc::division_by_zero);

    auto en = []() -> std::expected<N, errc> { return N{3.0}; };
    auto z  = en() / Dz{0.0};
    ASSERT_FALSE(z.has_value());
    ASSERT_EQ(z.error(), errc::division_by_zero);
}

//---------------------------------------------------------------------------
// Case 3 — Q-format fast path correctness
//---------------------------------------------------------------------------
// Q-format division: native_div_qformat matches rational arithmetic
TEST(DivisionTest, q_format_division_matches_rationals) {
    using fp = inside<{{0, 255}, 0x1p-8_r}>; // Q8.8; the `round_toward_zero` call policy
                                             // supplies the rounding for the native
                                             // path, without unsafe's skipped zero check

    // Spot checks against expected Q-format integer-truncation values.
    auto q1 = div(fp{200}, fp{8}, round_toward_zero);
    ASSERT_TRUE(q1.has_value());
    ASSERT_EQ(*q1, 25);

    auto q2 = div(fp{255}, fp{1}, round_toward_zero);
    ASSERT_TRUE(q2.has_value());
    ASSERT_EQ(*q2, 255);

    // Non-integer quotient: 200 / 3 ≈ 66.6667. Q-format multiplies before
    // dividing — (51200 * 256) / 768 = 17066 (= floor(66.6667 * 256)) — same
    // as native `(a << 8) / b`. NOT 66 * 256 = 16896 (that would be
    // truncate-then-scale, which loses fractional precision).
    auto q3 = div(fp{200}, fp{3}, round_toward_zero);
    ASSERT_TRUE(q3.has_value());
    ASSERT_EQ((*q3).raw(), 17066);

    // Divide by zero produces errc::division_by_zero.
    auto q4 = div(fp{1}, fp{0}, round_toward_zero);
    ASSERT_FALSE(q4.has_value());
    ASSERT_EQ(q4.error(), errc::division_by_zero);
}

// Q-format division: result type is Q-format (same notch as L)
TEST(DivisionTest, q_format_division_keeps_the_notch) {
    using fp = inside<{{0, 255}, 0x1p-8_r}, unsafe>;
    auto q   = div(fp{200}, fp{8}, round_toward_zero);
    using R  = std::remove_cvref_t<decltype(*q)>;
    static_assert(notch_of<R> == notch_of<fp>); // same Q-format, not rational-raw
    static_assert(!(rational_storage<R>));
}

// Point-operand divide — `x / just<c>` scales x's lattice by 1/|c| the same way:
// integer storage, a plain result, and sums of it stay plain.
TEST(DivisionTest, divide_by_point_keeps_integer_storage) {
    using U  = inside<{-100, 100}>;
    using R5 = decltype(U{} / 5_ins);
    static_assert(!rational_storage<R5>);
    static_assert(notch_of<R5> == rational{1, 5} && lower_of<R5> == -20 && upper_of<R5> == 20);
    using RN = decltype(U{} / just<-3, 2>);
    static_assert(notch_of<RN> == rational{2, 3} && lower_of<RN> == rational{-200, 3});
    using tenth = inside<{{-50, 150}, per<10>}>;
    using F     = decltype(tenth{} * 9_ins / 5_ins + 32_ins); // plain: no expected anywhere
    static_assert(notch_of<F> == rational{9, 50} && lower_of<F> == -58 && upper_of<F> == 302);
    static_assert(tenth{100.1_ins} * 9_ins / 5_ins + 32_ins == 212.18_ins);
    // A range divisor stays continuous.
    static_assert(rational_storage<decltype(U{} / inside<{1, 7}>{})>);
}

TEST(DivisionTest, divide_by_point_matches_exact_quotient) {
    for (int v : {-100, -7, 0, 1, 99, 100}) {
        inside<{-100, 100}> x{v};
        EXPECT_EQ(exact{x / just<3>}, exact{v} / exact{3});
        EXPECT_EQ(exact{x / just<-4>}, exact{v} / exact{-4});
        EXPECT_EQ((exact{x / just<-3, 2>}), (exact{v} * exact{rational{-2, 3}}));
        EXPECT_EQ(rational{x / just<3> * just<3>}, rational{v});
    }
}

//---------------------------------------------------------------------------
// Native div/mod narrowed to int32 — boundary values must match the 64-bit result.
//---------------------------------------------------------------------------
TEST(DivisionTest, int32_native_div_mod_matches_wide_reference) {
    constexpr std::int64_t M = 2147483647; // INT32_MAX; INT32_MIN is excluded
    using A                  = inside<{-M, M}>;
    using B                  = inside<{-(M / 2 + 2), M / 2 + 2}>;
    static_assert(std::same_as<native_div_t<A, B>, std::int32_t>);
    for (std::int64_t a : std::initializer_list<std::int64_t>{-M, -M + 1, -7, 0, 7, M - 1, M})
        for (std::int64_t b :
             std::initializer_list<std::int64_t>{-(M / 2 + 2), -(M / 2 + 1), -3, -1, 1, 3, M / 2 + 1, M / 2 + 2})
            for (round_mode m : {round_mode::trunc,
                                 round_mode::floor,
                                 round_mode::ceil,
                                 round_mode::nearest,
                                 round_mode::half_even}) {
                const std::int64_t q = div_rounded(a, b, m);
                EXPECT_EQ(div_rounded(static_cast<std::int32_t>(a), static_cast<std::int32_t>(b), m), q);
            }
    // q·b exceeds INT32_MAX here (ceil(M / (M/2 + 1)) = 2); mod must still be exact.
    A    a{M};
    B    b{M / 2 + 1};
    auto r = mod(a, b, make_policy<round_ceil>());
    EXPECT_EQ(rational{*r}, rational{M - 2 * (M / 2 + 1)});
}

} // namespace pinned_tests

// Arms no other test reaches, found by the reachability audit (gcov and an
// `if constexpr` probe over the suite).
namespace reached_arms_tests {
using namespace beman::inside;
using detail::rational;

// % and divmod on a lattice past int64 units (2^64 quarters) compute in wide
// integers; a divisor grid that holds 0 reports it.
TEST(DivisionArmTest, remainder_on_a_lattice_past_int64_units) {
    using L = inside<{{0, 4611686018427387904_g}, per<4>}>; // [0, 2^62]
    using Z = inside<{{0, 10}, per<2>}>;
    const L a{rational{(std::uint64_t{1} << 61) + 1, 4}}; // 2^59 + 1/4
    EXPECT_EQ((rational{a % inside<{{1, 10}, per<2>}>{1.5}}), (rational{3, 4}));
    EXPECT_EQ(rational{*(a % Z{test::at_run_time(1.5)})}, (rational{3, 4}));
    EXPECT_EQ((a % Z{test::at_run_time(0)}).error(), errc::division_by_zero);
    EXPECT_EQ(divmod(a, Z{test::at_run_time(0)}).error(), errc::division_by_zero);
}

// % on integers past int64 with a divisor grid that holds 0.
TEST(DivisionArmTest, remainder_of_wide_integers_by_a_grid_with_zero) {
    const inside<{0, 1180591620717411303424_g}> c{1000};
    using Z = inside<{0, 10}>;
    EXPECT_TRUE(*(c % Z{test::at_run_time(7)}) == 6);
    EXPECT_EQ((c % Z{test::at_run_time(0)}).error(), errc::division_by_zero);
}
} // namespace reached_arms_tests
