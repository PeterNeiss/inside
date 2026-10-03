// Error-code construction `inside x(value, ec)` must compile and behave as
// documented (docs/policies.md "Error code mode").
//
// Regression: a raw beman::inside::errc inside the generic `inside(A, Pol&&)` ctor
// template as a policy type, so `HasPolicy<L, beman::inside::errc, ...>` was
// ill-formed and the documented form did not compile. A dedicated
// `inside(A, beman::inside::errc&)` overload now wraps ec in the type's own policy.
// On a reported (out-of-range) error the inside's value is ill-defined — the
// caller must check ec before reading it.

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace beman::inside;
using namespace beman::inside::detail;

TEST_CASE("error_code construction: out-of-range sets ec (value then ill-defined)", "[ec][ctor]")
{
  beman::inside::errc ec{};
  inside<{0, 100}> x(150, ec);
  REQUIRE(ec != errc{});                                  // EDOM reported, not thrown
  // On error x's value is ill-defined — deliberately NOT read here.
}

TEST_CASE("error_code construction: in-range leaves ec clear", "[ec][ctor]")
{
  beman::inside::errc ec{};
  inside<{0, 100}> x(42, ec);
  REQUIRE(ec == errc{});
  REQUIRE(static_cast<rational>(x) == 42);
}

TEST_CASE("error_code construction respects clamp / wrap (no error)", "[ec][ctor]")
{
  {
    beman::inside::errc ec{};
    inside<{0, 100}, clamp> c(150, ec);
    REQUIRE(ec == errc{});                          // clamp is not an error
    REQUIRE(static_cast<rational>(c) == 100);
  }
  {
    beman::inside::errc ec{};
    inside<{0, 9}, wrap> w(13, ec);
    REQUIRE(ec == errc{});                          // wrap is not an error
    REQUIRE(static_cast<rational>(w) == 3);     // 13 mod 10
  }
}

TEST_CASE("error_code construction matches per-op policy(ec) on the same input", "[ec][ctor]")
{
  beman::inside::errc ec_ctor{}, ec_op{};
  inside<{0, 100}> via_ctor(200, ec_ctor);

  inside<{0, 100}> via_op{0};
  via_op.policy(ec_op) = 200;

  REQUIRE((ec_ctor != errc{}) == (ec_op != errc{}));
}
