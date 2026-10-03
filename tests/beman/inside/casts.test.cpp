#include <beman/inside/inside.hpp>
#include <beman/inside/numeric_limits.hpp>
#include <beman/inside/predicates.hpp>

#include <catch2/catch_test_macros.hpp>

#include <unordered_set>
#include <limits>

using namespace beman::inside;
using namespace beman::inside::detail;

//---------------------------------------------------------------------------
// std::numeric_limits<inside>
//---------------------------------------------------------------------------
TEST_CASE("numeric_limits reports grid bounds", "[inside][numeric_limits]")
{
  using pct = inside<{0, 100}>;
  using nl  = std::numeric_limits<pct>;

  STATIC_REQUIRE(nl::is_specialized);
  STATIC_REQUIRE(nl::is_bounded);
  STATIC_REQUIRE(nl::is_integer);
  STATIC_REQUIRE(nl::is_exact);
  STATIC_REQUIRE_FALSE(nl::is_signed);
  STATIC_REQUIRE_FALSE(nl::is_modulo);

  STATIC_REQUIRE(nl::min()    == pct{0});
  STATIC_REQUIRE(nl::max()    == pct{100});
  STATIC_REQUIRE(nl::lowest() == pct{0});
}

TEST_CASE("numeric_limits handles signed and wrapping bounds", "[inside][numeric_limits]")
{
  using temp = inside<{-40, 60}>;
  STATIC_REQUIRE(std::numeric_limits<temp>::is_signed);
  STATIC_REQUIRE(std::numeric_limits<temp>::lowest() == temp{-40});

  using ang = inside<{0, 359}, wrap>;
  STATIC_REQUIRE(std::numeric_limits<ang>::is_modulo);
}

