#include "bound/bound.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

using namespace bnd;

TEST_CASE("conversion between bounds with compatible grids", "[bound][assign]")
{
  using f30t40 = bound<{{30, 40}, 2}>;
  using f20t50 = bound<{interval{20, 50}, 1}>;
  f30t40 smaller{34};
  f20t50 bigger;

  bigger = smaller;
  REQUIRE(static_cast<rational>(bigger) == 34);

  // notch 2 -> notch 1 is always compatible (every step lands on integer)
  bigger = 39;
}

TEST_CASE("rounding requires opt-in", "[bound][assign][round]")
{
  using f30t40 = bound<{{30, 40}, 2}>;
  using f20t50 = bound<{interval{20, 50}, 1}>;
  f20t50 bigger{39};
  f30t40 smaller;

  SECTION("policy<ignore_round>() rounds (truncates toward zero on positive)")
  {
    smaller.policy<ignore_round>() = bigger;
    REQUIRE(static_cast<rational>(smaller) == 38);
  }

  SECTION("with_round() is the alias")
  {
    bigger = 30;
    smaller.with_round() = bigger;
    REQUIRE(static_cast<rational>(smaller) == 30);
  }

  SECTION("with_round_nearest() rounds half up to grid")
  {
    using celsius = bound<{{-40, 60}, 0.5}, round_nearest>;
    celsius room = 21.4;
    REQUIRE(static_cast<rational>(room) == rational{43u, 2});

    celsius exact = 21.5;
    REQUIRE(static_cast<rational>(exact) == rational{43u, 2});

    celsius truncated = 21.2;
    REQUIRE(static_cast<rational>(truncated) == 21);

    using half = bound<{{0, 10}, 0.5}>;
    half h;
    h.with_round_nearest() = 3.3;
    REQUIRE(static_cast<rational>(h) == rational{7u, 2});

    h.with_round_nearest() = 3.2;
    REQUIRE(static_cast<rational>(h) == 3);
  }

  SECTION("type-level ignore_round")
  {
    using n2_round = bound<{{0, 10}, 2}, ignore_round>;
    using n1       = bound<{{0, 10}, 1}>;
    n2_round c;
    c = n1{3};
    REQUIRE(static_cast<rational>(c) == 2);
  }

  SECTION("compatible notches require no opt-in")
  {
    using n1 = bound<{{0, 10}, 1}>;
    using n2 = bound<{{0, 10}, 2}>;
    n1 a;
    a = n2{6};
    REQUIRE(static_cast<rational>(a) == 6);
  }

  SECTION("wide compatible interval needs no opt-in")
  {
    using n2   = bound<{{0, 10}, 2}>;
    using wide = bound<{{0, 20}, 2}>;
    n2 b;
    b = wide{6};
    REQUIRE(static_cast<rational>(b) == 6);
  }
}

TEST_CASE("with_clamp / with_wrap per-operation", "[bound][assign][policy]")
{
  using u100 = bound<{0, 100}>;
  u100 x{50};

  x.with_clamp() = 150;
  REQUIRE(static_cast<rational>(x) == 100);

  x.with_wrap() = 103;
  REQUIRE(static_cast<rational>(x) == 2);
}

TEST_CASE("clamp during boundable assignment", "[bound][assign][clamp]")
{
  using wide   = bound<{0, 200}>;
  using narrow = bound<{0, 100}, clamp>;
  wide w{150};
  narrow n{0};
  n = w;
  REQUIRE(static_cast<rational>(n) == 100);
}

TEST_CASE("unsafe relaxes domain and round checks", "[bound][assign][unsafe]")
{
  // Notch-incompatible assignment compiles under unsafe.
  using src = bound<{{0, 100}, 2}, unsafe>;
  using dst = bound<{{0, 100}, 1}, unsafe>;
  src s{50};
  dst d{0};
  d = s;
  REQUIRE(static_cast<rational>(d) == 50);

  // Native int division path engages
  using u100u = bound<{0, 100}, unsafe>;
  u100u a{51}, b{8};
  auto q = a / b;
  STATIC_REQUIRE_FALSE(std::is_same_v<typename decltype(q)::value_type::raw_type, rational>);
  REQUIRE(*q == 6);

  // Binary div by zero -> nullopt
  u100u zero{0};
  REQUIRE_FALSE((a / zero).has_value());

  // Compound /= by 0: ignore_zero is NOT set in unsafe -> still throws
  REQUIRE_THROWS_AS(([&] { u100u y{50}; y /= 0; (void)y; }()), std::system_error);

  // Out-of-range silent overwrite (no domain check)
  REQUIRE_NOTHROW(([&] { u100u x{50}; x = 200; (void)x; }()));
}

TEST_CASE("trivial-type guarantees", "[bound][trivial]")
{
  STATIC_REQUIRE(std::is_trivial_v<bound<{0, 0},      unsafe>>);
  STATIC_REQUIRE(std::is_trivial_v<bound<{0, 100},    unsafe>>);
  STATIC_REQUIRE(std::is_trivial_v<bound<{-100, 100}, unsafe>>);
  STATIC_REQUIRE(std::is_trivial_v<bound<{{0, 10}, rational{1u, 2}}, unsafe>>);
  STATIC_REQUIRE(std::is_trivial_v<bound<{{-10, 10}, 0}, unsafe>>);
  STATIC_REQUIRE(std::is_trivial_v<bound<{0, 100}, clamp>>);
  STATIC_REQUIRE(std::is_trivial_v<bound<{0, 100}, wrap>>);
  STATIC_REQUIRE(std::is_trivial_v<bound<{0, 100}, sentinel>>);

  STATIC_REQUIRE(std::is_trivially_copyable_v<bound<{0, 100}>>);
  STATIC_REQUIRE(std::is_trivially_destructible_v<bound<{0, 100}>>);
  STATIC_REQUIRE_FALSE(std::is_trivially_default_constructible_v<bound<{0, 100}>>);
}

TEST_CASE("type-alias smoke checks", "[bound][types]")
{
  using test0_t = bound<{{1,3}, 1}>;
  using test4_t = bound<{{0u, std::numeric_limits<umax>::max()}, 1}>;
  using test5_t = bound<{1_r}>;
  STATIC_REQUIRE(std::is_same_v<test0_t::raw_type, std::uint8_t>);
  STATIC_REQUIRE(std::is_same_v<test4_t::raw_type, std::uint64_t>);
  STATIC_REQUIRE(std::is_same_v<test5_t::raw_type, rational>);
}
