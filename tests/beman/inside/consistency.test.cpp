// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Cross-path consistency: the same value, policy and grid must give the same
// result whichever code path (storage kind, assignment source, fast path,
// engine) handles it.

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>
#include <beman/inside/detail/rational.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::detail;

namespace { rational q(imax n, imax d = 1) { return rational{n, d}; } }

#ifndef BEMAN_INSIDE_MATH_FIXED   // f64 storage is compiled out under the integer engine

//---------------------------------------------------------------------------
// One tie rule (half away from zero) and honoured rounding modes on f64 storage.
//---------------------------------------------------------------------------
TEST(ConsistencyTest, f64_storage_rounds_like_integer_storage)
{
  using F = inside<{{-4, 4}, notch<1, 2>}, f64>;               // round_nearest
  using I = inside<{{-4, 4}, notch<1, 2>}, round_nearest>;
  for (rational v : {q(-3, 4), q(-1, 4), q(1, 4), q(3, 4), q(-5, 4), q(5, 4)})
    EXPECT_EQ(rational{F{v}}, rational{I{v}}) << "v = " << static_cast<double>(v);
  EXPECT_EQ(F{q(-1, 4)}.raw(), -0.5);                   // half away from zero
}

TEST(ConsistencyTest, f64_storage_honours_rounding_mode)
{
  using Fl = inside<{{-4, 4}, notch<1, 2>}, f64 | round_floor>;
  using Ce = inside<{{-4, 4}, notch<1, 2>}, f64 | round_ceil>;
  using He = inside<{{-4, 4}, notch<1, 2>}, f64 | round_half_even>;
  EXPECT_EQ(Fl{q(2, 5)}.raw(), 0.0);
  EXPECT_EQ(Fl{q(-1, 10)}.raw(), -0.5);
  EXPECT_EQ(Ce{q(1, 10)}.raw(), 0.5);
  EXPECT_EQ(He{q(3, 4)}.raw(), 1.0);                    // 1.5 notches → 2 (even)
  EXPECT_EQ(He{q(1, 4)}.raw(), 0.0);                    // 0.5 notches → 0 (even)
}

#endif

// The math store fast path rounds ties like the assignment path.
TEST(ConsistencyTest, math_store_grid_tie_rule)
{
  using O = inside<{-10, 10}, round_nearest>;
  EXPECT_EQ(rational{math::detail::store_grid<O>(q(-1, 2))}, rational{O{q(-1, 2)}});
  EXPECT_EQ(rational{math::detail::store_grid<O>(q(-1, 2))}, q(-1));
  EXPECT_EQ(rational{math::detail::store_grid<O>(q(1, 2))}, q(1));
}
