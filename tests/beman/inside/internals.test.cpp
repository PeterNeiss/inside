// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Building blocks: rational, big_int/big_rational, wide_int, the number
// parser and the overflow-checked builtins.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/casts.hpp>
#include <beman/inside/inside/detail/grid_rational.hpp>
#include <beman/inside/inside/detail/numbers.hpp>
#include <beman/inside/inside/detail/rational.hpp>
#include <beman/inside/inside/detail/wide_int.hpp>
#include <beman/inside/inside/formats.hpp>
#include <beman/inside/inside/grid.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/predicates.hpp>
#include <beman/inside/math.hpp>

#include <gtest/gtest.h>

#include <bit>
#include <cfloat>
#include <cmath>
#include <compare>
#include <cstdint>
#include <expected>
#include <limits>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support.hpp"

// rational
namespace rational_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
constexpr std::uint64_t M = std::numeric_limits<std::uint64_t>::max();
}

// rational construction normalizes
TEST(RationalTest, rational_construction_normalises) {
    {
        SCOPED_TRACE("zero numerator collapses denominator");
        ASSERT_EQ((rational{0u, 7}), (rational{0u, 1}));
        ASSERT_EQ((rational{0, -5}), (rational{0u, 1}));
        ASSERT_EQ((0_r).Den, 1);
    }

    {
        SCOPED_TRACE("negative denominator carries sign onto rational");
        rational a{1, -2};
        ASSERT_EQ(a.Num, 1u);
        ASSERT_EQ(a.Den, -2);
        ASSERT_TRUE(a < 0);
        ASSERT_EQ((rational{-1, 2}), (rational{1, -2}));
        ASSERT_EQ((rational{-1, -2}), (rational{1, 2}));
    }

    {
        SCOPED_TRACE("GCD reduction at construction");
        ASSERT_EQ((rational{6u, 8}), (rational{3u, 4}));
        ASSERT_EQ((rational{6, -8}), (rational{3, -4}));
        ASSERT_EQ((rational{100u, 25}), (rational{4u, 1}));
    }

    {
        SCOPED_TRACE("denominator zero: division_by_zero, as parse(\"1/0\")");
        ASSERT_THROW((void)((rational{1u, 0})), beman::inside::inside_error);
        ASSERT_THROW((void)((rational{1, 0})), beman::inside::inside_error);
        try {
            (void)rational{1, 0};
        } catch (const beman::inside::inside_error& e) {
            EXPECT_EQ(e.Code, errc::division_by_zero);
        }
    }

    {
        SCOPED_TRACE("denominator int64_min throws (cannot be negated without UB)");
        constexpr auto imin = std::numeric_limits<std::int64_t>::min();
        ASSERT_THROW((void)((rational{1u, imin})), beman::inside::inside_error);
        ASSERT_THROW((void)((rational{1, imin})), beman::inside::inside_error);
        // Negative-num path: the validation must run BEFORE the mem-init
        // negation, otherwise -int64_min would be signed overflow (UB).
        ASSERT_THROW((void)((rational{-1, imin})), beman::inside::inside_error);
    }

    {
        SCOPED_TRACE("from int min");
        constexpr rational a{std::numeric_limits<int>::min()};
        ASSERT_TRUE(a.Den < 0);
        ASSERT_TRUE(a < 0);
    }

    {
        SCOPED_TRACE("from double");
        ASSERT_EQ(rational{0.5}, (rational{1u, 2}));
        ASSERT_EQ(rational{-0.25}, (rational{1, -4}));
        ASSERT_EQ(rational{0.0}, (rational{0u, 1}));
    }

    {
        SCOPED_TRACE("user-defined literals");
        ASSERT_EQ(1_r, (rational{1u, 1}));
        ASSERT_EQ((3_r / 2), (rational{3u, 2}));
        ASSERT_EQ((-6_r / 16), (rational{-3, 8}));
        ASSERT_EQ(2.5_r, (rational{5u, 2}));
    }
}

// rational comparison
TEST(RationalTest, rational_comparison) {
    {
        SCOPED_TRACE("ordering");
        static_assert(-1_r < 0_r);
        static_assert(0_r < 1_r);
        static_assert(rational{-3, 2} < rational{-2, 3});
        static_assert(rational{1u, 2} < rational{2u, 3});
    }

    {
        SCOPED_TRACE("equality across normalisation");
        ASSERT_EQ((rational{2u, 4}), (rational{1u, 2}));
        ASSERT_EQ((rational{-2, 4}), (rational{1, -2}));
        ASSERT_EQ((rational{0u, 5}), (rational{0u, 9}));
    }

    {
        SCOPED_TRACE("comparison with arithmetic");
        ASSERT_TRUE((rational{3u, 2} > 1));
        ASSERT_TRUE((rational{1u, 2} < 1.0));
        ASSERT_EQ((rational{5u, 1}), 5);
    }

    {
        SCOPED_TRACE("comparison cross-cancels so big denominators don't always overflow");
        constexpr std::int64_t half = static_cast<std::int64_t>(M / 2);
        rational               a{static_cast<std::uint64_t>(half), half}; // -> 1
        rational               b{1u};
        ASSERT_EQ(a, b);
    }

    {
        SCOPED_TRACE("cross-multiplication uses 128-bit, never overflows");
        // M/2 vs (M-1)/3: numerators are consecutive (coprime) and denominators
        // {2,3} are coprime, so cross-cancellation cannot reduce either pair and the 64-bit
        // products overflow uint64 — but the 128-bit cross-multiply compares them
        // exactly. a - b = (M+2)/6 > 0, so a > b.
        rational a{M, 2};
        rational b{M - 1, 3};
        ASSERT_TRUE(a > b);
        ASSERT_TRUE(b < a);
        ASSERT_FALSE(a < b);
    }
}

// rational arithmetic
// Cross-multiplying in 128 bits is exact for every pair: an integer against
// a fraction with a huge denominator (a double's 19.99 is N/2^48), and
// magnitudes near 2^64, compare by value; zero against anything by sign.
TEST(RationalTest, rational_comparison_by_cross_multiplication) {
    const rational almost20{19.99};
    ASSERT_TRUE(almost20 < rational{20});
    ASSERT_TRUE(rational{19} < almost20);
    ASSERT_TRUE(-almost20 > rational{-20});
    ASSERT_TRUE(rational{-19} > -almost20);
    constexpr std::uint64_t big  = std::numeric_limits<std::uint64_t>::max();
    const rational          huge = rational_from_parts(big, 1), huge_half = rational_from_parts(big, 2);
    ASSERT_TRUE(huge_half < huge);
    ASSERT_TRUE(rational_from_parts(big - 1, 1) < huge);
    ASSERT_TRUE(rational_from_parts(big, 3) < rational_from_parts(big - 1, 2));
    ASSERT_TRUE(rational_from_parts(big, -3) > rational_from_parts(big - 1, -2));
    ASSERT_TRUE((rational_from_parts(6, 4) <=> rational{3, 2}) == 0); // unreduced, same value
    ASSERT_TRUE((rational{0} <=> rational_from_parts(0, -5)) == 0);
    ASSERT_TRUE((rational{0} < rational{1, std::numeric_limits<std::int64_t>::max()}));
    ASSERT_TRUE((rational{0} > rational{-1, std::numeric_limits<std::int64_t>::max()}));
}

