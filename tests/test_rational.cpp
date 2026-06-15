#include "bound/bound.hpp"
#include "bound/format.hpp"

#include <catch2/catch_test_macros.hpp>

#include <limits>

using namespace bnd;
using namespace bnd::detail;

namespace
{
  constexpr umax M = std::numeric_limits<umax>::max();
}

TEST_CASE("rational construction normalises", "[rational][construction]")
{
  SECTION("zero numerator collapses denominator")
  {
    REQUIRE(rational{0u, 7}      == rational{0u, 1});
    REQUIRE(rational{0,  -5}     == rational{0u, 1});
    REQUIRE((0_r).Denominator == 1);
  }

  SECTION("negative denominator carries sign onto rational")
  {
    rational a{1, -2};
    REQUIRE(a.Numerator == 1u);
    REQUIRE(a.Denominator == -2);
    REQUIRE(a < 0);
    REQUIRE(rational{-1, 2}  == rational{1, -2});
    REQUIRE(rational{-1, -2} == rational{1,  2});
  }

  SECTION("GCD reduction at construction")
  {
    REQUIRE(rational{6u, 8}  == rational{3u, 4});
    REQUIRE(rational{6,  -8} == rational{3, -4});
    REQUIRE(rational{100u, 25} == rational{4u, 1});
  }

  SECTION("denominator zero throws")
  {
    REQUIRE_THROWS_AS((rational{1u, 0}), std::system_error);
    REQUIRE_THROWS_AS((rational{1,  0}), std::system_error);
  }

  SECTION("denominator imax_min throws (cannot be negated without UB)")
  {
    constexpr auto imin = std::numeric_limits<imax>::min();
    REQUIRE_THROWS_AS((rational{1u, imin}), std::system_error);
    REQUIRE_THROWS_AS((rational{1,  imin}), std::system_error);
    // Negative-num path: the validation must run BEFORE the mem-init
    // negation, otherwise -imax_min would be signed overflow (UB).
    REQUIRE_THROWS_AS((rational{-1, imin}), std::system_error);
  }

  SECTION("from int min")
  {
    constexpr rational a{std::numeric_limits<int>::min()};
    REQUIRE(a.Denominator < 0);
    REQUIRE(a < 0);
  }

  SECTION("from double")
  {
    REQUIRE(rational{0.5}   == rational{1u, 2});
    REQUIRE(rational{-0.25} == rational{1, -4});
    REQUIRE(rational{0.0}   == rational{0u, 1});
  }

  SECTION("user-defined literals")
  {
    REQUIRE(1_r          == rational{1u, 1});
    REQUIRE(*(3_r/2)     == rational{3u, 2});
    REQUIRE(*(-6_r/16)   == rational{-3, 8});
    REQUIRE(2.5_r        == rational{5u, 2});
  }
}

TEST_CASE("rational comparison", "[rational][comparison]")
{
  SECTION("ordering")
  {
    STATIC_REQUIRE(-1_r < 0_r);
    STATIC_REQUIRE(0_r  < 1_r);
    STATIC_REQUIRE(rational{-3, 2} < rational{-2, 3});
    STATIC_REQUIRE(rational{1u, 2} < rational{2u, 3});
  }

  SECTION("equality across normalisation")
  {
    REQUIRE(rational{2u, 4}  == rational{1u, 2});
    REQUIRE(rational{-2, 4}  == rational{1, -2});
    REQUIRE(rational{0u, 5}  == rational{0u, 9});
  }

  SECTION("comparison with arithmetic")
  {
    REQUIRE(rational{3u, 2} > 1);
    REQUIRE(rational{1u, 2} < 1.0);
    REQUIRE(rational{5u, 1} == 5);
  }

  SECTION("comparison cross-trims so big denominators don't always overflow")
  {
    constexpr imax half = static_cast<imax>(M / 2);
    rational a{static_cast<umax>(half), half};   // -> 1
    rational b{1u};
    REQUIRE(a == b);
  }

  SECTION("cross-multiplication overflow traps at runtime")
  {
    // M/2 vs (M-1)/3: numerators are consecutive (coprime) and denominators
    // {2,3} are coprime, so cross-trim cannot reduce either pair. M*3 then
    // overflows umax, so the comparison must trap rather than return garbage.
    rational a{M, 2};
    rational b{M - 1, 3};
    REQUIRE_THROWS_AS(a < b, std::system_error);
    REQUIRE_THROWS_AS(b < a, std::system_error);
  }
}

