// Phase-1: transcendentals are gated on `snap` (rounding permission), not `real`
// (double storage). This exercises the new capability — `bnd::math` on NON-`real`
// snap grids: integer-index storage and non-dyadic (1/100) grids — using exact
// special values that are bit-exact on both engines and any grid containing them.

#include "bound/bound.hpp"
#include "bound/cmath.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bnd;
using namespace bnd::detail;

TEST_CASE("snap-gated transcendentals on non-real grids (integer & 1/100)",
          "[cmath][snap][nonreal]")
{
  // round_nearest implies snap but NOT real → these are integer/index-stored,
  // not double-backed. Pre-Phase-1 these were a hard `require_real` compile error.
  using Ang = bound<{{-8, 8}, notch<1, 16384>}, round_nearest>;     // integer-index storage
  REQUIRE(rational{math::sin(Ang{0})}  == 0);
  REQUIRE(rational{math::cos(Ang{0})}  == 1);
  REQUIRE(rational{math::atan(Ang{0})} == 0);

  using Sq = bound<{{0, 16}, notch<1, 100>}, round_nearest>;        // non-dyadic 1/100 grid
  REQUIRE(rational{math::sqrt(Sq{0})} == 0);
  REQUIRE(rational{math::sqrt(Sq{4})} == 2);

  using Lg = bound<{{1, 1000}, notch<1, 100>}, round_nearest>;
  REQUIRE(rational{math::log10(Lg{1})}   == 0);
  REQUIRE(rational{math::log10(Lg{100})} == 2);

  using Cb = bound<{{-8, 8}, notch<1, 100>}, round_nearest>;
  REQUIRE(rational{math::cbrt(Cb{0})}  ==  0);
  REQUIRE(rational{math::cbrt(Cb{8})}  ==  2);
  REQUIRE(rational{math::cbrt(Cb{-8})} == -2);

  using Hy = bound<{{-10, 10}, notch<1, 100>}, round_nearest>;
  REQUIRE(rational{math::sinh(Hy{0})} == 0);
  REQUIRE(rational{math::cosh(Hy{0})} == 1);
  REQUIRE(rational{math::tanh(Hy{0})} == 0);

  using Ex = bound<{{-4, 4}, notch<1, 100>}, round_nearest>;
  REQUIRE(rational{math::exp(Ex{0})} == 1);

  // pow returns expected; 2^4 snaps exactly onto the 1/100 grid.
  using B = bound<{{1, 16}, notch<1, 100>}, round_nearest>;
  using E = bound<{{-4, 8}, notch<1, 100>}, round_nearest>;
  auto p = math::pow(B{2}, E{4});
  REQUIRE(p.has_value());
  REQUIRE(rational{*p} == 16);
}