TEST(RationalTest, rational_arithmetic) {
    {
        SCOPED_TRACE("add / sub / mul / div basic");
        static_assert(rational{3, 2} + rational{1, 5} == rational{17u, 10});
        static_assert(rational{3, 2} - rational{1, 5} == rational{13u, 10});
        static_assert(rational{3, 2} / rational{1, 2} == 3_r);
        static_assert((rational{3, 2} * rational{1, 2}) == rational{3u, 4});
    }

    {
        SCOPED_TRACE("zero arms");
        ASSERT_EQ(((rational{3u, 2} + 0_r)), (rational{3u, 2}));
        ASSERT_EQ(((0_r + rational{3u, 2})), (rational{3u, 2}));
        ASSERT_EQ(((rational{3u, 2} * 0_r)), 0_r);
        ASSERT_EQ(((0_r / rational{3u, 2})), 0_r);
    }

    {
        SCOPED_TRACE("negation and unary plus");
        ASSERT_EQ((-rational{3u, 4}), (rational{3, -4}));
        ASSERT_EQ(-0_r, 0_r); // -0 == 0
        ASSERT_EQ((+rational{3u, 4}), (rational{3u, 4}));
    }

    {
        SCOPED_TRACE("self-cancel returns zero");
        constexpr rational a{3u, 7};
        static_assert((a + (-a)) == 0_r);
        static_assert((a - a) == 0_r);
    }

    {
        SCOPED_TRACE("unchecked variants");
        static_assert(add_unchecked(2_r, 3_r) == 5_r);
        static_assert(mul_unchecked(2_r, 3_r) == 6_r);
        static_assert(div_unchecked(6_r, 3_r) == 2_r);
        static_assert(rational_inv(rational{2u, 3}) == rational{3u, 2});
    }

    // The checked operators return std::expected<rational, errc>; an implicit
    // converting constructor unwraps that into a plain rational so coefficient
    // expressions read as ordinary arithmetic (no .value()). Overflow is a
    // compile error in constant evaluation.
    {
        SCOPED_TRACE("expected<rational> unwraps implicitly into rational");
        constexpr rational two_x   = 2 * rational{3}; // int ⊗ rational
        constexpr rational half    = rational{3} / 2; // rational ⊗ int
        constexpr rational chained = rational{3, 2} - rational{1, 5};
        static_assert(two_x == 6_r);
        static_assert(half == rational{3u, 2});
        static_assert(chained == rational{13u, 10});

        // Bit-identical to the prior mul_unchecked form it replaces.
        static_assert(half == mul_unchecked(rational{3}, rational{1, 2}));

        // Assignment (not just copy-init) also unwraps.
        rational acc{0u, 1};
        acc = 2 * rational{3};
        ASSERT_EQ(acc, 6_r);
    }

    {
        SCOPED_TRACE("inv basic");
        static_assert(rational_inv(rational{3u, 2}) == rational{2u, 3});
        static_assert(rational_inv(rational{1u, 5}) == 5_r);
        static_assert(rational_inv(5_r) == rational{1u, 5});

        // Sign preservation (denominator carries the sign)
        static_assert(rational_inv(rational{-3, 2}) == rational{-2, 3});
        static_assert(rational_inv(rational{3, -2}) == rational{-2, 3});

        // Involutive
        static_assert(rational_inv(rational_inv(rational{7u, 11})) == rational{7u, 11});
    }

    {
        SCOPED_TRACE("division by zero -> division_by_zero");
        ASSERT_EQ(try_div(1_r, 0_r).error(), errc::division_by_zero);
    }

    {
        SCOPED_TRACE("compound-assign: rational RHS (compile time)");
        static_assert([] {
            rational a{3, 2};
            a += rational{1, 5};
            const bool add = a == rational{17u, 10};
            a -= rational{1, 5};
            const bool sub = a == rational{3u, 2};
            a *= rational{1, 2};
            const bool mul = a == rational{3u, 4};
            a /= rational{1, 2};
            return add && sub && mul && a == rational{3u, 2};
        }());
    }

    {
        SCOPED_TRACE("compound-assign: arithmetic RHS (compile time)");
        static_assert([] {
            rational a{3, 2};
            a += 1;
            const bool add = a == rational{5u, 2};
            a *= 2;
            const bool mul = a == 5_r;
            a /= 5;
            return add && mul && a == 1_r;
        }());
    }

    {
        SCOPED_TRACE("compound-assign: expected<rational> RHS unwraps the checked op");
        // rational * rational returns expected<rational>; += on that unwraps.
        static_assert([] {
            rational a{1, 2};
            a += rational{1, 3} * rational{6, 1};
            return a == rational{5u, 2};
        }());
        // An overflow is a compile error (fail/rational_overflow).
    }
}

// rational overflow detection
TEST(RationalTest, rational_overflow_detection) {
    {
        SCOPED_TRACE("the checked steps return errc::overflow");
        ASSERT_FALSE(try_add(rational{M}, 1_r).has_value());
        ASSERT_EQ(try_add(rational{M}, 1_r).error(), errc::overflow);
        ASSERT_FALSE(try_mul(rational{M}, 2_r).has_value());
        ASSERT_FALSE(try_div(rational{M}, rational{1u, 2}).has_value());
    }

    {
        SCOPED_TRACE("subtraction goes through add(-rhs)");
        // M - (-1) = M+1 -> overflow
        ASSERT_FALSE(try_sub(rational{M}, rational{1, -1}).has_value());
    }

    {
        SCOPED_TRACE("cross-cancellation avoids spurious overflow on common factors");
        // (M/5)/2 + (M/5)/2 — common denominator avoids cross-multiplication.
        constexpr auto small = rational{M / 5, 2};
        static_assert(small + small == rational{M / 5});
    }

    {
        SCOPED_TRACE("add cross-cancellation on unequal denominators avoids spurious overflow");
        // gcd(4, 6) = 2; lcm = 12. With cross-cancellation a_ad'=2, b_ad'=3, so
        // (M/5) * b_ad' = (M/5)*3 fits, whereas (M/5)*6 (no cross-cancellation) would
        // overflow.
        constexpr auto a = rational{M / 5, 4};
        constexpr auto b = rational{1u, 6};
        static_assert(a + b == rational{3 * (M / 5) + 2, 12});
    }

    {
        SCOPED_TRACE("add unequal-denominator value correctness");
        // Catches regressions in the lcm-based common-denominator computation.
        // 1/4 + 1/6 = 3/12 + 2/12 = 5/12         (gcd=2, lcm=12)
        static_assert((rational{1u, 4} + rational{1u, 6}) == rational{5u, 12});
        // 1/2 + 1/3 = 3/6 + 2/6 = 5/6            (gcd=1, lcm=6)
        static_assert((rational{1u, 2} + rational{1u, 3}) == rational{5u, 6});
        // 3/8 + 5/12 = 9/24 + 10/24 = 19/24      (gcd=4, lcm=24)
        static_assert((rational{3u, 8} + rational{5u, 12}) == rational{19u, 24});
        // mixed signs: 5/6 - 1/4 = 10/12 - 3/12 = 7/12
        static_assert((rational{5u, 6} + rational{1, -4}) == rational{7u, 12});
    }

    {
        SCOPED_TRACE("sub overflow returning errc::overflow");
        // -M - 1 would overflow on the negative side
        ASSERT_FALSE(try_sub(rational{M, -1}, 1_r).has_value());
    }

    {
        SCOPED_TRACE("checked arithmetic rejects denominators exceeding int64_max");
        // 2^62 * 3 = 1.5 * 2^63 — fits in uint64, exceeds int64_max.
        // (Coprime denominators so the cross-cancellation cannot reduce the lcm.)
        std::int64_t p62 = std::int64_t{1} << 62;

        // mul: a_ad * b_ad after cross-cancellation still > int64_max.
        ASSERT_FALSE(try_mul(rational{1u, p62}, rational{1u, 3}).has_value());

        // add: lcm > int64_max.
        ASSERT_FALSE(try_add(rational{1u, p62}, rational{1u, 3}).has_value());

        // 1/M (M > int64_max) would land M in the result's Den slot.
        ASSERT_FALSE(try_div(rational{1u, 1}, rational{M, 1}).has_value());
    }

    {
        SCOPED_TRACE("add to_string sees overflow boundary fall-through");
        ASSERT_EQ((beman::inside::to_string(rational{M, 2})), "9223372036854775807.5");
        ASSERT_EQ((beman::inside::to_string(rational{M / 5, 2})), "1844674407370955161.5");
    }
}

