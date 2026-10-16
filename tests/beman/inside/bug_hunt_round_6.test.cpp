// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Defects from the sixth review round (a file-by-file read of include/). The
// static_asserts did not compile before the fixes; the runtime cases stored
// wrong values, threw, or reported the wrong error.

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>
#include <beman/inside/formats.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/predicates.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <expected>
#include <ranges>
#include <vector>

using namespace beman::inside;

// The expected sink of a policy ref unwrapped with .value(): an error threw
// bad_expected_access instead of going to the ref's errc&.
TEST(BugHuntRound6, policy_ref_expected_sink_reports_into_errc) {
    inside<{0, 100}>                    x{5};
    errc                                ec{};
    const std::expected<rational, errc> bad{std::unexpected{errc::overflow}};
    x.policy(ec) = bad;
    EXPECT_EQ(ec, errc::overflow);
    EXPECT_EQ(x, 5);
}

// A failed read-out through the errc& proxy returned an uninitialized
// target (a compile error in constant evaluation).
TEST(BugHuntRound6, policy_ref_failed_read_out_is_initialized) {
    static_assert([] {
        errc                 ec{};
        inside<{0, 100}>     x{50};
        const inside<{0, 9}> s = x.policy(ec); // 50 is past 9: overflow into ec
        return ec == errc::overflow && s == 0;
    }());
}

// floor / ceil / round / trunc of a value past INT64_MAX narrowed through
// imax.
TEST(BugHuntRound6, integer_roundings_past_int64) {
    const uint64 q{18446744073709551615u};
    EXPECT_EQ(math::floor(q).to<std::uint64_t>(), 18446744073709551615u);
    EXPECT_EQ(math::ceil(q).to<std::uint64_t>(), 18446744073709551615u);
    EXPECT_EQ(math::round(q).to<std::uint64_t>(), 18446744073709551615u);
    EXPECT_EQ(math::trunc(q).to<std::uint64_t>(), 18446744073709551615u);
}

// fmod's rational path divided loudly: a quotient past 64 bits stopped the
// build instead of taking the exact path.
TEST(BugHuntRound6, fmod_quotient_past_64_bits) {
    using X      = inside<{{0, 1000}, rational{1, 1024}}, round_nearest>;
    using Y      = inside<{{0, 1}, rational{1, 1ll << 60}}, round_nearest>;
    const auto r = math::fmod(X{rational{999999, 1024}}, Y{rational{17, 1ll << 60}});
    EXPECT_EQ(rational{*r}, (rational{15, 1ll << 60}));
}

// A wrap output of exp, exp2, sinh, cosh or pow_base stood in 2^(KMax+2) for
// a result past Out's range, and decide() took any two indices past it as
// one: the wrapped slot was wrong.
TEST(BugHuntRound6, math_wrap_output_folds_the_true_value) {
    using In   = inside<{{-20, 20}, rational{1, 16}}>;
    using Wide = inside<{{-rational{1ll << 40}, rational{1ll << 40}}, rational{1, 1024}}, round_nearest>;
    using Wrap = inside<{{-1, 1}, rational{1, 1024}}, round_nearest | wrap>;
    int bad    = 0;
    for (imax k = -320; k <= 320; k += 3) {
        const In x{rational{k, 16}};
        bad += !(wrap_cast<Wrap>(math::exp_into<Wide>(x)) == math::exp_into<Wrap>(x));
        bad += !(wrap_cast<Wrap>(math::exp2_into<Wide>(x)) == math::exp2_into<Wrap>(x));
        bad += !(wrap_cast<Wrap>(math::sinh_into<Wide>(x)) == math::sinh_into<Wrap>(x));
        bad += !(wrap_cast<Wrap>(math::cosh_into<Wide>(x)) == math::cosh_into<Wrap>(x));
        bad += !(wrap_cast<Wrap>(math::pow_base_into<Wide, 3>(x)) == math::pow_base_into<Wrap, 3>(x));
    }
    EXPECT_EQ(bad, 0);
    // pow, with a base reaching 0 (bounded: the exponent is not negative).
    using B = inside<{{0, 9}, rational{1, 4}}>;
    using P = inside<{{0, 6}, rational{1, 2}}>;
    for (int i = 0; i <= 36; i += 5)
        for (int e = 0; e <= 12; ++e) {
            const B b{rational{i, 4}};
            const P p{rational{e, 2}};
            bad += !(wrap_cast<Wrap>(*math::pow_into<Wide>(b, p)) == *math::pow_into<Wrap>(b, p));
        }
    EXPECT_EQ(bad, 0);
}

// rational compared with a double finer than 2^-62 through rational{double},
// which truncates it: 0 == 1e-30 held.
TEST(BugHuntRound6, rational_compares_exactly_with_fine_doubles) {
    const rational zero{0};
    EXPECT_FALSE(zero == 1e-30);
    EXPECT_TRUE(zero < 1e-30);
    EXPECT_TRUE(zero > -1e-30);
    const rational third{1, 3};
    EXPECT_FALSE(third == 1.0 / 3.0 + 0x1p-70);
    EXPECT_TRUE(rational{0x1p-62} < 0x1p-62 + 0x1p-80);
    EXPECT_TRUE(rational(1, 3ll << 60) > 0x1p-60 / 3); // the double rounds 1/3 down
    static_assert(rational{1, 4} == 0.25);
}

