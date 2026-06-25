#include "bound/bound.hpp"
#include "bound/io.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bnd;
using namespace bnd::detail;

// Raw int/float/double are no longer arithmetic-mutation operands: `b += 1`,
// `b *= 2.0` are ill-formed (guidance static_assert in arithmetic.hpp). The only
// non-bound operand a compound assign accepts is a `rational`. As with the binary
// operators, the guidance overloads are SFINAE-transparent (the static_assert
// fires only on a real call), so the ill-formedness can't be probed with
// `requires` — only the sanctioned spellings are positively testable.
TEST_CASE("compound assignment: sanctioned RHS compiles", "[bound][compound]")
{
  using u100 = bound<{0, 100}>;
  STATIC_REQUIRE( requires(u100 b){ b += 1_b; });            // a bound
  STATIC_REQUIRE( requires(u100 b){ b += rational{1}; });    // a rational
}

TEST_CASE("compound assignment: boundable RHS", "[bound][compound]")
{
  using u100 = bound<{0, 100}>;
  STATIC_REQUIRE([]{ u100 a{50}, d{20}; a -= d; return a.as<imax>(); }() == 30);

  using ui = bound<{0, 100}, snap>;
  STATIC_REQUIRE([]{ ui d{50}, two{2}; d *= two; return d.as<imax>(); }() == 100);
  STATIC_REQUIRE([]{ ui e{60}, three{3}; e /= three; return e.as<imax>(); }() == 20);
  STATIC_REQUIRE([]{ ui f{17}, five{5}; f %= five; return f.as<imax>(); }() == 2);
}

TEST_CASE("compound /= and %= by a zero bound report by default", "[bound][compound]")
{
  using u100 = bound<{0, 100}>;
  u100 a{50};
  REQUIRE_THROWS_AS(([&]{ a /= u100{0}; }()), bnd::bound_error);

  using ui = bound<{0, 100}, snap>;
  ui s{50};
  REQUIRE_THROWS_AS(([&]{ s %= ui{0}; }()), bnd::bound_error);
}

TEST_CASE("compound assignment: rational RHS", "[bound][compound][rational]")
{
  // round_nearest required so the bound's rational assignment path is available.
  using rn = bound<{{0, 100}, notch<1, 100>}, round_nearest>;

  rn a{0.5_r};
  a += 0.25_r;                       // 0.50 + 0.25 = 0.75
  REQUIRE(a == rn{0.75_r});

  a -= 0.25_r;                       // 0.75 − 0.25 = 0.50
  REQUIRE(a == rn{0.5_r});

  a *= 4_r;                          // 0.50 × 4 = 2
  REQUIRE(a == rn{2_r});

  a /= 2_r;                          // 2 / 2 = 1
  REQUIRE(a == rn{1_r});
}

TEST_CASE("compound assignment: fractional RHS via rational", "[bound][compound][rational]")
{
  using rn = bound<{{-100, 100}, notch<1, 16>}, round_nearest>;

  rn a{1.0};                         // construction from a double is unchanged
  a += 2.5_r;
  REQUIRE(a == rn{3.5});

  a -= 1.5_r;
  REQUIRE(a == rn{2.0});

  a *= 1.5_r;
  REQUIRE(a == rn{3.0});

  a /= 2_r;
  REQUIRE(a == rn{1.5});
}

TEST_CASE("compound /= 0_r (rational zero) reports error", "[bound][compound][rational]")
{
  using rn = bound<{{0, 100}, notch<1, 100>}, round_nearest>;
  rn a{0.5_r};
  REQUIRE_THROWS_AS(([&]{ a /= 0_r; }()), bnd::bound_error);
}

TEST_CASE("increment / decrement", "[bound][compound][inc]")
{
  using u10 = bound<{0, 10}>;
  // The ++/-- happens inside each lambda body, fully sequenced before the lambda
  // returns; the `== N` compares the lambda's *result*, not a mutated operand. So
  // bugprone-inc-dec-in-conditions (which assumes a side-effect inside the
  // condition) is a false positive from the STATIC_REQUIRE macro expansion.
  // NOLINTBEGIN(bugprone-inc-dec-in-conditions)
  STATIC_REQUIRE([]{ u10 a{5}; ++a; return a.as<imax>(); }() == 6);
  STATIC_REQUIRE([]{ u10 a{5}; a++; return a.as<imax>(); }() == 6);
  STATIC_REQUIRE([]{ u10 a{5}; --a; return a.as<imax>(); }() == 4);
  STATIC_REQUIRE([]{ u10 a{5}; a--; return a.as<imax>(); }() == 4);

  // post-inc/dec returns the old value
  STATIC_REQUIRE([]{ u10 a{5}; auto r = a++; return r.as<imax>(); }() == 5);
  STATIC_REQUIRE([]{ u10 a{5}; auto r = a--; return r.as<imax>(); }() == 5);
  // NOLINTEND(bugprone-inc-dec-in-conditions)
}