// rational helpers
TEST(RationalTest, rational_helpers) {
    {
        SCOPED_TRACE("abs");
        ASSERT_EQ((abs(rational{3, -4})), (rational{3u, 4}));
        ASSERT_EQ((abs(rational{3u, 4})), (rational{3u, 4}));
        ASSERT_EQ(abs(0_r), 0_r);
    }

    {
        SCOPED_TRACE("gcd");
        ASSERT_EQ((*gcd(rational{6u, 1}, rational{8u, 1})), (rational{2u, 1}));
        ASSERT_EQ((*gcd(rational{1u, 2}, rational{1u, 3})), (rational{1u, 6}));

        // Denominator-lcm overflow: lcm(2^62, 3) = 3 * 2^62 > int64_max.
        ASSERT_FALSE((gcd(rational{1u, std::int64_t{1} << 62}, rational{1u, 3}).has_value()));
    }

    {
        SCOPED_TRACE("divides_evenly");
        ASSERT_TRUE((divides_evenly(rational{6u, 1}, rational{2u, 1})));
        ASSERT_FALSE((divides_evenly(rational{6u, 1}, rational{4u, 1})));
        ASSERT_TRUE((divides_evenly(rational{1u, 2}, rational{1u, 4})));
        // by convention divisor==0 returns true
        ASSERT_TRUE((divides_evenly(rational{6u, 1}, 0_r)));
    }

    {
        SCOPED_TRACE("divides_evenly is alignment-only, independent of representability");
        // M / (1/2) = M*2 is mathematically an integer (M sits on the 1/2 lattice),
        // so divides_evenly is true even though the quotient numerator overflows
        // uint64. Representability of the index count is a separate concern
        // (grid::max_index_representable), not part of the lattice-alignment predicate.
        ASSERT_TRUE((divides_evenly(rational{M}, rational{1, 2})));
        // genuinely unaligned stays false (no overflow involved)
        ASSERT_FALSE((divides_evenly(rational{1u, 2}, rational{1u, 3})));
    }
}

// rational conversion to integer/float
TEST(RationalTest, rational_conversion_to_integer_float) {
    {
        SCOPED_TRACE("to_unsigned floors, rejects negatives");
        ASSERT_TRUE((static_cast<unsigned>(rational{7u, 2}) == 3u));
        ASSERT_THROW((void)(static_cast<unsigned>(rational{7, -2})), beman::inside::inside_error);
    }

    {
        SCOPED_TRACE("to_signed rounds toward zero (operator T)");
        ASSERT_TRUE((static_cast<int>(rational{7u, 2}) == 3));
        ASSERT_TRUE((static_cast<int>(rational{7, -2}) == -3));
    }

    {
        SCOPED_TRACE("to_double");
        ASSERT_TRUE((static_cast<double>(rational{1u, 2}) == 0.5));
        ASSERT_TRUE((static_cast<double>(rational{1u, -2}) == -0.5));
        ASSERT_TRUE(static_cast<double>(0_r) == 0.0);
    }

    {
        SCOPED_TRACE("to<T> reports domain_error for negative rational");
        rational pos{5u, 2};
        rational neg{5, -2};
        ASSERT_TRUE(pos.to<unsigned>().value() == 2u);
        auto r = neg.to<unsigned>();
        ASSERT_FALSE(r.has_value());
        ASSERT_EQ(r.error(), errc::domain_error);
    }
}

// rational trunc / floor / round
TEST(RationalTest, rational_trunc_floor_round) {
    {
        SCOPED_TRACE("trunc — toward zero");
        ASSERT_EQ((trunc(rational{7u, 2})), 3);  //  3.5 -> 3
        ASSERT_EQ((trunc(rational{7, -2})), -3); // -3.5 -> -3
        ASSERT_EQ((trunc(rational{1u, 2})), 0);
        ASSERT_EQ((trunc(rational{1, -2})), 0);
        ASSERT_EQ((trunc(rational{4u, 1})), 4);
        ASSERT_EQ(trunc((0_r)), 0);
    }

    {
        SCOPED_TRACE("floor — toward -inf");
        ASSERT_EQ((floor(rational{7u, 2})), 3);  //  3.5 -> 3
        ASSERT_EQ((floor(rational{7, -2})), -4); // -3.5 -> -4
        ASSERT_EQ((floor(rational{1u, 2})), 0);
        ASSERT_EQ((floor(rational{1, -2})), -1); // -0.5 -> -1
        ASSERT_EQ((floor(rational{4u, 1})), 4);
        ASSERT_EQ((floor(rational{4, -1})), -4); // exact integer: no step
        ASSERT_EQ(floor((0_r)), 0);
    }

    {
        SCOPED_TRACE("ceil — toward +inf");
        ASSERT_EQ((ceil(rational{7u, 2})), 4);  //  3.5 ->  4
        ASSERT_EQ((ceil(rational{7, -2})), -3); // -3.5 -> -3
        ASSERT_EQ((ceil(rational{1u, 2})), 1);  //  0.5 ->  1
        ASSERT_EQ((ceil(rational{1, -2})), 0);  // -0.5 ->  0
        ASSERT_EQ((ceil(rational{4u, 1})), 4);  // exact integer: no step
        ASSERT_EQ((ceil(rational{4, -1})), -4);
        ASSERT_EQ(ceil((0_r)), 0);
    }

    {
        SCOPED_TRACE("round — half away from zero");
        ASSERT_EQ((round(rational{1u, 2})), 1);  //  0.5 ->  1
        ASSERT_EQ((round(rational{1, -2})), -1); // -0.5 -> -1
        ASSERT_EQ((round(rational{3u, 2})), 2);  //  1.5 ->  2
        ASSERT_EQ((round(rational{3, -2})), -2); // -1.5 -> -2
        ASSERT_EQ((round(rational{1u, 4})), 0);  //  0.25 -> 0
        ASSERT_EQ((round(rational{1u, 3})), 0);  //  ~0.33 -> 0
        ASSERT_EQ((round(rational{2u, 3})), 1);  //  ~0.67 -> 1
        ASSERT_EQ((round(rational{2, -3})), -1);
        ASSERT_EQ(round((0_r)), 0);
    }
}

//---------------------------------------------------------------------------
// Mixed-sign addition rescued in 128-bit. `1024 − m/2^54` forms the
// cross-product 1024·2^54 == 2^64 (one past uint64) before the subtraction
// brings the numerator back into range — the dbl-engine store path hit
// exactly this via `rhs - Lower` and terminated through noexcept.
//---------------------------------------------------------------------------
// mixed-sign add rescues a cross-product overflow when the difference fits
TEST(RationalTest, mixed_sign_add_rescues_a_cross_product_overflow_when_the_difference_fits) {
    // -0.49996929771979287 as an exact double fraction: den 2^54.
    const rational fp_value{std::uint64_t{9006646171630191}, std::int64_t{-18014398509481984}};

    const auto offset = try_add(fp_value, rational{1024});
    ASSERT_TRUE(offset.has_value());
    ASSERT_TRUE(offset->Num == std::uint64_t{18437737427537921425u});
    ASSERT_TRUE(offset->Den == std::int64_t{18014398509481984});

    // Same-sign sums past uint64 stay an error (the result truly needs > 64 bits).
    const rational positive{std::uint64_t{9006646171630191}, std::int64_t{18014398509481984}};
    ASSERT_FALSE(try_add(positive, rational{1024}).has_value());
}
} // namespace rational_tests

