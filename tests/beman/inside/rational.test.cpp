// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <limits>

using namespace beman::inside;
using namespace beman::inside::detail;

namespace
{
  constexpr umax M = std::numeric_limits<umax>::max();
}

// rational construction normalises
TEST(RationalTest, rational_construction_normalises)
{
  {
    SCOPED_TRACE("zero numerator collapses denominator");
    ASSERT_EQ((rational{0u, 7}), (rational{0u, 1}));
    ASSERT_EQ((rational{0,  -5}), (rational{0u, 1}));
    ASSERT_EQ((0_r).Denominator, 1);
  }

  {
    SCOPED_TRACE("negative denominator carries sign onto rational");
    rational a{1, -2};
    ASSERT_EQ(a.Numerator, 1u);
    ASSERT_EQ(a.Denominator, -2);
    ASSERT_TRUE(a < 0);
    ASSERT_EQ((rational{-1, 2}), (rational{1, -2}));
    ASSERT_EQ((rational{-1, -2}), (rational{1,  2}));
  }

  {
    SCOPED_TRACE("GCD reduction at construction");
    ASSERT_EQ((rational{6u, 8}), (rational{3u, 4}));
    ASSERT_EQ((rational{6,  -8}), (rational{3, -4}));
    ASSERT_EQ((rational{100u, 25}), (rational{4u, 1}));
  }

  {
    SCOPED_TRACE("denominator zero throws");
    ASSERT_THROW((void)((rational{1u, 0})), beman::inside::inside_error);
    ASSERT_THROW((void)((rational{1,  0})), beman::inside::inside_error);
  }

  {
    SCOPED_TRACE("denominator imax_min throws (cannot be negated without UB)");
    constexpr auto imin = std::numeric_limits<imax>::min();
    ASSERT_THROW((void)((rational{1u, imin})), beman::inside::inside_error);
    ASSERT_THROW((void)((rational{1,  imin})), beman::inside::inside_error);
    // Negative-num path: the validation must run BEFORE the mem-init
    // negation, otherwise -imax_min would be signed overflow (UB).
    ASSERT_THROW((void)((rational{-1, imin})), beman::inside::inside_error);
  }

  {
    SCOPED_TRACE("from int min");
    constexpr rational a{std::numeric_limits<int>::min()};
    ASSERT_TRUE(a.Denominator < 0);
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
    ASSERT_EQ(*(3_r/2), (rational{3u, 2}));
    ASSERT_EQ(*(-6_r/16), (rational{-3, 8}));
    ASSERT_EQ(2.5_r, (rational{5u, 2}));
  }
}

// rational comparison
TEST(RationalTest, rational_comparison)
{
  {
    SCOPED_TRACE("ordering");
    static_assert(-1_r < 0_r);
    static_assert(0_r  < 1_r);
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
    SCOPED_TRACE("comparison cross-trims so big denominators don't always overflow");
    constexpr imax half = static_cast<imax>(M / 2);
    rational a{static_cast<umax>(half), half};   // -> 1
    rational b{1u};
    ASSERT_EQ(a, b);
  }

  {
    SCOPED_TRACE("cross-multiplication uses 128-bit, never overflows");
    // M/2 vs (M-1)/3: numerators are consecutive (coprime) and denominators
    // {2,3} are coprime, so cross-trim cannot reduce either pair and the 64-bit
    // products overflow umax — but the 128-bit cross-multiply compares them
    // exactly. a - b = (M+2)/6 > 0, so a > b.
    rational a{M, 2};
    rational b{M - 1, 3};
    ASSERT_TRUE(a > b);
    ASSERT_TRUE(b < a);
    ASSERT_FALSE(a < b);
  }
}

