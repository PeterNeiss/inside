// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
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

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::detail;

// error_code construction: out-of-range sets ec (value then ill-defined)
TEST(ErrorCodeCtorTest, error_code_construction_out_of_range_sets_ec_value_then_ill_defined)
{
  beman::inside::errc ec{};
  inside<{0, 100}> x(150, ec);
  ASSERT_TRUE(ec != errc{});                                  // EDOM reported, not thrown
  // On error x's value is ill-defined — deliberately NOT read here.
}

// error_code construction: in-range leaves ec clear
TEST(ErrorCodeCtorTest, error_code_construction_in_range_leaves_ec_clear)
{
  beman::inside::errc ec{};
  inside<{0, 100}> x(42, ec);
  ASSERT_EQ(ec, errc{});
  ASSERT_TRUE(static_cast<rational>(x) == 42);
}

// error_code construction respects clamp / wrap (no error)
TEST(ErrorCodeCtorTest, error_code_construction_respects_clamp_wrap_no_error)
{
  {
    beman::inside::errc ec{};
    inside<{0, 100}, clamp> c(150, ec);
    ASSERT_EQ(ec, errc{});                          // clamp is not an error
    ASSERT_TRUE(static_cast<rational>(c) == 100);
  }
  {
    beman::inside::errc ec{};
    inside<{0, 9}, wrap> w(13, ec);
    ASSERT_EQ(ec, errc{});                          // wrap is not an error
    ASSERT_TRUE(static_cast<rational>(w) == 3);     // 13 mod 10
  }
}

// error_code construction matches per-op policy(ec) on the same input
TEST(ErrorCodeCtorTest, error_code_construction_matches_per_op_policy_ec_on_the_same_input)
{
  beman::inside::errc ec_ctor{}, ec_op{};
  inside<{0, 100}> via_ctor(200, ec_ctor);

  inside<{0, 100}> via_op{0};
  via_op.policy(ec_op) = 200;

  ASSERT_EQ((ec_ctor != errc{}), (ec_op != errc{}));
}