// big_int / big_rational: grid numbers of any size.
namespace big_rational_tests {
using namespace beman::inside::detail;

using std::int64_t;
using std::uint64_t;

namespace {
// 2^k as a big_int, built by doubling (every step is exact).
consteval big_int pow2(int k) {
    big_int v{1};
    for (int i = 0; i < k; ++i)
        v = v * big_int{2};
    return v;
}

// Values are canonical and interned: equal values are equal structurally,
// so they are the same template argument.
template <big_int V>
struct tag {};
static_assert(std::is_same_v<tag<pow2(100)>, tag<pow2(50) * pow2(50)>>);
static_assert(!std::is_same_v<tag<pow2(100)>, tag<pow2(101)>>);
static_assert(pow2(63).fits_limb() && !pow2(64).fits_limb());
static_assert((pow2(64) - big_int{1}).fits_limb()); // shrinks back inline

// Arithmetic against identities.
static_assert((pow2(200) + pow2(100)) - pow2(200) == pow2(100));
static_assert(pow2(200) / pow2(70) == pow2(130));
static_assert((pow2(200) + big_int{5}) % pow2(70) == big_int{5});
static_assert(-pow2(100) < big_int{0} && -pow2(100) < -pow2(99));
static_assert(gcd(pow2(150) * big_int{3}, pow2(120) * big_int{9}) == pow2(120) * big_int{3});
static_assert(pow2(130).bit_width() == 131);
static_assert(static_cast<std::uint64_t>(pow2(64) + big_int{7}) == 7); // truncates like a builtin
static_assert(static_cast<wide_uint<3>>(pow2(130)) == (wide_uint<3>{1} << 130));
static_assert(big_int{wide_sint<3>{-1} << 130} == -pow2(130));

// big_rational: canonical fractions of any size.
constexpr big_rational huge{pow2(100) * big_int{3}, pow2(40) * big_int{6}}; // = 2^59
static_assert(huge == big_rational{pow2(59)} && huge.is_integer() && huge.fits_rational());
constexpr big_rational fine{big_int{1}, pow2(90)}; // 2^-90
static_assert(!fine.fits_rational());
static_assert(fine * big_rational{pow2(90)} == big_rational{1});
static_assert(fine + fine == big_rational{big_int{1}, pow2(89)});
static_assert(fine < big_rational{rational{1, 1024}});
static_assert(gcd(big_rational{rational{1, 4}}, big_rational{rational{1, 6}}) == big_rational{rational{1, 12}});
static_assert(big_rational{0x1p100} == big_rational{pow2(100)});
static_assert(big_rational{0x1p-100} == big_rational{big_int{1}, pow2(100)});

// The implicit 64-bit view of a small value.
constexpr rational r = big_rational{rational{-3, 7}};
static_assert(r == rational{-3, 7});
static_assert(big_rational{rational{5, 2}} == rational{5, 2}); // mixed comparison

// Mixed arithmetic with built-in integers: unambiguous (an int converts to
// both big_rational and rational), on either side.
constexpr big_rational p100{pow2(100)};
static_assert(p100 - 1 == big_rational{pow2(100) - big_int{1}});
static_assert(1 - p100 == -(p100 - 1));
static_assert(p100 + 2u == 2u + p100 && (p100 + 2u) - p100 == 2);
static_assert(p100 * 3 == 3 * p100 && (p100 * 3) / 3 == p100);
static_assert(1 / big_rational{2} == big_rational{rational{1, 2}} && fine / -2 == -(fine * rational{1, 2}));
} // namespace

TEST(BigRationalTest, runtime_small_values) {
    // The operators are consteval; the same arithmetic by name computes small
    // values at runtime without allocation (the grid constructors, printing).
    volatile int  x = 7;
    const big_int a{static_cast<int>(x)}, b{-3};
    EXPECT_TRUE(big_int::mul(a, b) == big_int{-21});
    EXPECT_TRUE((big_rational{a, big_int{14}} == big_rational{rational{1, 2}}));
    EXPECT_TRUE(static_cast<rational>(big_rational{a}) == rational{7});
}

TEST(BigRationalTest, runtime_comparison_of_small_values_forms_no_big_value) {
    // (2^64 − 1)/4 against 1/3: the cross products need 128 bits. A runtime
    // comparison must not try to form them as big values (which only exist at
    // compile time) — a store's range check runs exactly this.
    volatile std::uint64_t top = ~std::uint64_t{0};
    const big_rational     a{rational{static_cast<std::uint64_t>(top), 4}};
    const big_rational     b{rational{1, 3}};
    EXPECT_TRUE(b < a);
    EXPECT_TRUE(big_rational::neg(a) < b);
    EXPECT_FALSE(a < a);
    EXPECT_TRUE(a > 5);
}

TEST(BigRationalTest, runtime_overflow_is_reported) {
    volatile std::uint64_t big = ~std::uint64_t{0};
    EXPECT_THROW(
        (void)big_int::mul(big_int{static_cast<std::uint64_t>(big)}, big_int{static_cast<std::uint64_t>(big)}),
        beman::inside::inside_error);
}
} // namespace big_rational_tests

