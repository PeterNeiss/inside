// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::detail;

// interval structured binding
TEST(IntervalTest, interval_structured_binding)
{

  interval iv{-3, 5};
  auto [lo, hi] = iv;
  ASSERT_EQ(lo, -3);
  ASSERT_EQ(hi, 5);

  // Mutating through structured binding writes back to source.
  auto& [mlo, mhi] = iv;
  mlo = -7;
  ASSERT_EQ(iv.Lower, -7);
}

// interval construction and predicates
TEST(IntervalTest, interval_construction_and_predicates)
{
  {
    SCOPED_TRACE("point interval");
    constexpr interval p{*(3_r/4), *(3_r/4)};
    static_assert(p.Lower == p.Upper);
    static_assert(includes(p, *(3_r/4)));
    static_assert(!(includes(p, 0)));
  }

  {
    SCOPED_TRACE("includes / excludes / overlaps");
    constexpr interval a{0, 10};
    constexpr interval b{5, 15};
    constexpr interval c{20, 30};

    static_assert(includes(a, 5));
    static_assert(includes(a, 0));
    static_assert(includes(a, 10));
    static_assert(!(includes(a, 11)));

    static_assert(overlaps(a, b));
    static_assert(!(excludes(a, b)));
    static_assert(excludes(a, c));
    static_assert(!(overlaps(a, c)));
  }

  {
    SCOPED_TRACE("includes whole sub-interval");
    constexpr interval outer{0, 100};
    constexpr interval inner{10, 90};
    static_assert(includes(outer, inner));
    static_assert(!(includes(inner, outer)));
  }
}

// interval arithmetic via lift
TEST(IntervalTest, interval_arithmetic_via_lift)
{
  constexpr interval a{0, 10};
  constexpr interval b{2, 5};

  {
    SCOPED_TRACE("addition");
    constexpr auto r = a + b;
    static_assert(r.has_value());
    static_assert(r->Lower == 2);
    static_assert(r->Upper == 15);
  }

  {
    SCOPED_TRACE("subtraction propagates via -rhs");
    constexpr auto r = a - b;
    static_assert(r.has_value());
    static_assert(r->Lower == -5);
    static_assert(r->Upper == 8);
  }

  {
    SCOPED_TRACE("multiplication takes the bounding box");
    constexpr interval x{-2, 3};
    constexpr interval y{-4, 5};
    constexpr auto r = x * y;
    static_assert(r.has_value());
    static_assert(r->Lower == -12);     // -2 * 6 ... actually min(-2*-4, -2*5, 3*-4, 3*5) = min(8,-10,-12,15) = -12
    static_assert(r->Upper == 15);
  }

  {
    SCOPED_TRACE("division excludes zero in divisor");
    constexpr interval c{-1, 1};            // contains 0
    constexpr auto r = a / c;
    static_assert(!(r.has_value()));
  }

  {
    SCOPED_TRACE("division by positive interval");
    constexpr interval pos{2, 5};           // strictly positive
    constexpr auto r = a / pos;
    static_assert(r.has_value());
  }
}

// interval ordering
TEST(IntervalTest, interval_ordering)
{
  constexpr interval a{0, 10};
  constexpr interval b{20, 30};
  constexpr interval c{5, 15};
  constexpr interval d{0, 10};

  static_assert(a < b);
  static_assert(b > a);
  static_assert(a == d);
  // Overlapping but non-equal → unordered
  static_assert((a <=> c) == std::partial_ordering::unordered);
}

// interval / rational notch
TEST(IntervalTest, interval_rational_notch)
{
  constexpr interval a{0, 10};

  {
    SCOPED_TRACE("divides_evenly");
    static_assert(a.divides_evenly(1_r));
    static_assert(a.divides_evenly(0.5_r));
    static_assert(!(a.divides_evenly(3_r)));
  }

  {
    SCOPED_TRACE("operator/ rational gives notch count");
    constexpr auto r = a / 1_r;
    static_assert(r.has_value());
    static_assert(*r == 10);

    constexpr auto r2 = a / 0.5_r;
    static_assert(r2.has_value());
    static_assert(*r2 == 20);
  }
}