//---------------------------------------------------------------------------
// std::hash<inside>
//---------------------------------------------------------------------------
TEST_CASE("hash specialization works with unordered_set", "[inside][hash]")
{
  using idx = inside<{0, 9}>;
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
TEST_CASE("will_conversion_overflow", "[inside][predicates]")
{
  using pct = inside<{0, 100}>;

  STATIC_REQUIRE_FALSE(will_conversion_overflow<pct>(50));
  STATIC_REQUIRE      (will_conversion_overflow<pct>(150));
  STATIC_REQUIRE      (will_conversion_overflow<pct>(-1));
  STATIC_REQUIRE_FALSE(will_conversion_overflow<pct>(0));
  STATIC_REQUIRE_FALSE(will_conversion_overflow<pct>(100));
}

TEST_CASE("will_conversion_trunc detects non-notch values", "[inside][predicates]")
{
  using coarse = inside<{{0, 10}, 2}>;          // notch 2

  STATIC_REQUIRE_FALSE(will_conversion_trunc<coarse>(0));
  STATIC_REQUIRE_FALSE(will_conversion_trunc<coarse>(4));
  STATIC_REQUIRE      (will_conversion_trunc<coarse>(3));     // doesn't land on 2-notch
  STATIC_REQUIRE_FALSE(will_conversion_trunc<coarse>(11)); // out of range, not truncation
}

TEST_CASE("is_conversion_lossy combines both", "[inside][predicates]")
{
  using coarse = inside<{{0, 10}, 2}>;

  STATIC_REQUIRE_FALSE(is_conversion_lossy<coarse>(4));
  STATIC_REQUIRE      (is_conversion_lossy<coarse>(3));   // truncation
  STATIC_REQUIRE      (is_conversion_lossy<coarse>(20));  // overflow
}

//---------------------------------------------------------------------------
// clamp_cast / checked_cast / unchecked_cast
//---------------------------------------------------------------------------
TEST_CASE("clamp_cast clamps to boundary", "[inside][cast]")
{
  using pct = inside<{0, 100}>;

  REQUIRE(clamp_cast<pct>(150) == pct{100});
  REQUIRE(clamp_cast<pct>(-5)  == pct{0});
  REQUIRE(clamp_cast<pct>(42)  == pct{42});
}

TEST_CASE("checked_cast throws on out-of-range", "[inside][cast]")
{
  using pct = inside<{0, 100}>;

  STATIC_REQUIRE(checked_cast<pct>(42) == pct{42});
  REQUIRE_THROWS_AS(checked_cast<pct>(150), beman::inside::inside_error);
  REQUIRE_THROWS_AS(checked_cast<pct>(-1),  beman::inside::inside_error);
}

TEST_CASE("checked_cast throws on truncation", "[inside][cast]")
{
  using coarse = inside<{{0, 10}, 2}>;

  STATIC_REQUIRE(checked_cast<coarse>(4) == coarse{4});
  REQUIRE_THROWS_AS(checked_cast<coarse>(3), beman::inside::inside_error);
}

TEST_CASE("unchecked_cast bypasses runtime checks", "[inside][cast]")
{
  using pct = inside<{0, 100}>;

  // In-range value: same result as checked_cast.
  STATIC_REQUIRE(unchecked_cast<pct>(42) == pct{42});
  STATIC_REQUIRE(unchecked_cast<pct>(0)  == pct{0});
}

//---------------------------------------------------------------------------
// _ins literal
//---------------------------------------------------------------------------
TEST_CASE("_ins literal produces just<N>", "[inside][literal]")
{
  constexpr auto five = 5_ins;
  STATIC_REQUIRE(Lower<decltype(five)> == 5);
  STATIC_REQUIRE(Upper<decltype(five)> == 5);
  STATIC_REQUIRE(five == 5);

  // Composes with inside arithmetic — grid widens through addition.
  using pct = inside<{0, 100}>;
  STATIC_REQUIRE(10_ins + pct{40} == 50);
}

//---------------------------------------------------------------------------
// add_all / mul_all
//---------------------------------------------------------------------------
TEST_CASE("add_all / mul_all fold variadically", "[inside][fold]")
{
  using v = inside<{0, 100}>;
  constexpr v a{10}, b{20}, c{30}, d{40};
  STATIC_REQUIRE(add_all(a, b, c, d) == 100);

  constexpr v p{2}, q{3}, r{5};
  STATIC_REQUIRE(mul_all(p, q, r) == 30);
}

//---------------------------------------------------------------------------
// rounding modes (with_snap<round_floor> / with_snap<round_ceil> / with_snap<round_half_even>)
//---------------------------------------------------------------------------
// Rounding modes apply when rhs is real-valued (float, double, rational).
// Integer rhs takes the truncation fast path which is *intentionally*
// rounding-policy-agnostic — see assignment.hpp:store(integral).
TEST_CASE("with_snap<round_floor> rounds toward -inf for double rhs", "[inside][round]")
{
  using coarse = inside<{{0, 10}, 2}>;
  coarse c{0};

  c.with_snap<round_floor>() = 3.0;
  REQUIRE(c == 2);

  c.with_snap<round_floor>() = 4.0;
  REQUIRE(c == 4);

  c.with_snap<round_floor>() = 5.0;
  REQUIRE(c == 4);
}

TEST_CASE("with_snap<round_ceil> rounds toward +inf for double rhs", "[inside][round]")
{
  using coarse = inside<{{0, 10}, 2}>;
  coarse c{0};

  c.with_snap<round_ceil>() = 3.0;
  REQUIRE(c == 4);

  c.with_snap<round_ceil>() = 4.0;
  REQUIRE(c == 4);

  c.with_snap<round_ceil>() = 5.0;
  REQUIRE(c == 6);
}

TEST_CASE("with_snap<round_half_even> applies banker's rounding", "[inside][round]")
{
  using coarse = inside<{{0, 10}, 2}>;
  coarse c{0};

  // 1.0 is the half-way point between notch 0 and notch 2 → even wins (0).
  c.with_snap<round_half_even>() = 1.0;
  REQUIRE(c == 0);

  // 3.0 is half-way between 2 and 4 → even wins (4).
  c.with_snap<round_half_even>() = 3.0;
  REQUIRE(c == 4);

  // 5.0 is halfway between 4 and 6 → even wins (4).
  c.with_snap<round_half_even>() = 5.0;
  REQUIRE(c == 4);

  // 7.0 is halfway between 6 and 8 → even wins (8).
  c.with_snap<round_half_even>() = 7.0;
  REQUIRE(c == 8);
}

//---------------------------------------------------------------------------
// clamp_floor / clamp_ceil / clamp_round
//---------------------------------------------------------------------------
TEST_CASE("clamp_floor / clamp_ceil / clamp_round compose clamp + round", "[inside][cast][round]")
{
  using coarse = inside<{{0, 10}, 2}>;

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

//---------------------------------------------------------------------------
// clamp_* accept a notch-incompatible *inside* source. The one-shot rounding
// policy widens the value+policy constructor's assignable check, so a finer
// grid rounds onto the target (not just arithmetic sources).
//---------------------------------------------------------------------------
TEST_CASE("clamp_floor / clamp_ceil / clamp_round accept an inside source",
          "[inside][cast][round][inside2inside]")
{
  using small = inside<{{0, 10}, notch<1, 10>}, clamp>;   // 1/10 grid
  small a  = 2.5;                                          // exact on the 1/10 grid
  auto  sq = a * a;                                        // exact 6.25 on the 1/100 grid

  REQUIRE(clamp_floor<small>(sq) == 6.2_r);   // toward −∞
  REQUIRE(clamp_ceil <small>(sq) == 6.3_r);   // toward +∞
  REQUIRE(clamp_round<small>(sq) == 6.3_r);   // 6.25 tie → half away → 6.3

  small four = 4;                                          // 16 is out of range
  REQUIRE(clamp_round<small>(four * four) == 10);          // clamp to boundary

  REQUIRE(clamp_round<small>(150.0) == 10);                // arithmetic source: no regression
}