// wide_int against builtin oracles. Small limbs (uint8_t, uint16_t) make the
// multi-limb paths — carries, Knuth D's q̂ correction and add-back, shifts
// across limbs — reachable with exhaustive or dense sweeps whose results a
// builtin integer can check exactly.
namespace wide_int_tests {
using namespace beman::inside::detail;

namespace {
// Values that stress limb boundaries at the given width, plus a dense stride.
template <typename U>
std::vector<U> probe_values() {
    constexpr int  B = std::numeric_limits<U>::digits;
    std::vector<U> v;
    for (int b = 0; b < B; ++b) {
        const U p = static_cast<U>(U{1} << b);
        for (U d : {U(0), U(1), U(2)}) {
            v.push_back(static_cast<U>(p + d));
            v.push_back(static_cast<U>(p - d));
            v.push_back(static_cast<U>(~p + d));
        }
    }
    const U step = static_cast<U>(std::numeric_limits<U>::max() / 251);
    for (U x = 0; x < static_cast<U>(std::numeric_limits<U>::max() - step); x = static_cast<U>(x + step))
        v.push_back(x);
    v.push_back(std::numeric_limits<U>::max());
    return v;
}

// Deterministic 64-bit generator (splitmix64).
struct rng {
    std::uint64_t State;
    std::uint64_t operator()() {
        std::uint64_t z = (State += 0x9e3779b97f4a7c15ull);
        z               = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z               = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
};

// Check every operation of W (unsigned, `bits` wide) against the builtin U,
// and of its signed twin against S, over all pairs from `vals`.
template <typename WU, typename WS, typename U, typename S>
void check_against(const std::vector<U>& vals) {
    constexpr int B = std::numeric_limits<U>::digits;
    for (U a : vals)
        for (U b : vals) {
            const WU wa{a}, wb{b};
            ASSERT_EQ(static_cast<U>(wa + wb), static_cast<U>(a + b)) << a << " + " << b;
            ASSERT_EQ(static_cast<U>(wa - wb), static_cast<U>(a - b)) << a << " - " << b;
            // (Promote through uint64 first: uint16 * uint16 is an int product.)
            const U ab = static_cast<U>(static_cast<std::uint64_t>(a) * b);
            ASSERT_EQ(static_cast<U>(wa * wb), ab) << a << " * " << b;
            ASSERT_EQ(wa < wb, a < b);
            ASSERT_EQ(wa == wb, a == b);
            if (b != 0) {
                ASSERT_EQ(static_cast<U>(wa / wb), static_cast<U>(a / b)) << a << " / " << b;
                ASSERT_EQ(static_cast<U>(wa % wb), static_cast<U>(a % b)) << a << " % " << b;
            }
            const S  sa = static_cast<S>(a), sb = static_cast<S>(b);
            const WS xa{sa}, xb{sb};
            ASSERT_EQ(xa < xb, sa < sb) << sa << " < " << sb;
            ASSERT_EQ(static_cast<S>(xa * xb), static_cast<S>(ab));
            // Builtin S / S overflows for min / −1; wide_int wraps.
            if (sb != 0 && !(sa == std::numeric_limits<S>::min() && sb == -1)) {
                ASSERT_EQ(static_cast<S>(xa / xb), static_cast<S>(sa / sb)) << sa << " / " << sb;
                ASSERT_EQ(static_cast<S>(xa % xb), static_cast<S>(sa % sb)) << sa << " % " << sb;
            }
        }
    for (U a : vals)
        for (int s = 0; s < B; ++s) {
            ASSERT_EQ(static_cast<U>(WU{a} << s), static_cast<U>(static_cast<std::uint64_t>(a) << s));
            ASSERT_EQ(static_cast<U>(WU{a} >> s), static_cast<U>(a >> s));
            const S sa = static_cast<S>(a);
            ASSERT_EQ(static_cast<S>(WS{sa} >> s), static_cast<S>(sa >> s)); // arithmetic
        }
}
} // namespace

TEST(WideIntTest, two_uint8_limbs_against_uint16) {
    check_against<wide_uint<2, std::uint8_t>, wide_sint<2, std::uint8_t>, std::uint16_t, std::int16_t>(
        probe_values<std::uint16_t>());
}

TEST(WideIntTest, four_uint8_limbs_against_uint32) {
    check_against<wide_uint<4, std::uint8_t>, wide_sint<4, std::uint8_t>, std::uint32_t, std::int32_t>(
        probe_values<std::uint32_t>());
}

TEST(WideIntTest, four_uint16_limbs_against_uint64) {
    BEMAN_INSIDE_RELEASE_ONLY();
    check_against<wide_uint<4, std::uint16_t>, wide_sint<4, std::uint16_t>, std::uint64_t, std::int64_t>(
        probe_values<std::uint64_t>());
}

// Exhaustive 16-bit division over uint8_t limbs: every dividend against a
// dense divisor set covers each q̂ correction and add-back case.
TEST(WideIntTest, exhaustive_division_two_uint8_limbs) {
    BEMAN_INSIDE_RELEASE_ONLY();
    using W = wide_uint<2, std::uint8_t>;
    for (unsigned b = 1; b <= 0xffff; b += (b < 1024 ? 1 : 97))
        for (unsigned a = 0; a <= 0xffff; ++a) {
            const auto r = W::divmod(W{a}, W{b});
            ASSERT_EQ(static_cast<unsigned>(r.Quotient), a / b) << a << " / " << b;
            ASSERT_EQ(static_cast<unsigned>(r.Remainder), a % b) << a << " % " << b;
        }
}

#if defined(__SIZEOF_INT128__)
namespace {
// __int128 is not std::integral in strict modes: go through the limbs.
using u128n = unsigned __int128;
template <bool S>
wide_int<2, S> from128(u128n v) {
    wide_int<2, S> w;
    w.Word[0] = static_cast<std::uint64_t>(v);
    w.Word[1] = static_cast<std::uint64_t>(v >> 64);
    return w;
}
template <bool S>
u128n to128(const wide_int<2, S>& w) {
    return (u128n{w.Word[1]} << 64) | w.Word[0];
}
} // namespace

TEST(WideIntTest, two_uint64_limbs_against_int128) {
    using U = u128n;
    using S = __int128;
    rng  next{42};
    auto pick = [&]() -> U {
        // Mix full-width, half-width and near-limb-boundary values.
        switch (next() % 4) {
        case 0:
            return (U{next()} << 64) | next();
        case 1:
            return U{next()};
        case 2:
            return (U{1} << (next() % 128)) + (next() % 5) - 2;
        default:
            return (U{next() >> (next() % 64)} << 64) | next();
        }
    };
    for (int i = 0; i < 200000; ++i) {
        const U    a = pick(), b = pick();
        const auto wa = from128<false>(a), wb = from128<false>(b);
        ASSERT_TRUE(to128(wa + wb) == a + b);
        ASSERT_TRUE(to128(wa - wb) == a - b);
        ASSERT_TRUE(to128(wa * wb) == a * b);
        ASSERT_EQ(wa < wb, a < b);
        if (b != 0) {
            ASSERT_TRUE(to128(wa / wb) == a / b);
            ASSERT_TRUE(to128(wa % wb) == a % b);
        }
        const S    sa = static_cast<S>(a), sb = static_cast<S>(b);
        const auto xa = from128<true>(a), xb = from128<true>(b);
        ASSERT_EQ(xa < xb, sa < sb);
        if (sb != 0 && sb != -1) {
            ASSERT_TRUE(static_cast<S>(to128(xa / xb)) == sa / sb);
            ASSERT_TRUE(static_cast<S>(to128(xa % xb)) == sa % sb);
        }
    }
}
#endif

// Three limbs have no builtin oracle: check the division identity and the
// round trip through two-limb values.
TEST(WideIntTest, three_uint64_limbs_division_identity) {
    using W = wide_sint<3>;
    rng next{7};
    for (int i = 0; i < 100000; ++i) {
        W a, b;
        for (auto& w : a.Word)
            w = next();
        for (auto& w : b.Word)
            w = next();
        b = b >> static_cast<int>(next() % 191);
        if (b.is_zero())
            continue;
        const auto r = W::divmod(a, b);
        ASSERT_TRUE(r.Quotient * b + r.Remainder == a);
        // |remainder| < |divisor|, and the remainder takes the dividend's sign.
        const W ar = r.Remainder.negative() ? -r.Remainder : r.Remainder;
        const W ab = b.negative() ? -b : b;
        ASSERT_TRUE(wide_uint<3>(ar) < wide_uint<3>(ab));
        ASSERT_TRUE(r.Remainder.is_zero() || r.Remainder.negative() == a.negative());
    }
}

TEST(WideIntTest, conversions) {
    using U2 = wide_uint<2>;
    using S2 = wide_sint<2>;
    using S3 = wide_sint<3>;
    // Sign extension from builtins and between widths.
    EXPECT_EQ(static_cast<std::int64_t>(S3{S2{-7}}), -7);
    EXPECT_TRUE(S3{S2{-1}} == S3{-1});
    EXPECT_TRUE(S3{U2{~0ull}} == S3{~0ull});
    EXPECT_EQ(static_cast<std::uint64_t>(U2{-1} >> 64), ~0ull); // -1 sign-extends to all ones
    // Truncation to builtins keeps the low bits.
    EXPECT_EQ(static_cast<std::uint8_t>(U2{0x1234}), 0x34u);
    EXPECT_EQ(static_cast<std::int32_t>(S2{-2}), -2);
    // Nearest double, ties to even, sticky bits honoured.
    EXPECT_EQ(static_cast<double>(U2{1} << 100), 0x1p100);
    EXPECT_EQ(static_cast<double>((U2{1} << 100) + U2{1}), 0x1p100);
    EXPECT_EQ(static_cast<double>((U2{1} << 53) + U2{1}), 0x1p53);                            // tie → even
    EXPECT_EQ(static_cast<double>((U2{1} << 120) + (U2{1} << 67) + U2{1}), 0x1p120 + 0x1p68); // tie broken by sticky
    EXPECT_EQ(static_cast<double>(S2{-3}), -3.0);
    EXPECT_EQ(static_cast<double>(std::numeric_limits<S2>::min()), -0x1p127);
}

TEST(WideIntTest, limits_and_bit_width) {
    using S2 = wide_sint<2>;
    using U3 = wide_uint<3>;
    EXPECT_EQ(std::numeric_limits<S2>::digits, 127);
    EXPECT_EQ(std::numeric_limits<U3>::digits, 192);
    EXPECT_TRUE(std::numeric_limits<S2>::max() + S2{1} == std::numeric_limits<S2>::min());
    EXPECT_EQ(bit_width_of(U3{0}), 0);
    EXPECT_EQ(bit_width_of(U3{1} << 130), 131);
    EXPECT_EQ(bit_width_of(std::numeric_limits<U3>::max()), 192);
}

TEST(WideIntTest, shifts_saturate_past_width) {
    using S2 = wide_sint<2>;
    EXPECT_TRUE((S2{5} << 128).is_zero());
    EXPECT_TRUE((S2{-5} >> 200) == S2{-1});
    EXPECT_TRUE((S2{5} >> 200).is_zero());
}

// Everything works in constant evaluation.
namespace {
using U2         = wide_uint<2>;
using S2         = wide_sint<2>;
constexpr U2 big = (U2{1} << 127) + U2{12345};
static_assert(big / U2{3} * U2{3} + big % U2{3} == big);
static_assert(S2{-5} / S2{2} == S2{-2} && S2{-5} % S2{2} == S2{-1});
static_assert(static_cast<double>(U2{1} << 100) == 0x1p100);
static_assert(std::numeric_limits<S2>::min() < S2{0});
static_assert(std::is_trivially_copyable_v<U2> && std::is_standard_layout_v<U2>);

template <U2 V>
struct nttp {
    static constexpr U2 value = V;
};
static_assert(nttp<big>::value == big); // structural
} // namespace

// GCC 15/16 miscompare a defaulted == over an array member in constant
// evaluation: `a == x && b == y` came out true with b != y (seen as √(1/2)
// "exact" in the math engine's compile-time path). wide_int writes == out.
TEST(WideIntTest, equality_in_constant_evaluation) {
    using W = beman::inside::detail::wide_sint<3>;
    static_assert(![] {
        const W n{1}, d{1};
        return n == W{1} && d == W{2};
    }());
    static_assert([] {
        const W n{1}, d{2};
        return n == W{1} && d == W{2};
    }());
    SUCCEED();
}

using std::uint64_t;

// The 64-bit limb product and division: native __int128, or the portable
// split on targets without it. Every path gives the same {hi, lo} and
// {quotient, remainder}.
namespace {
struct limb_case {
    std::uint64_t A, B;
};
constexpr std::uint64_t ones         = std::numeric_limits<std::uint64_t>::max();
constexpr limb_case     limb_cases[] = {
    {0, 0},
    {1, ones},
    {ones, ones},
    {ones, 2},
    {std::uint64_t{1} << 63, std::uint64_t{1} << 63},
    {std::uint64_t{1} << 32, std::uint64_t{1} << 32},
    {0xffffffffu, 0xffffffffu},
    {0x123456789abcdef0u, 0x0fedcba987654321u},
};
constexpr bool limb_ops_agree(limb_case c) {
    const limb::pair<std::uint64_t> p = limb::mul(c.A, c.B);
    if (c.B == 0)
        return p.Hi == 0 && p.Lo == 0;
    // {hi, lo} / B gives back A, remainder 0; hi < B holds because A·B / B = A.
    const limb::pair<std::uint64_t> q = limb::div(p.Hi, p.Lo, c.B);
    return q.Hi == c.A && q.Lo == 0;
}
} // namespace

TEST(WideIntTest, limb_mul_and_div_paths_agree) {
    static_assert([] {
        for (const limb_case c : limb_cases)
            if (!limb_ops_agree(c))
                return false;
        return true;
    }());
    for (const limb_case c : limb_cases) {
        volatile std::uint64_t a = c.A, b = c.B; // keep the run-time path
        const limb_case        rt{a, b};
        EXPECT_TRUE(limb_ops_agree(rt)) << c.A << " * " << c.B;
        const limb::pair<std::uint64_t> p  = limb::mul(rt.A, rt.B);
        const limb::pair<std::uint64_t> pc = limb::mul(c.A, c.B);
        EXPECT_EQ(p.Hi, pc.Hi);
        EXPECT_EQ(p.Lo, pc.Lo);
    }
    // hi = d − 1, the largest high word div accepts.
    volatile std::uint64_t          d    = 0x8000000000000001u;
    const limb::pair<std::uint64_t> q    = limb::div(d - 1, ones, std::uint64_t{d});
    const limb::pair<std::uint64_t> back = limb::mul(q.Hi, std::uint64_t{d});
    // q·d + r == {d − 1, ones}, and r < d.
    EXPECT_LT(q.Lo, std::uint64_t{d});
    const std::uint64_t lo = back.Lo + q.Lo;
    EXPECT_EQ(back.Hi + (lo < back.Lo), std::uint64_t{d} - 1);
    EXPECT_EQ(lo, ones);
}
} // namespace wide_int_tests

// numbers
namespace numbers_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

// safe_abs handles INT_MIN without UB
TEST(NumbersTest, safe_abs_handles_int_min_without_ub) {
    static_assert(safe_abs(std::int64_t{0}) == 0u);
    static_assert(safe_abs(std::int64_t{42}) == 42u);
    static_assert(safe_abs(std::int64_t{-42}) == 42u);

    // INT_MIN: -INT_MIN as signed would overflow; safe_abs computes via uint64
    constexpr std::int64_t min_v = std::numeric_limits<std::int64_t>::min();
    static_assert(safe_abs(min_v) == static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1u);
}

// frexp handles zero
TEST(NumbersTest, frexp_handles_zero) {
    int    e = 99;
    double r = beman::inside::detail::frexp(0.0, &e);
    ASSERT_EQ(r, 0.0);
    ASSERT_EQ(e, 0);

    // negative zero
    e         = 99;
    double rn = beman::inside::detail::frexp(-0.0, &e);
    ASSERT_EQ(rn, 0.0);
    ASSERT_EQ(e, 0);
}

// frexp handles infinity and NaN
TEST(NumbersTest, frexp_handles_infinity_and_nan) {
    int    e   = 99;
    double inf = std::numeric_limits<double>::infinity();
    double r   = beman::inside::detail::frexp(inf, &e);
    ASSERT_TRUE(std::isinf(r));
    ASSERT_EQ(e, 0);

    e          = 99;
    double nan = std::numeric_limits<double>::quiet_NaN();
    double rn  = beman::inside::detail::frexp(nan, &e);
    ASSERT_TRUE(std::isnan(rn));
    ASSERT_EQ(e, 0);
}

// frexp matches std::frexp on normals
TEST(NumbersTest, frexp_matches_std_frexp_on_normals) {
    for (double v : {1.0, 0.5, 0.25, 1.5, 1024.0, 0.001, -7.25}) {
        int    be = 0, se = 0;
        double br = beman::inside::detail::frexp(v, &be);
        double sr = std::frexp(v, &se);
        ASSERT_EQ(br, sr);
        ASSERT_EQ(be, se);
    }
}

// frexp on subnormal scales up via recursion
TEST(NumbersTest, frexp_on_subnormal_scales_up_via_recursion) {
    // smallest positive subnormal — exercises the e==0 branch
    double sub = std::numeric_limits<double>::denorm_min();
    ASSERT_TRUE(sub > 0.0);
    int    be = 0, se = 0;
    double br = beman::inside::detail::frexp(sub, &be);
    double sr = std::frexp(sub, &se);
    ASSERT_EQ(br, sr);
    ASSERT_EQ(be, se);
}

// ldexp identity cases
TEST(NumbersTest, ldexp_identity_cases) {
    static_assert(beman::inside::detail::ldexp(0.0, 5) == 0.0);
    static_assert(beman::inside::detail::ldexp(1.5, 0) == 1.5);
}

// ldexp inf/NaN passthrough
TEST(NumbersTest, ldexp_inf_nan_passthrough) {
    double inf = std::numeric_limits<double>::infinity();
    ASSERT_TRUE((std::isinf(beman::inside::detail::ldexp(inf, 3))));

    double nan = std::numeric_limits<double>::quiet_NaN();
    ASSERT_TRUE((std::isnan(beman::inside::detail::ldexp(nan, 3))));
}

// ldexp overflow goes to infinity
TEST(NumbersTest, ldexp_overflow_goes_to_infinity) {
    // Use volatile inputs so the compiler can't fold the call at compile time.
    volatile double v_pos = 1.0;
    volatile double v_neg = -1.0;
    volatile int    e     = 2048;

    double r = beman::inside::detail::ldexp(v_pos, e);
    ASSERT_TRUE(std::isinf(r));
    ASSERT_TRUE(r > 0.0);

    double rn = beman::inside::detail::ldexp(v_neg, e);
    ASSERT_TRUE(std::isinf(rn));
    ASSERT_TRUE(rn < 0.0);
}

// ldexp underflow produces subnormal or zero
TEST(NumbersTest, ldexp_underflow_produces_subnormal_or_zero) {
    volatile double v   = 1.0;
    volatile int    big = -1100;
    volatile int    mid = -1050;

    double zero = beman::inside::detail::ldexp(v, big);
    ASSERT_EQ(zero, 0.0);

    double sub = beman::inside::detail::ldexp(v, mid);
    ASSERT_TRUE(sub > 0.0);
    ASSERT_TRUE(sub < std::numeric_limits<double>::min());
}

// ldexp matches std::ldexp on normal exponents
TEST(NumbersTest, ldexp_matches_std_ldexp_on_normal_exponents) {
    for (double v : {1.0, 0.5, 1.5, -3.25, 1024.0}) {
        for (int e : {0, 1, -1, 10, -10, 50, -50}) {
            double a = beman::inside::detail::ldexp(v, e);
            double b = std::ldexp(v, e);
            ASSERT_EQ(a, b);
        }
    }
}

// ldexp subnormal input is normalised first
TEST(NumbersTest, ldexp_subnormal_input_is_normalised_first) {
    // Scaling a subnormal up should match std::ldexp
    double sub = std::numeric_limits<double>::denorm_min();
    double a   = beman::inside::detail::ldexp(sub, 100);
    double b   = std::ldexp(sub, 100);
    ASSERT_EQ(a, b);
}

// abs_fraction handles values >= 2^53
TEST(NumbersTest, abs_fraction_handles_values_ge_2_53) {
    // exponent >= 53 path: num <<= (exponent - bits); den = 1
    // Volatile so the compiler can't fold the constexpr call at compile time.
    volatile double v53 = static_cast<double>(1ull << 53);
    volatile double v60 = static_cast<double>(1ull << 60);

    auto [n1, d1] = abs_fraction(v53);
    ASSERT_EQ(d1, 1u);
    ASSERT_EQ(n1, (1ull << 53));

    auto [n2, d2] = abs_fraction(v60);
    ASSERT_EQ(d2, 1u);
    ASSERT_EQ(n2, (1ull << 60));
}

// abs_fraction throws on non-finite
TEST(NumbersTest, abs_fraction_throws_on_non_finite) {
    ASSERT_THROW((void)(abs_fraction(std::numeric_limits<double>::infinity())), beman::inside::inside_error);
    ASSERT_THROW((void)(abs_fraction(-std::numeric_limits<double>::infinity())), beman::inside::inside_error);
    ASSERT_THROW((void)(abs_fraction(std::numeric_limits<double>::quiet_NaN())), beman::inside::inside_error);
}

// rational from large doubles
TEST(NumbersTest, rational_from_large_doubles) {
    // Through rational ctor — exercises the exponent>=53 branch in abs_fraction
    rational r1{static_cast<double>(1ull << 53)};
    ASSERT_EQ(r1.Num, (1ull << 53));
    ASSERT_EQ(r1.Den, 1);

    rational r2{static_cast<double>(1ull << 50)};
    ASSERT_EQ(r2.Num, (1ull << 50));
    ASSERT_EQ(r2.Den, 1);
}

// rational rejects non-finite double
TEST(NumbersTest, rational_rejects_non_finite_double) {
    ASSERT_THROW((void)(rational{std::numeric_limits<double>::infinity()}), beman::inside::inside_error);
    ASSERT_THROW((void)(rational{std::numeric_limits<double>::quiet_NaN()}), beman::inside::inside_error);
}

// scale_pow2 (the exp kernels' scaling) gives ldexp's bits for every k,
// through normal, subnormal, overflowing and underflowing results.
TEST(NumbersTest, scale_pow2_matches_ldexp) {
    const double values[] = {1.0,
                             -1.0,
                             0.0,
                             -0.0,
                             0.7071067811865476,
                             1.4142135623730951,
                             1.9999999999999998,
                             -1.0000000000000002,
                             std::numeric_limits<double>::denorm_min(),
                             std::numeric_limits<double>::min(),
                             std::numeric_limits<double>::infinity()};
    for (const double v : values)
        for (int k = -1200; k <= 1200; ++k) {
            volatile int kk = k; // the run-time path
            const double a  = scale_pow2(v, kk);
            const double b  = beman::inside::detail::ldexp(v, kk);
            ASSERT_EQ(std::bit_cast<std::uint64_t>(a), std::bit_cast<std::uint64_t>(b)) << v << " * 2^" << k;
        }
}
} // namespace numbers_tests

