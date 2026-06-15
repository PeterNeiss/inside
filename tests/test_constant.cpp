#include "bound/bound.hpp"
#include "bound/detail/rational.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bnd;
using namespace bnd::detail;

//---------------------------------------------------------------------------
// Regression: a single-point source bound (e.g. `0_b`, any `just<N>`) must
// assign into ANY grid that can exactly represent the value — including grids
// with a non-unit notch, where the whole-range notch mapping is inexact. Before
// the fix, `bound_assignable`'s notch clause rejected these even though the one
// value lands on a notch. The integer path (`b = 0`) already worked, so the
// boundable path was simply inconsistent with it.
//---------------------------------------------------------------------------
TEST_CASE("point-bound assignment onto non-unit notch grids", "[bound][assign][notch]")
{
  // values {0,3,6,9}
  STATIC_REQUIRE([]{ bound<{{0,9},3}>  b = 0_b; return b == 0; }());
  STATIC_REQUIRE([]{ bound<{{0,9},3}>  b = 3_b; return b == 3; }());
  STATIC_REQUIRE([]{ bound<{{0,9},3}>  b = 9_b; return b == 9; }());
  // values {0,2,..,10}
  STATIC_REQUIRE([]{ bound<{{0,10},2}> b = 0_b; return b == 0; }());
  STATIC_REQUIRE([]{ bound<{{0,10},2}> b = 4_b; return b == 4; }());
  // offset grid (non-zero Lower), values {3,6,9}
  STATIC_REQUIRE([]{ bound<{{3,9},3}>  b = 6_b; return b == 6; }());

  // The boundable path now matches the integer path for the same grid.
  STATIC_REQUIRE([]{ bound<{{0,9},3}> b = 0;   return b == 0; }());
  STATIC_REQUIRE([]{ bound<{{0,9},3}> b = 0_b; return b == 0; }());
}

TEST_CASE("point-bound assignment rejected when not representable", "[bound][assign][notch]")
{
  // not on a notch
  STATIC_REQUIRE(!bound_assignable<bound<{{0,9},3}>,  decltype(1_b)>);
  STATIC_REQUIRE(!bound_assignable<bound<{{0,10},2}>, decltype(3_b)>);
  // out of range (valid grids: Lower divisible by notch)
  STATIC_REQUIRE(!bound_assignable<bound<{{3,9},3}>,  decltype(0_b)>);
  STATIC_REQUIRE(!bound_assignable<bound<{6,12}>,     decltype(0_b)>);

  // still admitted where representable
  STATIC_REQUIRE( bound_assignable<bound<{{0,9},3}>,  decltype(0_b)>);
  STATIC_REQUIRE( bound_assignable<bound<{{3,9},3}>,  decltype(6_b)>);
}

//---------------------------------------------------------------------------
// bnd::zero / bnd::one — universal exact constants.
//---------------------------------------------------------------------------
TEST_CASE("bnd::zero / bnd::one assign across storage kinds", "[bound][constant]")
{
  STATIC_REQUIRE([]{ bound<{0,200}>          b = zero; return b == 0; }());
  STATIC_REQUIRE([]{ bound<{0,200}>          b = one;  return b == 1; }());
  STATIC_REQUIRE([]{ bound<{-40,60}>         b = zero; return b == 0; }());  // signed direct
  STATIC_REQUIRE([]{ bound<{-40,60}>         b = one;  return b == 1; }());
  STATIC_REQUIRE([]{ bound<{{0,9},3}>        b = zero; return b == 0; }());  // non-unit notch
  // Q8.8: value 1 is raw 256.
  STATIC_REQUIRE([]{ bound<{{0,1},0x1p-8_r}> b = one;  return static_cast<imax>(b.raw()) == 256; }());

  // assignment operator (not just construction)
  STATIC_REQUIRE([]{ bound<{0,200}> b = 5; b = zero; return b == 0; }());
  STATIC_REQUIRE([]{ bound<{0,200}> b = 5; b = one;  return b == 1; }());
}

TEST_CASE("bnd::zero / bnd::one rejected where not representable", "[bound][constant]")
{
  STATIC_REQUIRE(!bound_assignable<bound<{5,10}>,    decltype(zero)>);  // 0 out of range
  STATIC_REQUIRE(!bound_assignable<bound<{{0,9},3}>, decltype(one)>);   // 1 not on notch
}

TEST_CASE("bnd::zero / bnd::one in comparison and arithmetic", "[bound][constant]")
{
  using B = bound<{0,200}>;

  // comparison (both orders — C++20 rewritten/reversed candidates)
  STATIC_REQUIRE( B{0} == zero );
  STATIC_REQUIRE( zero == B{0} );
  STATIC_REQUIRE( B{1} == one  );
  STATIC_REQUIRE( B{5} >  zero );
  STATIC_REQUIRE( zero <  B{5} );
  STATIC_REQUIRE( B{0} <  one  );

  // arithmetic (forwards through the ordinary bound operators)
  STATIC_REQUIRE( (B{5} + one)  == 6 );
  STATIC_REQUIRE( (one  + B{5}) == 6 );
  STATIC_REQUIRE( (B{5} - zero) == 5 );
  STATIC_REQUIRE( (B{5} - one)  == 4 );

  // a runtime check too, for good measure
  B b = 41;
  b = b + one;
  REQUIRE(b == 42);
}