TEST_CASE("rational arithmetic", "[rational][arithmetic]")
{
  SECTION("add / sub / mul / div basic")
  {
    STATIC_REQUIRE(rational{3,2} + rational{1,5} == rational{17u,10});
    STATIC_REQUIRE(rational{3,2} - rational{1,5} == rational{13u,10});
    STATIC_REQUIRE(rational{3,2} / rational{1,2} == 3_r);
    STATIC_REQUIRE(*(rational{3,2} * rational{1,2}) == rational{3u, 4});
  }

  SECTION("zero arms")
  {
    REQUIRE(*(rational{3u, 2} + 0_r) == rational{3u, 2});
    REQUIRE(*(0_r     + rational{3u, 2}) == rational{3u, 2});
    REQUIRE(*(rational{3u, 2} * 0_r) == 0_r);
    REQUIRE(*(0_r     / rational{3u, 2}) == 0_r);
  }

  SECTION("negation and unary plus")
  {
    REQUIRE(-rational{3u, 4} == rational{3, -4});
    REQUIRE(-0_r    == 0_r);             // -0 == 0
    REQUIRE(+rational{3u, 4} == rational{3u, 4});
  }

  SECTION("self-cancel returns zero")
  {
    rational a{3u, 7};
    REQUIRE(*(a + (-a)) == 0_r);
    REQUIRE(*(a - a)    == 0_r);
  }

  SECTION("unchecked variants")
  {
    STATIC_REQUIRE(rational::add_unchecked(2_r, 3_r) == 5_r);
    STATIC_REQUIRE(rational::mul_unchecked(2_r, 3_r) == 6_r);
    STATIC_REQUIRE(rational::div_unchecked(6_r, 3_r) == 2_r);
    STATIC_REQUIRE(rational::inv_unchecked(rational{2u, 3}) == rational{3u, 2});
  }

  // The checked operators return slim::optional<rational>; an implicit
  // converting constructor unwraps that into a plain rational so coefficient
  // expressions read as ordinary arithmetic (no .value()). Overflow on an empty
  // optional is a compile error in constant evaluation.
  SECTION("optional<rational> unwraps implicitly into rational")
  {
    constexpr rational two_x   = 2 * rational{3};       // int ⊗ rational
    constexpr rational half    = rational{3} / 2;       // rational ⊗ int
    constexpr rational chained = rational{3,2} - rational{1,5};
    STATIC_REQUIRE(two_x   == 6_r);
    STATIC_REQUIRE(half    == rational{3u, 2});
    STATIC_REQUIRE(chained == rational{13u, 10});

    // Bit-identical to the prior mul_unchecked form it replaces.
    STATIC_REQUIRE(half == rational::mul_unchecked(rational{3}, rational{1, 2}));

    // Assignment (not just copy-init) also unwraps.
    rational acc{0u, 1};
    acc = 2 * rational{3};
    REQUIRE(acc == 6_r);
  }

  SECTION("inv basic")
  {
    STATIC_REQUIRE(*rational::inv(rational{3u, 2}) == rational{2u, 3});
    STATIC_REQUIRE(*rational::inv(rational{1u, 5}) == 5_r);
    STATIC_REQUIRE(*rational::inv(5_r)    == rational{1u, 5});

    // Sign preservation (denominator carries the sign)
    STATIC_REQUIRE(*rational::inv(rational{-3, 2}) == rational{-2, 3});
    STATIC_REQUIRE(*rational::inv(rational{3, -2}) == rational{-2, 3});

    // Involutive
    STATIC_REQUIRE(*rational::inv(*rational::inv(rational{7u, 11})) == rational{7u, 11});
  }

  SECTION("inv of zero -> nullopt")
  {
    REQUIRE_FALSE(rational::inv(0_r).has_value());
  }

  SECTION("compound-assign: rational RHS")
  {
    rational a{3, 2};
    a += rational{1, 5};
    REQUIRE(a == rational{17u, 10});
    a -= rational{1, 5};
    REQUIRE(a == rational{3u, 2});
    a *= rational{1, 2};
    REQUIRE(a == rational{3u, 4});
    a /= rational{1, 2};
    REQUIRE(a == rational{3u, 2});
  }

  SECTION("compound-assign: arithmetic RHS")
  {
    rational a{3, 2};
    a += 1;
    REQUIRE(a == rational{5u, 2});
    a *= 2;
    REQUIRE(a == 5_r);
    a /= 5;
    REQUIRE(a == 1_r);
  }

  SECTION("compound-assign: optional<rational> RHS unwraps the checked op")
  {
    rational a{1, 2};
    // rational * rational returns optional<rational>; += on that unwraps.
    a += rational{1, 3} * rational{6, 1};
    REQUIRE(a == rational{5u, 2});
  }

  SECTION("compound-assign throws bad_optional_access on overflow")
  {
    constexpr auto M = std::numeric_limits<imax>::max();
    // 1/M + 1/(M-1) — denominator product M*(M-1) overflows imax.
    rational a{1u, M};
    rational b{1u, M - 1};
    REQUIRE_THROWS_AS(a += b, slim::bad_optional_access);
  }
}