// Sanity checks for the overflow-detecting wrappers (GCC/Clang builtins):
// boundary values per width, signedness, and use in constant evaluation.
namespace overflow_tests {
using namespace beman::inside;

using namespace beman::inside::detail; // the overflow helpers are internal

TEST(OverflowTest, signed_boundaries) {
    using L = std::numeric_limits<std::int64_t>;
    std::int64_t r{};
    EXPECT_FALSE(add_overflow<std::int64_t>(L::max() - 1, 1, &r));
    EXPECT_EQ(r, L::max());
    EXPECT_TRUE(add_overflow<std::int64_t>(L::max(), 1, &r));
    EXPECT_FALSE(sub_overflow<std::int64_t>(-1, L::max(), &r));
    EXPECT_EQ(r, L::min());
    EXPECT_TRUE(sub_overflow<std::int64_t>(0, L::min(), &r));
    EXPECT_FALSE(sub_overflow<std::int64_t>(-1, L::min(), &r));
    EXPECT_EQ(r, L::max());
    EXPECT_TRUE(mul_overflow<std::int64_t>(L::min(), -1, &r));
    EXPECT_FALSE(mul_overflow<std::int64_t>(L::min() / 2, 2, &r));
    EXPECT_EQ(r, L::min());
}

TEST(OverflowTest, unsigned_and_narrow_boundaries) {
    std::uint64_t u{};
    EXPECT_TRUE(add_overflow<std::uint64_t>(~0ull, 1, &u));
    EXPECT_EQ(u, 0u);
    EXPECT_TRUE(sub_overflow<std::uint64_t>(0, 1, &u));
    EXPECT_FALSE(mul_overflow<std::uint64_t>(1ull << 32, (1ull << 32) - 1, &u));
    EXPECT_TRUE(mul_overflow<std::uint64_t>(1ull << 32, 1ull << 32, &u));
    std::int8_t s{};
    EXPECT_TRUE(mul_overflow<std::int8_t>(100, 100, &s));
    EXPECT_TRUE(sub_overflow<std::int8_t>(5, std::numeric_limits<std::int8_t>::min(), &s));
    EXPECT_FALSE(add_overflow<std::int8_t>(-128, 127, &s));
    EXPECT_EQ(s, -1);
}

TEST(OverflowTest, usable_in_constant_evaluation) {
    static_assert([] {
        std::int32_t r{};
        return !add_overflow<std::int32_t>(2, 3, &r) && r == 5;
    }());
    static_assert([] {
        std::int8_t r{};
        return mul_overflow<std::int8_t>(100, 100, &r);
    }());
    SUCCEED();
}
} // namespace overflow_tests

