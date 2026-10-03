// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/detail/rational.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string_view>

using namespace beman::inside;
using namespace beman::inside::detail;

namespace
{
  using opt_r = slim::optional<rational>;
}

// optional<rational> * optional<rational>
TEST(OptionalArithmeticTest, optional_rational_optional_rational)
{
  opt_r a{0.75_r};
  opt_r b{rational{2, 3}};

  auto r = a * b;
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(*r, 0.5_r);

  opt_r empty{slim::nullopt};
  ASSERT_FALSE((empty * b).has_value());
  ASSERT_FALSE((a * empty).has_value());
  ASSERT_FALSE((empty * empty).has_value());
}

// optional<rational> + optional<rational>
TEST(OptionalArithmeticTest, optional_rational_plus_optional_rational)
{
  opt_r a{0.25_r};
  opt_r b{0.5_r};

  auto r = a + b;
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(*r, 0.75_r);

  opt_r empty{slim::nullopt};
  ASSERT_FALSE((empty + a).has_value());
  ASSERT_FALSE((a + empty).has_value());
}

// optional<rational> - optional<rational>
TEST(OptionalArithmeticTest, optional_rational_optional_rational_2)
{
  opt_r a{0.75_r};
  opt_r b{0.5_r};
  auto r = a - b;
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(*r, 0.25_r);
}

// optional<rational> / optional<rational>
TEST(OptionalArithmeticTest, optional_rational_optional_rational_3)
{
  opt_r a{0.75_r};
  opt_r b{0.5_r};
  auto r = a / b;
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(*r, 1.5_r);

  opt_r zero{rational{0}};
  ASSERT_FALSE((a / zero).has_value());
}

// optional<rational> op arithmetic and symmetric
TEST(OptionalArithmeticTest, optional_rational_op_arithmetic_and_symmetric)
{
  opt_r a{0.5_r};

  ASSERT_EQ(*(a + 1), 1.5_r);
  ASSERT_EQ(*(1 + a), 1.5_r);
  ASSERT_EQ(*(a - 1), -0.5_r);
  ASSERT_EQ(*(1 - a), 0.5_r);
  ASSERT_EQ(*(a * 2), (rational{1, 1}));
  ASSERT_EQ(*(2 * a), (rational{1, 1}));
  ASSERT_EQ(*(a / 2), 0.25_r);
  ASSERT_EQ(*(2 / a), (rational{4, 1}));

  opt_r empty{slim::nullopt};
  ASSERT_FALSE((empty + 1).has_value());
  ASSERT_FALSE((1 + empty).has_value());
}

// unary -optional<rational>
TEST(OptionalArithmeticTest, unary_optional_rational)
{
  opt_r a{0.75_r};
  auto r = -a;
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(*r, -0.75_r);

  opt_r empty{slim::nullopt};
  ASSERT_FALSE((-empty).has_value());
}

// inside construction from optional<rational> - sink unwrap
TEST(OptionalArithmeticTest, inside_construction_from_optional_rational_sink_unwrap)
{
  using b_t = inside<{{0, 1}, notch<1, 16>}, round_nearest>;

  opt_r ok{0.5_r};
  b_t  v{ok};
  ASSERT_EQ(rational{v}, 0.5_r);

  opt_r empty{slim::nullopt};
  ASSERT_THROW((void)(b_t{empty}), slim::bad_optional_access);
}

// inside operator= from optional<rational>
TEST(OptionalArithmeticTest, inside_operator_from_optional_rational)
{
  using b_t = inside<{{0, 1}, notch<1, 16>}, round_nearest>;

  b_t v{0};
  opt_r ok{0.25_r};
  v = ok;
  ASSERT_EQ(rational{v}, 0.25_r);

  opt_r empty{slim::nullopt};
  ASSERT_THROW((void)(v = empty), slim::bad_optional_access);
}

// optional<inside> + rational propagates nullopt
TEST(OptionalArithmeticTest, optional_inside_plus_rational_propagates_nullopt)
{
  using b_t = inside<{{0, 1}, notch<1, 16>}, round_nearest>;
  slim::optional<b_t> some{b_t{0.5_r}};

  auto r = some + 0.25_r;
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(*r, 0.75_r);

  slim::optional<b_t> empty{slim::nullopt};
  ASSERT_FALSE((empty + 0.25_r).has_value());
  ASSERT_FALSE((0.25_r + empty).has_value());
  ASSERT_FALSE((empty - 0.25_r).has_value());
  ASSERT_FALSE((empty * 0.25_r).has_value());
  ASSERT_FALSE((empty / 0.25_r).has_value());
}

// optional<rational> value_or
TEST(OptionalArithmeticTest, optional_rational_value_or)
{
  ASSERT_EQ(opt_r{slim::nullopt}.value_or(7_r), 7_r);
  ASSERT_EQ(opt_r{0.5_r}.value_or(7_r), 0.5_r);
}

// optional<rational> rejects sentinel construction
TEST(OptionalArithmeticTest, optional_rational_rejects_sentinel_construction)
{
  // The {N, 0} slot is the trait's sentinel; constructing a non-empty optional
  // from it must throw rather than masquerade as a held value.
  ASSERT_THROW((void)(opt_r{rational::make_sentinel()}), slim::bad_optional_access);

  try
  {
    opt_r bad{rational::make_sentinel()};
    (void)bad;
    FAIL() << "expected bad_optional_access";
  }
  catch (const slim::bad_optional_access& e)
  {
    ASSERT_TRUE(std::string_view{e.what()}.size() > 0);
  }
}