// rational arithmetic
TEST(RationalTest, rational_arithmetic)
{
  {
    SCOPED_TRACE("add / sub / mul / div basic");
    static_assert(rational{3,2} + rational{1,5} == rational{17u,10});
    static_assert(rational{3,2} - rational{1,5} == rational{13u,10});
    static_assert(rational{3,2} / rational{1,2} == 3_r);
    static_assert(*(rational{3,2} * rational{1,2}) == rational{3u, 4});
  }

  {
    SCOPED_TRACE("zero arms");
    ASSERT_EQ((*(rational{3u, 2} + 0_r)), (rational{3u, 2}));
    ASSERT_EQ((*(0_r     + rational{3u, 2})), (rational{3u, 2}));
    ASSERT_EQ((*(rational{3u, 2} * 0_r)), 0_r);
    ASSERT_EQ((*(0_r     / rational{3u, 2})), 0_r);
  }

  {
    SCOPED_TRACE("negation and unary plus");
    ASSERT_EQ((-rational{3u, 4}), (rational{3, -4}));
    ASSERT_EQ(-0_r, 0_r);             // -0 == 0
    ASSERT_EQ((+rational{3u, 4}), (rational{3u, 4}));
  }

  {
    SCOPED_TRACE("self-cancel returns zero");
    rational a{3u, 7};
    ASSERT_EQ(*(a + (-a)), 0_r);
    ASSERT_EQ(*(a - a), 0_r);
  }

  {
    SCOPED_TRACE("unchecked variants");
    static_assert(rational::add_unchecked(2_r, 3_r) == 5_r);
    static_assert(rational::mul_unchecked(2_r, 3_r) == 6_r);
    static_assert(rational::div_unchecked(6_r, 3_r) == 2_r);
    static_assert(rational::inv_unchecked(rational{2u, 3}) == rational{3u, 2});
  }

  // The checked operators return std::expected<rational, errc>; an implicit
  // converting constructor unwraps that into a plain rational so coefficient
  // expressions read as ordinary arithmetic (no .value()). Overflow is a
  // compile error in constant evaluation.
  {
    SCOPED_TRACE("expected<rational> unwraps implicitly into rational");
    constexpr rational two_x   = 2 * rational{3};       // int ⊗ rational
    constexpr rational half    = rational{3} / 2;       // rational ⊗ int
    constexpr rational chained = rational{3,2} - rational{1,5};
    static_assert(two_x   == 6_r);
    static_assert(half    == rational{3u, 2});
    static_assert(chained == rational{13u, 10});

    // Bit-identical to the prior mul_unchecked form it replaces.
    static_assert(half == rational::mul_unchecked(rational{3}, rational{1, 2}));

    // Assignment (not just copy-init) also unwraps.
    rational acc{0u, 1};
    acc = 2 * rational{3};
    ASSERT_EQ(acc, 6_r);
  }

  {
    SCOPED_TRACE("inv basic");
    static_assert(*rational::inv(rational{3u, 2}) == rational{2u, 3});
    static_assert(*rational::inv(rational{1u, 5}) == 5_r);
    static_assert(*rational::inv(5_r)    == rational{1u, 5});

    // Sign preservation (denominator carries the sign)
    static_assert(*rational::inv(rational{-3, 2}) == rational{-2, 3});
    static_assert(*rational::inv(rational{3, -2}) == rational{-2, 3});

    // Involutive
    static_assert(*rational::inv(*rational::inv(rational{7u, 11})) == rational{7u, 11});
  }

  {
    SCOPED_TRACE("inv of zero -> division_by_zero");
    ASSERT_FALSE(rational::inv(0_r).has_value());
    ASSERT_EQ(rational::inv(0_r).error(), errc::division_by_zero);
    ASSERT_EQ((1_r / 0_r).error(), errc::division_by_zero);
  }

  {
    SCOPED_TRACE("compound-assign: rational RHS");
    rational a{3, 2};
    a += rational{1, 5};
    ASSERT_EQ(a, (rational{17u, 10}));
    a -= rational{1, 5};
    ASSERT_EQ(a, (rational{3u, 2}));
    a *= rational{1, 2};
    ASSERT_EQ(a, (rational{3u, 4}));
    a /= rational{1, 2};
    ASSERT_EQ(a, (rational{3u, 2}));
  }

  {
    SCOPED_TRACE("compound-assign: arithmetic RHS");
    rational a{3, 2};
    a += 1;
    ASSERT_EQ(a, (rational{5u, 2}));
    a *= 2;
    ASSERT_EQ(a, 5_r);
    a /= 5;
    ASSERT_EQ(a, 1_r);
  }

  {
    SCOPED_TRACE("compound-assign: expected<rational> RHS unwraps the checked op");
    rational a{1, 2};
    // rational * rational returns expected<rational>; += on that unwraps.
    a += rational{1, 3} * rational{6, 1};
    ASSERT_EQ(a, (rational{5u, 2}));
  }

  {
    SCOPED_TRACE("compound-assign throws bad_expected_access on overflow");
    constexpr auto M = std::numeric_limits<imax>::max();
    // 1/M + 1/(M-1) — denominator product M*(M-1) overflows imax.
    rational a{1u, M};
    rational b{1u, M - 1};
    ASSERT_THROW((void)(a += b), std::bad_expected_access<errc>);
  }
}

