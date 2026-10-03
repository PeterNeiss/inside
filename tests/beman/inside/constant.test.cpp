// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/detail/rational.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::detail;

//---------------------------------------------------------------------------
// Regression: a single-point source inside (e.g. `0_ins`, any `just<N>`) must
// assign into ANY grid that can exactly represent the value — including grids
// with a non-unit notch, where the whole-range notch mapping is inexact. Before
// the fix, `inside_assignable`'s notch clause rejected these even though the one
// value lands on a notch. The integer path (`b = 0`) already worked, so the
// insidable path was simply inconsistent with it.
//---------------------------------------------------------------------------
// point-inside assignment onto non-unit notch grids
TEST(ConstantTest, point_inside_assignment_onto_non_unit_notch_grids)
{
  // values {0,3,6,9}
  static_assert([]{ inside<{{0,9},3}>  b = 0_ins; return b == 0; }());
  static_assert([]{ inside<{{0,9},3}>  b = 3_ins; return b == 3; }());
  static_assert([]{ inside<{{0,9},3}>  b = 9_ins; return b == 9; }());
  // values {0,2,..,10}
  static_assert([]{ inside<{{0,10},2}> b = 0_ins; return b == 0; }());
  static_assert([]{ inside<{{0,10},2}> b = 4_ins; return b == 4; }());
  // offset grid (non-zero Lower), values {3,6,9}
  static_assert([]{ inside<{{3,9},3}>  b = 6_ins; return b == 6; }());

  // The insidable path now matches the integer path for the same grid.
  static_assert([]{ inside<{{0,9},3}> b = 0;   return b == 0; }());
  static_assert([]{ inside<{{0,9},3}> b = 0_ins; return b == 0; }());
}

// point-inside assignment rejected when not representable
TEST(ConstantTest, point_inside_assignment_rejected_when_not_representable)
{
  // not on a notch
  static_assert(!inside_assignable<inside<{{0,9},3}>,  decltype(1_ins)>);
  static_assert(!inside_assignable<inside<{{0,10},2}>, decltype(3_ins)>);
  // out of range (valid grids: Lower divisible by notch)
  static_assert(!inside_assignable<inside<{{3,9},3}>,  decltype(0_ins)>);
  static_assert(!inside_assignable<inside<{6,12}>,     decltype(0_ins)>);

  // still admitted where representable
  static_assert(inside_assignable<inside<{{0,9},3}>,  decltype(0_ins)>);
  static_assert(inside_assignable<inside<{{3,9},3}>,  decltype(6_ins)>);
}

//---------------------------------------------------------------------------
// beman::inside::zero / beman::inside::one — universal exact constants.
//---------------------------------------------------------------------------
// beman::inside::zero / beman::inside::one assign across storage kinds
TEST(ConstantTest, beman_inside_zero_beman_inside_one_assign_across_storage_kinds)
{
  static_assert([]{ inside<{0,200}>          b = zero; return b == 0; }());
  static_assert([]{ inside<{0,200}>          b = one;  return b == 1; }());
  static_assert([]{ inside<{-40,60}>         b = zero; return b == 0; }());  // signed direct
  static_assert([]{ inside<{-40,60}>         b = one;  return b == 1; }());
  static_assert([]{ inside<{{0,9},3}>        b = zero; return b == 0; }());  // non-unit notch
  // Q8.8: value 1 is raw 256.
  static_assert([]{ inside<{{0,1},0x1p-8_r}> b = one;  return static_cast<imax>(b.raw()) == 256; }());

  // assignment operator (not just construction)
  static_assert([]{ inside<{0,200}> b = 5; b = zero; return b == 0; }());
  static_assert([]{ inside<{0,200}> b = 5; b = one;  return b == 1; }());
}

// beman::inside::zero / beman::inside::one rejected where not representable
TEST(ConstantTest, beman_inside_zero_beman_inside_one_rejected_where_not_representable)
{
  static_assert(!inside_assignable<inside<{5,10}>,    decltype(zero)>);  // 0 out of range
  static_assert(!inside_assignable<inside<{{0,9},3}>, decltype(one)>);   // 1 not on notch
}

// beman::inside::zero / beman::inside::one in comparison and arithmetic
TEST(ConstantTest, beman_inside_zero_beman_inside_one_in_comparison_and_arithmetic)
{
  using B = inside<{0,200}>;

  // comparison (both orders — C++20 rewritten/reversed candidates)
  static_assert(B{0} == zero);
  static_assert(zero == B{0});
  static_assert(B{1} == one);
  static_assert(B{5} >  zero);
  static_assert(zero <  B{5});
  static_assert(B{0} <  one);

  // arithmetic (forwards through the ordinary inside operators)
  static_assert((B{5} + one)  == 6);
  static_assert((one  + B{5}) == 6);
  static_assert((B{5} - zero) == 5);
  static_assert((B{5} - one)  == 4);

  // a runtime check too, for good measure
  B b = 41;
  b = b + one;
  ASSERT_EQ(b, 42);
}
