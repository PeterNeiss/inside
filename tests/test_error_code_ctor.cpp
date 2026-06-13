// Error-code construction `bound x(value, ec)` must compile and behave as
// documented (docs/policies.md "Error code mode").
//
// Regression: a raw std::error_code bound the generic `bound(A, Pol&&)` ctor
// template as a policy type, so `HasPolicy<L, std::error_code, ...>` was
// ill-formed and the documented form did not compile. A dedicated
// `bound(A, std::error_code&)` overload now wraps ec in the type's own policy;
// both 2-arg ctors zero-init Raw so an ec-reported error leaves a defined value
// rather than an indeterminate one.

#include "bound/bound.hpp"
#include "bound/format.hpp"

#include <catch2/catch_test_macros.hpp>
#include <system_error>

using namespace bnd;
using namespace bnd::detail;

TEST_CASE("error_code construction: out-of-range sets ec, value stays defined", "[ec][ctor]")
{
  std::error_code ec;
  bound<{0, 100}> x(150, ec);
  REQUIRE(ec);                                  // EDOM reported, not thrown
  // value must be a defined, in-range point (default), not indeterminate.
  const auto v = static_cast<rational>(x);
  REQUIRE(v >= 0);
  REQUIRE(v <= 100);
}

TEST_CASE("error_code construction: in-range leaves ec clear", "[ec][ctor]")
{
  std::error_code ec;
  bound<{0, 100}> x(42, ec);
  REQUIRE_FALSE(ec);
  REQUIRE(static_cast<rational>(x) == 42);
}

TEST_CASE("error_code construction respects clamp / wrap (no error)", "[ec][ctor]")
{
  {
    std::error_code ec;
    bound<{0, 100}, clamp> c(150, ec);
    REQUIRE_FALSE(ec);                          // clamp is not an error
    REQUIRE(static_cast<rational>(c) == 100);
  }
  {
    std::error_code ec;
    bound<{0, 9}, wrap> w(13, ec);
    REQUIRE_FALSE(ec);                          // wrap is not an error
    REQUIRE(static_cast<rational>(w) == 3);     // 13 mod 10
  }
}

TEST_CASE("error_code construction matches per-op policy(ec) on the same input", "[ec][ctor]")
{
  std::error_code ec_ctor, ec_op;
  bound<{0, 100}> via_ctor(200, ec_ctor);

  bound<{0, 100}> via_op{0};
  via_op.policy(ec_op) = 200;

  REQUIRE(static_cast<bool>(ec_ctor) == static_cast<bool>(ec_op));
}