// Pinned defects: each case failed before its fix.
namespace pinned_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

#define WIDE_LONG_DOUBLE (LDBL_MANT_DIG > DBL_MANT_DIG)

// rational{unsigned, unsigned} with a den past int64: the cast to the signed
// denominator flipped the sign silently.
TEST(InternalsTest, rational_of_two_unsigned_rejects_a_large_denominator) {
    EXPECT_THROW((void)(rational{1u, 0x8000'0000'0000'0001ull}), inside_error);
    static_assert(rational{1u, 0x7FFF'FFFF'FFFF'FFFFull} > rational{0});
}

// rational(num, den) with a signed num took den as an int64 parameter: a
// uint64 den past it converted on the way in, so rational(1, 2^64 − 1)
// was −1.
TEST(InternalsTest, rational_of_any_integers_rejects_a_large_denominator) {
    constexpr std::uint64_t u64_max = std::numeric_limits<std::uint64_t>::max();
    EXPECT_THROW((void)(rational(1, test::at_run_time(u64_max))), inside_error);
    EXPECT_THROW((void)(rational(-1, test::at_run_time(std::uint64_t{1} << 63))), inside_error);
    static_assert(rational(-3, std::uint64_t{6}) == rational{-1, 2});
    static_assert(rational(3u, -6) == rational{-1, 2} && rational(-3, -6) == rational{1, 2});
    constexpr int         i = 3;
    constexpr std::size_t n = 6;
    static_assert(rational{i, n} == rational{1, 2} && rational{n, i} == rational{2});
}

// trunc, floor, ceil and round of a rational narrowed its integer part (up
// to 2^64 − 1) to int64 silently: trunc(rational{2^64 − 1}) was −1. Past
// int64 they raise overflow (fail/rational_trunc_past_int64 while compiling).
TEST(InternalsTest, rational_rounding_to_int64_at_its_limits) {
    constexpr std::int64_t i64_min = std::numeric_limits<std::int64_t>::min();
    EXPECT_THROW((void)trunc(rational{test::at_run_time(std::uint64_t{1} << 63)}), inside_error);
    EXPECT_THROW((void)floor(rational{test::at_run_time((std::uint64_t{1} << 63) + 1), -1}), inside_error);
    static_assert(trunc(rational{std::uint64_t{1} << 63, -1}) == i64_min);
    static_assert(floor(rational{(std::uint64_t{1} << 63) - 1, -1}) == i64_min + 1);
    static_assert(ceil(rational{(std::uint64_t{1} << 63) - 3, 2u}) == (std::int64_t{1} << 62) - 1);
    static_assert(round(rational{(std::uint64_t{1} << 63) - 1}) == std::numeric_limits<std::int64_t>::max());
}

// Checked rational / and + reported overflow for results that fit.
TEST(InternalsTest, rational_ops_reduce_before_overflowing) {
    constexpr std::uint64_t big = (std::uint64_t{1} << 63) + 2;
    static_assert((rational{big} / rational{big}) == rational{1});
    static_assert((rational{big} / rational{big, 3u}) == rational{3});
    static_assert(*detail::try_add(rational{(std::uint64_t{1} << 63) + 1, 2u}, rational{1, 6}) ==
                  rational{3 * (std::uint64_t{1} << 62) + 2, 3u});
    static_assert(*detail::try_add(rational{~std::uint64_t{0}, 2u}, rational{1u, 2u}) ==
                  rational{std::uint64_t{1} << 63});
    static_assert(!detail::try_add(rational{~std::uint64_t{0}}, rational{1}).has_value());
}

// rational compared with a double finer than 2^-62 through rational{double},
// which truncates it: 0 == 1e-30 held.
TEST(InternalsTest, rational_compares_exactly_with_fine_doubles) {
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

// rational ⋈ double went through rational{d}, which raises at |d| ≥ 2^64:
// every rational lies strictly inside ±2^64, so the sign decides.
TEST(InternalsTest, rational_against_a_huge_double) {
    static_assert(rational{3} < 1e20 && rational{-3} > -1e20 && rational{3} != 1e20);
    EXPECT_TRUE(rational{3} < test::at_run_time(1e20));
    EXPECT_TRUE(rational{-3} > test::at_run_time(-0x1p64));
    EXPECT_FALSE(rational{3} == test::at_run_time(1e300));
}

// rational ⋈ long double, rational{long double}: the long double was cast to
// a double first (1e-30L compared equal to 0, 1 + 2^-60 equal to 1).
TEST(InternalsTest, rational_and_long_double) {
    using I64 = std::int64_t;
#if WIDE_LONG_DOUBLE
    static_assert(rational{0} != 1e-30L && rational{0} < 1e-30L);
    static_assert(rational{1} < 1.0L + 0x1p-63L && !(rational{1} == 1.0L + 0x1p-63L));
    static_assert(rational{1.0L + 0x1p-60L} == (rational{1} + rational{1, I64{1} << 60}));
    static_assert(rational{3} < 1e4000L && rational{3} > -1e4000L);
    EXPECT_TRUE(rational{1} < test::at_run_time(1.0L + 0x1p-63L));
    EXPECT_TRUE((rational{1, 3} < test::at_run_time(1.0L / 3))); // 1/3 rounds up in 64 bits
    EXPECT_EQ(rational{test::at_run_time(0.5L + 0x1p-60L)}, (rational{1, 2} + rational{1, I64{1} << 60}));
#endif
}

// rational::to<T> checks that the quotient fits T.
TEST(InternalsTest, rational_to_checks_width) {
    static_assert(rational{300}.to<std::uint8_t>().error() == errc::overflow);
    static_assert(rational{255}.to<std::uint8_t>().value() == 255);
    static_assert(rational{-1}.to<std::uint8_t>().error() == errc::domain_error);
}

//---------------------------------------------------------------------------
// rational's operator<=> (rational.hpp) — the cross-multiply path at runtime
// (the existing ordering tests are static_assert; the runtime path, and its
//  negative-lhs branch, were uncovered.)
//---------------------------------------------------------------------------
// rational spaceship cross-multiply, positive and negative
TEST(InternalsTest, rational_spaceship_cross_multiply_positive_and_negative) {
    using std::strong_ordering;

    // Both denominators != 1, no overflow, positive operands.
    ASSERT_EQ(((rational{1u, 2} <=> rational{1u, 3})), strong_ordering::greater);
    ASSERT_EQ(((rational{1u, 3} <=> rational{1u, 2})), strong_ordering::less);
    ASSERT_EQ(((rational{2u, 6} <=> rational{1u, 3})), strong_ordering::equal);

    // Both negative -> the lhs_neg flip (B <=> A).
    ASSERT_EQ(((rational{1, -2} <=> rational{1, -3})), strong_ordering::less);    // -1/2 < -1/3
    ASSERT_EQ(((rational{1, -3} <=> rational{1, -2})), strong_ordering::greater); // -1/3 > -1/2
}

//---------------------------------------------------------------------------
// gcd(rational, rational) (rational.hpp) — the lcm cofactor multiply overflows
// uint64 -> overflow (distinct from the int64-cap overflow exercised elsewhere:
//  here the product exceeds 2^64, not merely int64_max.)
//---------------------------------------------------------------------------
// rational gcd: lcm that overflows uint64 returns overflow
TEST(InternalsTest, rational_gcd_lcm_that_overflows_uint64_returns_overflow) {
    // Two odd, coprime denominators near 2^40; their product ~2^80 overflows uint64,
    // so the cross-multiplication trap (not the int64 cap) fires.
    std::int64_t a = (std::int64_t{1} << 40) + 1;
    std::int64_t b = (std::int64_t{1} << 40) + 3;
    ASSERT_FALSE((gcd(rational{1u, a}, rational{1u, b}).has_value()));
    ASSERT_EQ((gcd(rational{1u, a}, rational{1u, b}).error()), errc::overflow);
}

//---------------------------------------------------------------------------
// rational's checked add (rational.hpp) — the numerator SUM overflows after
// the (non-overflowing) cross-multiply on UNEQUAL denominators. The equal-
// denominator overflow and the cross-multiply overflow were already
// covered; this is the third, post-cross-multiply overflow.
//---------------------------------------------------------------------------
// rational add: numerator sum overflow on unequal denominators
TEST(InternalsTest, rational_add_numerator_sum_overflow_on_unequal_denominators) {
    // Denominators 2 and 6 (odd numerators so neither reduces). After cross-
    // multiply A = num_a*3, B = num_b*1 each fit in uint64, but A + B exceeds
    // 2^64 — the sum reduces back by 2: 18999999999999999994/6.
    rational a{5999999999999999999u, 2}; // odd numerator -> stays /2
    rational b{999999999999999997u, 6};  // coprime to 6   -> stays /6
    ASSERT_EQ(*try_add(a, b), (rational{9499999999999999997u, 3}));
    // One that stays past 2^64: (2^64 − 1)/2 + 2^63/3 = (5·2^63 − 3)/6.
    ASSERT_FALSE(try_add(rational{18446744073709551615u, 2}, rational{9223372036854775808u, 3}).has_value());
}

//---------------------------------------------------------------------------
// float_bits.hpp — constexpr ldexp into the subnormal range, including the
// round-to-nearest-even ++mantissa at :165-166. Validated against std::ldexp.
//---------------------------------------------------------------------------
// ldexp into subnormal range matches std::ldexp (with rounding)
TEST(InternalsTest, ldexp_into_subnormal_range_matches_std_ldexp_with_rounding) {
    const double mants[] = {1.0,
                            1.5,
                            1.9999999999,
                            0x1.fffffffffffffp0, // all-ones mantissa: forces round-up
                            0x1.5555555555555p0,
                            0x1.0000000000001p0};
    for (int e = -1080; e <= -1020; ++e)
        for (double m : mants)
            ASSERT_EQ((beman::inside::detail::ldexp(m, e)), (std::ldexp(m, e)));
}

//---------------------------------------------------------------------------
// float_bits.hpp — abs_fraction on a subnormal (no implicit leading 1)
// float_bits.hpp — magnitudes below ~2^-62: significand-drop / flush-to-zero
//---------------------------------------------------------------------------
// rational from subnormal and very small doubles
TEST(InternalsTest, rational_from_subnormal_and_very_small_doubles) {
    // Subnormal input: exercises the e==0 branch, then collapses to 0 (drop>=64).
    ASSERT_EQ(rational{std::numeric_limits<double>::denorm_min()}, 0_r);

    // Normal but below 2^-62: the significand is shifted down and recovered by
    // the trailing reduction, yielding the exact dyadic fraction.
    ASSERT_EQ(rational{0x1p-40}, (rational{1u, std::int64_t{1} << 40}));

    // Far below the cap: drops to a hard zero.
    ASSERT_EQ(rational{0x1p-70}, 0_r);
}

} // namespace pinned_tests
