#include "bound/bound.hpp"
#include "bound/numeric_limits.hpp"
#include "bound/predicates.hpp"

#include <catch2/catch_test_macros.hpp>

#include <unordered_set>
#include <limits>

using namespace bnd;

//---------------------------------------------------------------------------
// std::numeric_limits<bound>
//---------------------------------------------------------------------------
TEST_CASE("numeric_limits reports grid bounds", "[bound][numeric_limits]")
{
  using pct = bound<{0, 100}>;
  using nl  = std::numeric_limits<pct>;

  REQUIRE(nl::is_specialized);
  REQUIRE(nl::is_bounded);
  REQUIRE(nl::is_integer);
  REQUIRE(nl::is_exact);
  REQUIRE_FALSE(nl::is_signed);
  REQUIRE_FALSE(nl::is_modulo);

  REQUIRE(nl::min()    == pct{0});
  REQUIRE(nl::max()    == pct{100});
  REQUIRE(nl::lowest() == pct{0});
}

TEST_CASE("numeric_limits handles signed and wrapping bounds", "[bound][numeric_limits]")
{
  using temp = bound<{-40, 60}>;
  REQUIRE(std::numeric_limits<temp>::is_signed);
  REQUIRE(std::numeric_limits<temp>::lowest() == temp{-40});

  using ang = bound<{0, 359}, wrap>;
  REQUIRE(std::numeric_limits<ang>::is_modulo);
}

//---------------------------------------------------------------------------
// std::hash<bound>
//---------------------------------------------------------------------------
TEST_CASE("hash specialization works with unordered_set", "[bound][hash]")
{
  using idx = bound<{0, 9}>;
  std::unordered_set<idx> s;
  s.insert(idx{3});
  s.insert(idx{7});
  s.insert(idx{3});

  REQUIRE(s.size() == 2);
  REQUIRE(s.contains(idx{3}));
  REQUIRE_FALSE(s.contains(idx{5}));
}

//---------------------------------------------------------------------------
// predicates
//---------------------------------------------------------------------------
TEST_CASE("will_conversion_overflow", "[bound][predicates]")
{
  using pct = bound<{0, 100}>;

  REQUIRE_FALSE(will_conversion_overflow<pct>(50));
  REQUIRE(will_conversion_overflow<pct>(150));
  REQUIRE(will_conversion_overflow<pct>(-1));
  REQUIRE_FALSE(will_conversion_overflow<pct>(0));
  REQUIRE_FALSE(will_conversion_overflow<pct>(100));
}

TEST_CASE("will_conversion_truncate detects non-notch values", "[bound][predicates]")
{
  using coarse = bound<{{0, 10}, 2}>;          // notch 2

  REQUIRE_FALSE(will_conversion_truncate<coarse>(0));
  REQUIRE_FALSE(will_conversion_truncate<coarse>(4));
  REQUIRE(will_conversion_truncate<coarse>(3));     // doesn't land on 2-notch
  REQUIRE_FALSE(will_conversion_truncate<coarse>(11)); // out of range, not truncation
}

TEST_CASE("is_conversion_lossy combines both", "[bound][predicates]")
{
  using coarse = bound<{{0, 10}, 2}>;

  REQUIRE_FALSE(is_conversion_lossy<coarse>(4));
  REQUIRE(is_conversion_lossy<coarse>(3));   // truncation
  REQUIRE(is_conversion_lossy<coarse>(20));  // overflow
}

//---------------------------------------------------------------------------
// saturated_cast / checked_cast / unchecked_cast
//---------------------------------------------------------------------------
TEST_CASE("saturated_cast clamps to boundary", "[bound][cast]")
{
  using pct = bound<{0, 100}>;

  REQUIRE(saturated_cast<pct>(150) == pct{100});
  REQUIRE(saturated_cast<pct>(-5)  == pct{0});
  REQUIRE(saturated_cast<pct>(42)  == pct{42});
}

TEST_CASE("checked_cast throws on out-of-range", "[bound][cast]")
{
  using pct = bound<{0, 100}>;

  REQUIRE(checked_cast<pct>(42) == pct{42});
  REQUIRE_THROWS_AS(checked_cast<pct>(150), std::system_error);
  REQUIRE_THROWS_AS(checked_cast<pct>(-1),  std::system_error);
}

TEST_CASE("checked_cast throws on truncation", "[bound][cast]")
{
  using coarse = bound<{{0, 10}, 2}>;

  REQUIRE(checked_cast<coarse>(4) == coarse{4});
  REQUIRE_THROWS_AS(checked_cast<coarse>(3), std::system_error);
}

