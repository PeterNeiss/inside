// Cross-grid comparison and arithmetic against an exact rational oracle.
//
// The fuzzer covers same-grid comparison and same-notch cross-grid addition;
// this fills the gap: comparing and combining bounds whose grids differ in
// Lower, Upper AND notch. Reconciling two different (Lower, Notch) encodings is
// where off-by-one / sign / scaling bugs hide. The oracle is exact rational
// arithmetic: each bound decodes to a rational, and the bound-level result must
// equal the rational-level result exactly (+, −, × are lossless on the widened
// result grid; comparison is order-exact).

#include "bound/bound.hpp"
#include "bound/io.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace bnd;
using namespace bnd::detail;

namespace
{
  // Values on a 1/4 grid so they land exactly on the coarsest notch used below
  // (halves and quarters); each `A`/`B` snaps losslessly, keeping the oracle exact.
  std::vector<double> sweep(double lo, double hi, double step)
  {
    std::vector<double> v;
    for (double x = lo; x <= hi + 1e-9; x += step) v.push_back(x);
    return v;
  }

  template <class A, class B>
  void check_cross(const std::vector<double>& va, const std::vector<double>& vb)
  {
    for (double xa : va)
      for (double xb : vb)
      {
        A a = xa;
        B b = xb;
        const rational ar = static_cast<rational>(a);
        const rational br = static_cast<rational>(b);

        // --- comparison: bound-level must match rational-level order ---
        if constexpr (requires { a <=> b; })
        {
          auto got = (a <=> b);
          INFO("a=" << to_string(ar) << " b=" << to_string(br));
          REQUIRE((got < 0)  == (ar <  br));
          REQUIRE((got > 0)  == (ar >  br));
          REQUIRE((got == 0) == (ar == br));
        }
        if constexpr (requires { a == b; })
        {
          INFO("== a=" << to_string(ar) << " b=" << to_string(br));
          REQUIRE((a == b) == (ar == br));
          REQUIRE((a != b) == (ar != br));
        }
        if constexpr (requires { a < b; })
        {
          INFO("< a=" << to_string(ar) << " b=" << to_string(br));
          REQUIRE((a <  b) == (ar <  br));
          REQUIRE((a <= b) == (ar <= br));
          REQUIRE((a >  b) == (ar >  br));
          REQUIRE((a >= b) == (ar >= br));
        }

        // --- arithmetic: exact on the widened result grid ---
        if constexpr (requires { a + b; })
        {
          INFO("+ a=" << to_string(ar) << " b=" << to_string(br));
          REQUIRE(static_cast<rational>(a + b) == *(ar + br));
        }
        if constexpr (requires { a - b; })
        {
          INFO("- a=" << to_string(ar) << " b=" << to_string(br));
          REQUIRE(static_cast<rational>(a - b) == *(ar - br));
        }
        if constexpr (requires { a * b; })
        {
          INFO("* a=" << to_string(ar) << " b=" << to_string(br));
          REQUIRE(static_cast<rational>(a * b) == *(ar * br));
        }
      }
  }
}

TEST_CASE("cross-grid compare/arith: differing notches (1/2 vs 1/4)", "[cross][compare][arith]")
{
  using A = bound<{{-8, 8}, notch<1, 2>}>;
  using B = bound<{{-8, 8}, notch<1, 4>}>;
  check_cross<A, B>(sweep(-8, 8, 0.5), sweep(-8, 8, 0.25));
}

TEST_CASE("cross-grid compare/arith: integer vs half-notch, different offset", "[cross][compare][arith]")
{
  using A = bound<{0, 100}>;                 // notch 1, Lower 0
  using B = bound<{{-50, 50}, 0.5}>;         // notch 1/2, Lower -50
  check_cross<A, B>(sweep(0, 100, 1.0), sweep(-50, 50, 0.5));
}

TEST_CASE("cross-grid compare/arith: quarter-notch vs unit signed", "[cross][compare][arith]")
{
  using A = bound<{{-8, 8}, notch<1, 4>}>;
  using B = bound<{-3, 7}>;
  check_cross<A, B>(sweep(-8, 8, 0.25), sweep(-3, 7, 1.0));
}

TEST_CASE("cross-grid compare/arith: Q8.8 vs Q1.14 (power-of-two notches)", "[cross][compare][arith]")
{
  using A = bound<{{0, 255}, notch<1, 256>}>;
  using B = bound<{{-1, 1}, notch<1, 16384>}>;
  // Values chosen on both grids' common refinement (1/256) so the oracle stays exact.
  check_cross<A, B>(sweep(0, 4, 1.0 / 256 * 37), sweep(-1, 1, 1.0 / 256 * 5));
}

TEST_CASE("cross-grid compare/arith: asymmetric offsets, same notch", "[cross][compare][arith]")
{
  using A = bound<{{-7, 11}, 0.25}>;
  using B = bound<{{3, 30}, 0.25}>;
  check_cross<A, B>(sweep(-7, 11, 0.5), sweep(3, 30, 0.75));
}

// Explicit regression for the fractional-operand truncation bug: adding a
// fractional notch-offset bound to a direct-storage (integer) bound used to
// drop the fractional part (-7.75 + (-3) silently became -10, not -10.75)
// because the dispatch chose the integer `to_value` path. The integer path is
// now gated on IsIntegerAligned of BOTH operands (matching multiplication).
TEST_CASE("regression: fractional + integer-direct keeps the fraction", "[cross][arith][regression]")
{
  using Frac = bound<{{-8, 8}, notch<1, 4>}>;   // fractional, notch-offset storage
  using Int  = bound<{-3, 7}>;                    // integer, direct storage

  REQUIRE(static_cast<rational>(Frac{-7.75} + Int{-3}) == rational{43, -4});  // -10.75
  REQUIRE(static_cast<rational>(Frac{0.25}  + Int{2})  == rational{9u, 4});   //   2.25
  REQUIRE(static_cast<rational>(Frac{5.25}  + Int{7})  == rational{49u, 4});  //  12.25
  // subtraction routes through add(-rhs); same path.
  REQUIRE(static_cast<rational>(Frac{5.25}  - Int{3})  == rational{9u, 4});   //   2.25
  REQUIRE(static_cast<rational>(Int{4} - Frac{1.5})    == rational{5u, 2});   //   2.5
}