TEST_CASE("rational overflow detection", "[rational][overflow]")
{
  SECTION("checked operators return nullopt at runtime")
  {
    REQUIRE_FALSE((rational{M} + 1_r).has_value());
    REQUIRE_FALSE((rational{M} * 2_r).has_value());
    REQUIRE_FALSE((rational{M} / rational{1u, 2}).has_value());
  }

  SECTION("subtraction goes through add(-rhs)")
  {
    // M - (-1) = M+1 -> overflow
    REQUIRE_FALSE((rational{M} - rational{1, -1}).has_value());
  }

  SECTION("cross-trim avoids spurious overflow on common factors")
  {
    // (M/5)/2 + (M/5)/2 — common denominator avoids cross-multiplication.
    auto small = rational{M / 5, 2};
    REQUIRE((small + small).has_value());
  }

  SECTION("add cross-trim on unequal denominators avoids spurious overflow")
  {
    // gcd(4, 6) = 2; lcm = 12. With cross-trim a_ad'=2, b_ad'=3, so
    // (M/5) * b_ad' = (M/5)*3 fits, whereas (M/5)*6 (no cross-trim) would
    // overflow.
    auto a = rational{M / 5, 4};
    auto b = rational{1u, 6};
    REQUIRE((a + b).has_value());
  }

  SECTION("add unequal-denominator value correctness")
  {
    // Catches regressions in the lcm-based common-denominator computation.
    // 1/4 + 1/6 = 3/12 + 2/12 = 5/12         (gcd=2, lcm=12)
    STATIC_REQUIRE(*(rational{1u, 4} + rational{1u, 6}) == rational{5u, 12});
    // 1/2 + 1/3 = 3/6 + 2/6 = 5/6            (gcd=1, lcm=6)
    STATIC_REQUIRE(*(rational{1u, 2} + rational{1u, 3}) == rational{5u, 6});
    // 3/8 + 5/12 = 9/24 + 10/24 = 19/24      (gcd=4, lcm=24)
    STATIC_REQUIRE(*(rational{3u, 8} + rational{5u, 12}) == rational{19u, 24});
    // mixed signs: 5/6 - 1/4 = 10/12 - 3/12 = 7/12
    STATIC_REQUIRE(*(rational{5u, 6} + rational{1, -4}) == rational{7u, 12});
  }

  SECTION("sub overflow returning nullopt")
  {
    // -M - 1 would overflow on the negative side
    REQUIRE_FALSE((rational{M, -1} - 1_r).has_value());
  }

  SECTION("checked arithmetic rejects denominators exceeding imax_max")
  {
    // 2^62 * 3 = 1.5 * 2^63 — fits in umax, exceeds imax_max.
    // (Coprime denominators so the cross-trim cannot reduce the lcm.)
    imax p62 = imax{1} << 62;

    // mul: a_ad * b_ad after cross-trim still > imax_max.
    REQUIRE_FALSE((rational{1u, p62} * rational{1u, 3}).has_value());

    // add: lcm > imax_max.
    REQUIRE_FALSE((rational{1u, p62} + rational{1u, 3}).has_value());

    // inv of M (M > imax_max) would land M in the result's Denominator slot.
    REQUIRE_FALSE(rational::inv(rational{M, 1}).has_value());

    // div via the checked path inherits the inv range check.
    REQUIRE_FALSE((rational{1u, 1} / rational{M, 1}).has_value());
  }

  SECTION("add to_string sees overflow boundary fall-through")
  {
    REQUIRE(bnd::to_string(rational{M, 2})     == "9223372036854775807 1/2");
    REQUIRE(bnd::to_string(rational{M / 5, 2}) == "1844674407370955161.5");
  }
}

