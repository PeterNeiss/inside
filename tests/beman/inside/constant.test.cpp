#include <beman/inside/inside.hpp>
#include <beman/inside/detail/rational.hpp>

#include <catch2/catch_test_macros.hpp>

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
TEST_CASE("point-inside assignment onto non-unit notch grids", "[inside][assign][notch]")
{
  // values {0,3,6,9}
  STATIC_REQUIRE([]{ inside<{{0,9},3}>  b = 0_ins; return b == 0; }());
  STATIC_REQUIRE([]{ inside<{{0,9},3}>  b = 3_ins; return b == 3; }());
  STATIC_REQUIRE([]{ inside<{{0,9},3}>  b = 9_ins; return b == 9; }());
  // values {0,2,..,10}
  STATIC_REQUIRE([]{ inside<{{0,10},2}> b = 0_ins; return b == 0; }());
  STATIC_REQUIRE([]{ inside<{{0,10},2}> b = 4_ins; return b == 4; }());
  // offset grid (non-zero Lower), values {3,6,9}
  STATIC_REQUIRE([]{ inside<{{3,9},3}>  b = 6_ins; return b == 6; }());

  // The insidable path now matches the integer path for the same grid.
  STATIC_REQUIRE([]{ inside<{{0,9},3}> b = 0;   return b == 0; }());
  STATIC_REQUIRE([]{ inside<{{0,9},3}> b = 0_ins; return b == 0; }());
}

TEST_CASE("point-inside assignment rejected when not representable", "[inside][assign][notch]")
{
  // not on a notch
  STATIC_REQUIRE(!inside_assignable<inside<{{0,9},3}>,  decltype(1_ins)>);
  STATIC_REQUIRE(!inside_assignable<inside<{{0,10},2}>, decltype(3_ins)>);
  // out of range (valid grids: Lower divisible by notch)
  STATIC_REQUIRE(!inside_assignable<inside<{{3,9},3}>,  decltype(0_ins)>);
  STATIC_REQUIRE(!inside_assignable<inside<{6,12}>,     decltype(0_ins)>);

  // still admitted where representable
  STATIC_REQUIRE( inside_assignable<inside<{{0,9},3}>,  decltype(0_ins)>);
  STATIC_REQUIRE( inside_assignable<inside<{{3,9},3}>,  decltype(6_ins)>);
}

//---------------------------------------------------------------------------
// beman::inside::zero / beman::inside::one — universal exact constants.
//---------------------------------------------------------------------------
TEST_CASE("beman::inside::zero / beman::inside::one assign across storage kinds", "[inside][constant]")
{
  STATIC_REQUIRE([]{ inside<{0,200}>          b = zero; return b == 0; }());
  STATIC_REQUIRE([]{ inside<{0,200}>          b = one;  return b == 1; }());
  STATIC_REQUIRE([]{ inside<{-40,60}>         b = zero; return b == 0; }());  // signed direct
  STATIC_REQUIRE([]{ inside<{-40,60}>         b = one;  return b == 1; }());
  STATIC_REQUIRE([]{ inside<{{0,9},3}>        b = zero; return b == 0; }());  // non-unit notch
  // Q8.8: value 1 is raw 256.
  STATIC_REQUIRE([]{ inside<{{0,1},0x1p-8_r}> b = one;  return static_cast<imax>(b.raw()) == 256; }());

  // assignment operator (not just construction)
  STATIC_REQUIRE([]{ inside<{0,200}> b = 5; b = zero; return b == 0; }());
  STATIC_REQUIRE([]{ inside<{0,200}> b = 5; b = one;  return b == 1; }());
}

TEST_CASE("beman::inside::zero / beman::inside::one rejected where not representable", "[inside][constant]")
{
  STATIC_REQUIRE(!inside_assignable<inside<{5,10}>,    decltype(zero)>);  // 0 out of range
  STATIC_REQUIRE(!inside_assignable<inside<{{0,9},3}>, decltype(one)>);   // 1 not on notch
}

TEST_CASE("beman::inside::zero / beman::inside::one in comparison and arithmetic", "[inside][constant]")
{
  using B = inside<{0,200}>;

  // comparison (both orders — C++20 rewritten/reversed candidates)
  STATIC_REQUIRE( B{0} == zero );
  STATIC_REQUIRE( zero == B{0} );
  STATIC_REQUIRE( B{1} == one  );
  STATIC_REQUIRE( B{5} >  zero );
  STATIC_REQUIRE( zero <  B{5} );
  STATIC_REQUIRE( B{0} <  one  );

  // arithmetic (forwards through the ordinary inside operators)
  STATIC_REQUIRE( (B{5} + one)  == 6 );
  STATIC_REQUIRE( (one  + B{5}) == 6 );
  STATIC_REQUIRE( (B{5} - zero) == 5 );
  STATIC_REQUIRE( (B{5} - one)  == 4 );

  // a runtime check too, for good measure
  B b = 41;
  b = b + one;
  REQUIRE(b == 42);
}