TEST_CASE("unchecked_cast bypasses runtime checks", "[bound][cast]")
{
  using pct = bound<{0, 100}>;

  // In-range value: same result as checked_cast.
  REQUIRE(unchecked_cast<pct>(42) == pct{42});
  REQUIRE(unchecked_cast<pct>(0)  == pct{0});
}

//---------------------------------------------------------------------------
// _b literal
//---------------------------------------------------------------------------
TEST_CASE("_b literal produces just<N>", "[bound][literal]")
{
  constexpr auto five = 5_b;
  STATIC_REQUIRE(Lower<decltype(five)> == 5_r);
  STATIC_REQUIRE(Upper<decltype(five)> == 5_r);
  REQUIRE(five == 5);

  // Composes with bound arithmetic — grid widens through addition.
  using pct = bound<{0, 100}>;
  pct p{40};
  auto s = 10_b + p;
  REQUIRE(s == 50);
}

//---------------------------------------------------------------------------
// add_all / mul_all
//---------------------------------------------------------------------------
TEST_CASE("add_all / mul_all fold variadically", "[bound][fold]")
{
  using v = bound<{0, 100}>;
  v a{10}, b{20}, c{30}, d{40};

  auto s = add_all(a, b, c, d);
  REQUIRE(s == 100);

  v p{2}, q{3}, r{5};
  auto prod = mul_all(p, q, r);
  REQUIRE(prod == 30);
}

//---------------------------------------------------------------------------
// rounding modes (with_floor / with_ceil / with_round_half_even)
//---------------------------------------------------------------------------
// Rounding modes apply when rhs is real-valued (float, double, rational).
// Integer rhs takes the truncation fast path which is *intentionally*
// rounding-policy-agnostic — see assignment.hpp:store(integral).
TEST_CASE("with_floor rounds toward -inf for double rhs", "[bound][round]")
{
  using coarse = bound<{{0, 10}, 2}>;
  coarse c{0};

  c.with_floor() = 3.0;
  REQUIRE(c == 2);

  c.with_floor() = 4.0;
  REQUIRE(c == 4);

  c.with_floor() = 5.0;
  REQUIRE(c == 4);
}

TEST_CASE("with_ceil rounds toward +inf for double rhs", "[bound][round]")
{
  using coarse = bound<{{0, 10}, 2}>;
  coarse c{0};

  c.with_ceil() = 3.0;
  REQUIRE(c == 4);

  c.with_ceil() = 4.0;
  REQUIRE(c == 4);

  c.with_ceil() = 5.0;
  REQUIRE(c == 6);
}

TEST_CASE("with_round_half_even applies banker's rounding", "[bound][round]")
{
  using coarse = bound<{{0, 10}, 2}>;
  coarse c{0};

  // 1.0 is the half-way point between notch 0 and notch 2 → even wins (0).
  c.with_round_half_even() = 1.0;
  REQUIRE(c == 0);

  // 3.0 is half-way between 2 and 4 → even wins (4).
  c.with_round_half_even() = 3.0;
  REQUIRE(c == 4);

  // 5.0 is halfway between 4 and 6 → even wins (4).
  c.with_round_half_even() = 5.0;
  REQUIRE(c == 4);

  // 7.0 is halfway between 6 and 8 → even wins (8).
  c.with_round_half_even() = 7.0;
  REQUIRE(c == 8);
}

//---------------------------------------------------------------------------
// clamp_floor / clamp_ceil / clamp_round
//---------------------------------------------------------------------------
TEST_CASE("clamp_floor / clamp_ceil / clamp_round compose clamp + round", "[bound][cast][round]")
{
  using coarse = bound<{{0, 10}, 2}>;

  // In-range, off-notch: round per mode.
  REQUIRE(clamp_floor<coarse>(3.0) == coarse{2});
  REQUIRE(clamp_ceil <coarse>(3.0) == coarse{4});
  REQUIRE(clamp_round<coarse>(3.0) == coarse{4});

  // Out-of-range: clamp to boundary.
  REQUIRE(clamp_floor<coarse>(15.0) == coarse{10});
  REQUIRE(clamp_ceil <coarse>(15.0) == coarse{10});
  REQUIRE(clamp_round<coarse>(15.0) == coarse{10});

  REQUIRE(clamp_floor<coarse>(-3.0) == coarse{0});
}