// rational overflow detection
TEST(RationalTest, rational_overflow_detection)
{
  {
    SCOPED_TRACE("checked operators return errc::overflow at runtime");
    ASSERT_FALSE((rational{M} + 1_r).has_value());
    ASSERT_EQ((rational{M} + 1_r).error(), errc::overflow);
    ASSERT_FALSE((rational{M} * 2_r).has_value());
    ASSERT_FALSE(((rational{M} / rational{1u, 2}).has_value()));
  }

  {
    SCOPED_TRACE("subtraction goes through add(-rhs)");
    // M - (-1) = M+1 -> overflow
    ASSERT_FALSE(((rational{M} - rational{1, -1}).has_value()));
  }

  {
    SCOPED_TRACE("cross-trim avoids spurious overflow on common factors");
    // (M/5)/2 + (M/5)/2 — common denominator avoids cross-multiplication.
    auto small = rational{M / 5, 2};
    ASSERT_TRUE((small + small).has_value());
  }

  {
    SCOPED_TRACE("add cross-trim on unequal denominators avoids spurious overflow");
    // gcd(4, 6) = 2; lcm = 12. With cross-trim a_ad'=2, b_ad'=3, so
    // (M/5) * b_ad' = (M/5)*3 fits, whereas (M/5)*6 (no cross-trim) would
    // overflow.
    auto a = rational{M / 5, 4};
    auto b = rational{1u, 6};
    ASSERT_TRUE((a + b).has_value());
  }

  {
    SCOPED_TRACE("add unequal-denominator value correctness");
    // Catches regressions in the lcm-based common-denominator computation.
    // 1/4 + 1/6 = 3/12 + 2/12 = 5/12         (gcd=2, lcm=12)
    static_assert(*(rational{1u, 4} + rational{1u, 6}) == rational{5u, 12});
    // 1/2 + 1/3 = 3/6 + 2/6 = 5/6            (gcd=1, lcm=6)
    static_assert(*(rational{1u, 2} + rational{1u, 3}) == rational{5u, 6});
    // 3/8 + 5/12 = 9/24 + 10/24 = 19/24      (gcd=4, lcm=24)
    static_assert(*(rational{3u, 8} + rational{5u, 12}) == rational{19u, 24});
    // mixed signs: 5/6 - 1/4 = 10/12 - 3/12 = 7/12
    static_assert(*(rational{5u, 6} + rational{1, -4}) == rational{7u, 12});
  }

  {
    SCOPED_TRACE("sub overflow returning errc::overflow");
    // -M - 1 would overflow on the negative side
    ASSERT_FALSE(((rational{M, -1} - 1_r).has_value()));
  }

  {
    SCOPED_TRACE("checked arithmetic rejects denominators exceeding imax_max");
    // 2^62 * 3 = 1.5 * 2^63 — fits in umax, exceeds imax_max.
    // (Coprime denominators so the cross-trim cannot reduce the lcm.)
    imax p62 = imax{1} << 62;

    // mul: a_ad * b_ad after cross-trim still > imax_max.
    ASSERT_FALSE(((rational{1u, p62} * rational{1u, 3}).has_value()));

    // add: lcm > imax_max.
    ASSERT_FALSE(((rational{1u, p62} + rational{1u, 3}).has_value()));

    // inv of M (M > imax_max) would land M in the result's Denominator slot.
    ASSERT_FALSE((rational::inv(rational{M, 1}).has_value()));

    // div via the checked path inherits the inv range check.
    ASSERT_FALSE(((rational{1u, 1} / rational{M, 1}).has_value()));
  }

  {
    SCOPED_TRACE("add to_string sees overflow boundary fall-through");
    ASSERT_EQ((beman::inside::to_string(rational{M, 2})), "9223372036854775807 1/2");
    ASSERT_EQ((beman::inside::to_string(rational{M / 5, 2})), "1844674407370955161.5");
  }
}