// The _g literal multiplied in grid_int without a limit: a long literal
// wrapped; trailing zeros counted as digits.
TEST(BugHuntRound6, grid_literal_takes_long_exact_literals) {
    static_assert(1.50000000000000000000000000000000000000000000000000000000000000000000000000000000000000_g ==
                  rational{3, 2});
    static_assert(100000000000000000000000000000000000000000000000000000000000000000000000000000000000e-83_g ==
                  rational{1});
    static_assert(0e500_g == rational{0});
    static_assert(18446744073709551615_g == rational{~umax{0}});
}

// The _r literal rejected 0x1p63 (which fits) and fine forms whose factors
// cancel; a decimal denominator past imax was checked before reducing.
TEST(BugHuntRound6, rational_literal_cancels_before_the_limits) {
    static_assert(0x1p63_r == rational{umax{1} << 63});
    static_assert(0x0.1p4_r == rational{1});
    static_assert(0x10p-66_r == rational{1, 1ll << 62});
    static_assert(0.5e-18_r == rational{1, 2'000'000'000'000'000'000ll});
}

// conversion_rounds divided loudly (a compile error for a quotient past 64
// bits) and guessed for doubles past the 64-bit rationals.
TEST(BugHuntRound6, predicates_decide_exactly) {
    using F = inside<{{0, 1}, rational{1, 1ll << 61}}>;
    using C = inside<{{0, 2}, rational{1, 1ll << 61}}, round_nearest>;
    using N = inside<{{0, 10}, rational{1, 3}}, round_nearest>;
    EXPECT_FALSE(conversion_rounds<F>(0x1p-61));
    EXPECT_TRUE(conversion_rounds<F>(0x1p-62));
    EXPECT_TRUE(conversion_overflows<N>(1e30));
    EXPECT_FALSE(conversion_rounds<N>(1e30));
    EXPECT_FALSE(conversion_overflows<N>(1e-300));
    EXPECT_TRUE(conversion_rounds<N>(1e-300));
    EXPECT_TRUE(conversion_rounds<N>(-1e-300)); // rounds onto 0
    static_assert(conversion_rounds<C>(rational{1, (1ll << 62) + 1}));
    static_assert(!conversion_rounds<C>(rational{3, 1ll << 61}));
    // A bounded continuous grid rounds before its range check: 22/21 rounds
    // onto 1 in sevenths (rounding, not overflow), as the exact path decides.
    using S = inside<grid{interval{0, 1}, max_denominator{7}}, round_nearest>;
    static_assert(conversion_rounds<S>(rational{22, 21}) && !conversion_overflows<S>(rational{22, 21}));
    static_assert(!conversion_rounds<S>(rational{3, 2}) && conversion_overflows<S>(rational{3, 2}));
}

// The sum of two grids whose value units have no 64-bit gcd (1/(3·2^60)
// against 1/(7·2^58) here: offset by 1/3 and 2/3) failed to compile although
// the sum grid fits.
TEST(BugHuntRound6, sum_with_units_past_the_64_bit_gcd) {
    using L      = inside<{{rational{1, 3}, rational{4, 3}}, rational{1, 1ll << 60}}>;
    using R      = inside<{{rational{2, 3}, rational{5, 3}}, rational{1, 7ll << 58}}>;
    const auto s = L{rational{1, 3}} + R{rational{2, 3}};
    EXPECT_EQ(s, 1);
    const auto t = L::from_raw(12345) + R::from_raw(999);
    EXPECT_EQ(to_string(t), "8070450532248019243/8070450532247928832");
}

// sum<T> over a random-access range whose end is a sentinel computed
// `end - it`, which does not compile.
namespace {
using element = inside<{0, 100}>;
struct zero_end { // the end of a range: the element 0
    friend bool operator==(std::vector<element>::const_iterator i, zero_end) { return *i == 0; }
};
} // namespace

TEST(BugHuntRound6, sum_over_a_sentinel_range) {
    const std::vector<element> v{element{1}, element{2}, element{3}, element{0}};
    const auto                 total = sum<inside<{0, 10000}>>(std::ranges::subrange(v.cbegin(), zero_end{}));
    EXPECT_EQ(total, 6);
}

#if !BEMAN_INSIDE_BIG_GRIDS
// A span past the 64-bit rationals was on the lattice only when both limits
// were multiples of the notch: odd limits 2 apart are too.
TEST(BugHuntRound6, divides_evenly_past_the_rational_range) {
    constexpr interval iv{rational{-9223372036854775807ll}, rational{18446744073709551615ull}};
    static_assert(iv.divides_evenly(rational{2}));
    static_assert(!iv.divides_evenly(rational{3}));
    static_assert(grid::try_make(iv, rational{2}).has_value());
}
#endif