TEST_CASE("rational sentinel", "[rational][sentinel]")
{
  rational s = rational::make_sentinel();
  REQUIRE(s.Denominator == 0);

  // sentinel_traits must agree
  REQUIRE_FALSE(slim::optional<rational>{}.has_value());

  // converting to integer of a non-sentinel rational truncates toward zero
  REQUIRE(rational{7u, 2}.trunc() ==  3);
  REQUIRE(rational{7,  -2}.trunc() == -3);
}

TEST_CASE("rational helpers", "[rational][helpers]")
{
  SECTION("abs")
  {
    REQUIRE(abs(rational{3, -4}) == rational{3u, 4});
    REQUIRE(abs(rational{3u, 4}) == rational{3u, 4});
    REQUIRE(abs(0_r)    == 0_r);
  }

  SECTION("gcd")
  {
    REQUIRE(gcd(rational{6u, 1}, rational{8u, 1}) == rational{2u, 1});
    REQUIRE(gcd(rational{1u, 2}, rational{1u, 3}) == rational{1u, 6});

    // Denominator-lcm overflow: lcm(2^62, 3) = 3 * 2^62 > imax_max.
    REQUIRE_FALSE(gcd(rational{1u, imax{1} << 62}, rational{1u, 3}).has_value());
  }

  SECTION("divides_evenly")
  {
    REQUIRE(divides_evenly(rational{6u, 1}, rational{2u, 1}));
    REQUIRE_FALSE(divides_evenly(rational{6u, 1}, rational{4u, 1}));
    REQUIRE(divides_evenly(rational{1u, 2}, rational{1u, 4}));
    // by convention divisor==0 returns true
    REQUIRE(divides_evenly(rational{6u, 1}, 0_r));
  }

  SECTION("divides_evenly does not throw on overflow")
  {
    // M / (1/2) = M*2 overflows the numerator -> false, not exception.
    REQUIRE_FALSE(divides_evenly(rational{M}, rational{1, 2}));
  }
}