// rational trunc toward zero
TEST(RationalTest, rational_trunc_toward_zero)
{
  // converting to integer truncates toward zero
  ASSERT_EQ((trunc(rational{7u, 2})), 3);
  ASSERT_EQ((trunc(rational{7,  -2})), -3);
}

// rational helpers
TEST(RationalTest, rational_helpers)
{
  {
    SCOPED_TRACE("abs");
    ASSERT_EQ((abs(rational{3, -4})), (rational{3u, 4}));
    ASSERT_EQ((abs(rational{3u, 4})), (rational{3u, 4}));
    ASSERT_EQ(abs(0_r), 0_r);
  }

  {
    SCOPED_TRACE("gcd");
    ASSERT_EQ((gcd(rational{6u, 1}, rational{8u, 1})), (rational{2u, 1}));
    ASSERT_EQ((gcd(rational{1u, 2}, rational{1u, 3})), (rational{1u, 6}));

    // Denominator-lcm overflow: lcm(2^62, 3) = 3 * 2^62 > imax_max.
    ASSERT_FALSE((gcd(rational{1u, imax{1} << 62}, rational{1u, 3}).has_value()));
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
    // umax. Representability of the index count is a separate concern
    // (grid::max_index_checked), not part of the lattice-alignment predicate.
    ASSERT_TRUE((divides_evenly(rational{M}, rational{1, 2})));
    // genuinely unaligned stays false (no overflow involved)
    ASSERT_FALSE((divides_evenly(rational{1u, 2}, rational{1u, 3})));
  }
}

// rational conversion to integer/float
TEST(RationalTest, rational_conversion_to_integer_float)
{
  {
    SCOPED_TRACE("to_unsigned floors, rejects negatives");
    ASSERT_TRUE((static_cast<unsigned>(rational{7u, 2}) == 3u));
    ASSERT_THROW((void)(static_cast<unsigned>(rational{7, -2})), beman::inside::inside_error);
  }

  {
    SCOPED_TRACE("to_signed rounds toward zero (operator T)");
    ASSERT_TRUE((static_cast<int>(rational{7u, 2})  ==  3));
    ASSERT_TRUE((static_cast<int>(rational{7,  -2}) == -3));
  }

  {
    SCOPED_TRACE("to_double");
    ASSERT_TRUE((static_cast<double>(rational{1u, 2})  == 0.5));
    ASSERT_TRUE((static_cast<double>(rational{1u, -2}) == -0.5));
    ASSERT_TRUE(static_cast<double>(0_r)     == 0.0);
  }

  {
    SCOPED_TRACE("to<T> reports domain_error for negative rational");
    rational pos{5u, 2};
    rational neg{5,  -2};
    ASSERT_TRUE(pos.to<unsigned>().value() == 2u);
    auto r = neg.to<unsigned>();
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error(), errc::domain_error);
  }
}

