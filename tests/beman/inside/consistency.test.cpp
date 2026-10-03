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

//---------------------------------------------------------------------------
// An integer source on a grid it is not on rounds (or reports) exactly like the
// same value given as a rational — never a silent truncation.
//---------------------------------------------------------------------------
TEST(ConsistencyTest, integer_source_off_notch_rounds_like_rational_source)
{
  using N2  = inside<{{0, 10}, 2}, round_nearest>;
  using N2f = inside<{{0, 10}, 2}, round_floor>;
  EXPECT_EQ(rational{N2{3}}, rational{N2{q(3)}});
  EXPECT_EQ(rational{N2{3}}, q(4));                      // 1.5 notches → 2 (half away)
  EXPECT_EQ(rational{N2f{3}}, q(2));
  EXPECT_EQ(rational{N2{-0} }, q(0));

  using Strict = inside<{{0, 10}, 2}>;                    // checked, no rounding mode
  errc ec{};
  Strict s(3, ec);
  EXPECT_EQ(ec, errc::rounding_error);
  EXPECT_THROW((void)Strict{3}, inside_error);

  using Ex = inside<{{0, 10}, 2}, exact | round_nearest>; // rational storage snaps too
  EXPECT_EQ(rational{Ex{3}}, q(4));
}

#ifndef BEMAN_INSIDE_MATH_FIXED
TEST(ConsistencyTest, f64_target_from_integer_snaps_on_every_path)
{
  using F = inside<{{0, 10}, 2}, f64>;
  EXPECT_EQ(F{3}.raw(), 4.0);
  EXPECT_EQ(F::try_make(3)->raw(), 4.0);
  F b = F::from_raw(0.0);
  b.policy<>() = 3;
  EXPECT_EQ(b.raw(), 4.0);
}
#endif

//---------------------------------------------------------------------------
// wrap folds modulo span + notch whatever the source type.
//---------------------------------------------------------------------------
TEST(ConsistencyTest, integer_wrap_uses_span_plus_notch)
{
  using H = inside<{{0, 10}, notch<1, 2>}>;               // span 10, notch 1/2 → modulus 10.5
  EXPECT_EQ(rational{wrap_cast<H>(12)}, rational{wrap_cast<H>(q(12))});
  EXPECT_EQ(rational{wrap_cast<H>(12)}, q(3, 2));
  using N2 = inside<{{0, 10}, 2}, wrap>;                  // modulus 12
  EXPECT_EQ(rational{N2{22}}, rational{N2{q(22)}});
  EXPECT_EQ(rational{N2{22}}, q(10));
  using U = inside<{0, 10}, wrap>;                        // unit notch: modulus 11
  EXPECT_EQ(rational{U{12}}, q(1));
  EXPECT_EQ(rational{U{-1}}, q(10));
}
