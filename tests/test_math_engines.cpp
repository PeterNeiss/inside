// Phase-3: explicit engine namespaces. `bnd::math::cordic::fn` (integer/CORDIC,
// always present) and `bnd::math::dbl::fn` (double engine, present unless
// BND_MATH_NO_FP) expose the same public-shaped API as `bnd::math::fn` and are
// callable SIDE-BY-SIDE in one binary. The unqualified name aliases the build's
// default engine. This TU instantiates both so the wrappers actually compile.

#include "bound/bound.hpp"
#include "bound/cmath.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bnd;
using namespace bnd::detail;

namespace
{
  // A double-backed real grid (works under both engines: `real` ⊃ snap; under
  // BND_MATH_FIXED it is an ordinary round_nearest integer-backed bound).
  using Ang = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
  using Pos = bound<{{1, 1000}, notch<1, 16384>}, round_nearest | real>;
  using Sq  = bound<{{0, 16}, notch<1, 16384>}, round_nearest | real>;   // sqrt needs Lower 0
}

TEST_CASE("cordic engine is always callable and exact on special values",
          "[cmath][engines][cordic]")
{
  REQUIRE(rational{math::cordic::sin(Ang{0})}   == 0);
  REQUIRE(rational{math::cordic::cos(Ang{0})}   == 1);
  REQUIRE(rational{math::cordic::atan(Ang{0})}  == 0);
  REQUIRE(rational{math::cordic::sinh(Ang{0})}  == 0);
  REQUIRE(rational{math::cordic::cosh(Ang{0})}  == 1);
  REQUIRE(rational{math::cordic::tanh(Ang{0})}  == 0);
  REQUIRE(rational{math::cordic::sqrt(Sq{4})}  == 2);
  REQUIRE(rational{math::cordic::cbrt(Ang{8})}  == 2);
  REQUIRE(rational{math::cordic::log10(Pos{100})} == 2);
  REQUIRE(rational{math::cordic::exp(Ang{0})}   == 1);

  // expected-returning ops
  auto p = math::cordic::pow(Pos{2}, Ang{4});
  REQUIRE(p.has_value());
  REQUIRE(rational{*p} == 16);

  // constexpr: the integer engine evaluates at compile time
  constexpr auto cs = math::cordic::sin(Ang{0});
  static_assert(rational{cs} == 0);
}

#ifndef BND_MATH_NO_FP
TEST_CASE("double engine is callable side-by-side and agrees on special values",
          "[cmath][engines][dbl]")
{
  REQUIRE(rational{math::dbl::sin(Ang{0})}    == 0);
  REQUIRE(rational{math::dbl::cos(Ang{0})}    == 1);
  REQUIRE(rational{math::dbl::atan(Ang{0})}   == 0);
  REQUIRE(rational{math::dbl::sinh(Ang{0})}   == 0);
  REQUIRE(rational{math::dbl::cosh(Ang{0})}   == 1);
  REQUIRE(rational{math::dbl::tanh(Ang{0})}   == 0);
  REQUIRE(rational{math::dbl::sqrt(Sq{4})}   == 2);
  REQUIRE(rational{math::dbl::cbrt(Ang{8})}   == 2);
  REQUIRE(rational{math::dbl::log10(Pos{100})} == 2);
  REQUIRE(rational{math::dbl::exp(Ang{0})}    == 1);

  auto p = math::dbl::pow(Pos{2}, Ang{4});
  REQUIRE(p.has_value());
  REQUIRE(rational{*p} == 16);
}

TEST_CASE("both engines coexist in one binary and meet at exact points",
          "[cmath][engines][mix]")
{
  // The defining property of Phase 3: both engines instantiated in the same TU.
  // On algebraically-exact inputs they land on the identical grid value.
  REQUIRE(rational{math::cordic::sqrt(Sq{4})} == rational{math::dbl::sqrt(Sq{4})});
  REQUIRE(rational{math::cordic::cos(Ang{0})}  == rational{math::dbl::cos(Ang{0})});

  // A pole still errors through the expected channel under both engines.
  using TanAng = bound<{{-2, 2}, notch<1, 4096>}, round_nearest | real>;
  auto tc = math::cordic::tan(TanAng{0});
  auto td = math::dbl::tan(TanAng{0});
  REQUIRE(tc.has_value());
  REQUIRE(td.has_value());
  REQUIRE(rational{*tc} == 0);
  REQUIRE(rational{*td} == 0);
}
#endif // !BND_MATH_NO_FP

TEST_CASE("unqualified name aliases the build's default engine",
          "[cmath][engines][default]")
{
  // bnd::math::sin must equal the selected engine bit-for-bit.
#ifdef BND_MATH_NO_FP
  REQUIRE(rational{math::sin(Ang{1})} == rational{math::cordic::sin(Ang{1})});
#else
  REQUIRE(rational{math::sin(Ang{1})} == rational{math::dbl::sin(Ang{1})});
#endif
}