// rational trunc / floor / round
TEST(RationalTest, rational_trunc_floor_round)
{
  {
    SCOPED_TRACE("trunc — toward zero");
    ASSERT_EQ((trunc(rational{7u, 2})), 3);   //  3.5 -> 3
    ASSERT_EQ((trunc(rational{7,  -2})), -3);   // -3.5 -> -3
    ASSERT_EQ((trunc(rational{1u, 2})), 0);
    ASSERT_EQ((trunc(rational{1,  -2})), 0);
    ASSERT_EQ((trunc(rational{4u, 1})), 4);
    ASSERT_EQ(trunc((0_r)), 0);
  }

  {
    SCOPED_TRACE("floor — toward -inf");
    ASSERT_EQ((floor(rational{7u, 2})), 3);   //  3.5 -> 3
    ASSERT_EQ((floor(rational{7,  -2})), -4);   // -3.5 -> -4
    ASSERT_EQ((floor(rational{1u, 2})), 0);
    ASSERT_EQ((floor(rational{1,  -2})), -1);   // -0.5 -> -1
    ASSERT_EQ((floor(rational{4u, 1})), 4);
    ASSERT_EQ((floor(rational{4,  -1})), -4);   // exact integer: no step
    ASSERT_EQ(floor((0_r)), 0);
  }

  {
    SCOPED_TRACE("ceil — toward +inf");
    ASSERT_EQ((ceil(rational{7u, 2})), 4);   //  3.5 ->  4
    ASSERT_EQ((ceil(rational{7,  -2})), -3);   // -3.5 -> -3
    ASSERT_EQ((ceil(rational{1u, 2})), 1);   //  0.5 ->  1
    ASSERT_EQ((ceil(rational{1,  -2})), 0);   // -0.5 ->  0
    ASSERT_EQ((ceil(rational{4u, 1})), 4);   // exact integer: no step
    ASSERT_EQ((ceil(rational{4,  -1})), -4);
    ASSERT_EQ(ceil((0_r)), 0);
  }

  {
    SCOPED_TRACE("round — half away from zero");
    ASSERT_EQ((round(rational{1u, 2})), 1);    //  0.5 ->  1
    ASSERT_EQ((round(rational{1,  -2})), -1);   // -0.5 -> -1
    ASSERT_EQ((round(rational{3u, 2})), 2);    //  1.5 ->  2
    ASSERT_EQ((round(rational{3,  -2})), -2);   // -1.5 -> -2
    ASSERT_EQ((round(rational{1u, 4})), 0);    //  0.25 -> 0
    ASSERT_EQ((round(rational{1u, 3})), 0);    //  ~0.33 -> 0
    ASSERT_EQ((round(rational{2u, 3})), 1);    //  ~0.67 -> 1
    ASSERT_EQ((round(rational{2,  -3})), -1);
    ASSERT_EQ(round((0_r)), 0);
  }
}

//---------------------------------------------------------------------------
// 2026-07: mixed-sign addition rescued in 128-bit. `1024 − m/2^54` forms the
// cross-product 1024·2^54 == 2^64 (one past umax) before the subtraction
// brings the numerator back into range — the dbl-engine store path hit
// exactly this via `rhs - Lower` and terminated through noexcept.
//---------------------------------------------------------------------------
// mixed-sign add rescues a cross-product overflow when the difference fits
TEST(RationalTest, mixed_sign_add_rescues_a_cross_product_overflow_when_the_difference_fits)
{
  // -0.49996929771979287 as an exact double fraction: den 2^54.
  const rational fp_value{umax{9006646171630191}, imax{-18014398509481984}};

  const auto offset = fp_value + rational{1024};
  ASSERT_TRUE(offset.has_value());
  ASSERT_TRUE(offset->Numerator   == umax{18437737427537921425u});
  ASSERT_TRUE(offset->Denominator == imax{18014398509481984});

  // Same-sign sums past umax stay an error (the result truly needs > 64 bits).
  const rational positive{umax{9006646171630191}, imax{18014398509481984}};
  ASSERT_FALSE((positive + rational{1024}).has_value());
}