TEST_CASE("rational conversion to integer/float", "[rational][conversion]")
{
  SECTION("to_unsigned floors, rejects negatives")
  {
    REQUIRE(static_cast<unsigned>(rational{7u, 2}) == 3u);
    REQUIRE_THROWS_AS(static_cast<unsigned>(rational{7, -2}), std::system_error);
  }

  SECTION("to_signed rounds toward zero (operator T)")
  {
    REQUIRE(static_cast<int>(rational{7u, 2})  ==  3);
    REQUIRE(static_cast<int>(rational{7,  -2}) == -3);
  }

  SECTION("to_double")
  {
    REQUIRE(static_cast<double>(rational{1u, 2})  == 0.5);
    REQUIRE(static_cast<double>(rational{1u, -2}) == -0.5);
    REQUIRE(static_cast<double>(0_r)     == 0.0);
  }

  SECTION("to<T> reports domain_error for negative rational")
  {
    rational pos{5u, 2};
    rational neg{5,  -2};
    REQUIRE(pos.to<unsigned>().value() == 2u);
    auto r = neg.to<unsigned>();
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == errc::domain_error);
  }

  SECTION("to<T> reports not_a_value for sentinel rational")
  {
    rational s = rational::make_sentinel();
    auto r = s.to<unsigned>();
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == errc::not_a_value);
  }
}

TEST_CASE("rational trunc / floor / round", "[rational][reduce]")
{
  SECTION("trunc — toward zero")
  {
    REQUIRE(rational{7u, 2}.trunc()  ==  3);   //  3.5 -> 3
    REQUIRE(rational{7,  -2}.trunc() == -3);   // -3.5 -> -3
    REQUIRE(rational{1u, 2}.trunc()  ==  0);
    REQUIRE(rational{1,  -2}.trunc() ==  0);
    REQUIRE(rational{4u, 1}.trunc()  ==  4);
    REQUIRE((0_r).trunc()     ==  0);
  }

  SECTION("floor — toward -inf")
  {
    REQUIRE(rational{7u, 2}.floor()  ==  3);   //  3.5 -> 3
    REQUIRE(rational{7,  -2}.floor() == -4);   // -3.5 -> -4
    REQUIRE(rational{1u, 2}.floor()  ==  0);
    REQUIRE(rational{1,  -2}.floor() == -1);   // -0.5 -> -1
    REQUIRE(rational{4u, 1}.floor()  ==  4);
    REQUIRE(rational{4,  -1}.floor() == -4);   // exact integer: no step
    REQUIRE((0_r).floor()     ==  0);
  }

  SECTION("ceil — toward +inf")
  {
    REQUIRE(rational{7u, 2}.ceil()  ==  4);   //  3.5 ->  4
    REQUIRE(rational{7,  -2}.ceil() == -3);   // -3.5 -> -3
    REQUIRE(rational{1u, 2}.ceil()  ==  1);   //  0.5 ->  1
    REQUIRE(rational{1,  -2}.ceil() ==  0);   // -0.5 ->  0
    REQUIRE(rational{4u, 1}.ceil()  ==  4);   // exact integer: no step
    REQUIRE(rational{4,  -1}.ceil() == -4);
    REQUIRE((0_r).ceil()     ==  0);
  }

  SECTION("round — half away from zero")
  {
    REQUIRE(rational{1u, 2}.round() ==  1);    //  0.5 ->  1
    REQUIRE(rational{1,  -2}.round() == -1);   // -0.5 -> -1
    REQUIRE(rational{3u, 2}.round() ==  2);    //  1.5 ->  2
    REQUIRE(rational{3,  -2}.round() == -2);   // -1.5 -> -2
    REQUIRE(rational{1u, 4}.round() ==  0);    //  0.25 -> 0
    REQUIRE(rational{1u, 3}.round() ==  0);    //  ~0.33 -> 0
    REQUIRE(rational{2u, 3}.round() ==  1);    //  ~0.67 -> 1
    REQUIRE(rational{2,  -3}.round() == -1);
    REQUIRE((0_r).round()    ==  0);
  }
}
