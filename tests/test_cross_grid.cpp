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

#include <limits>
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

//---------------------------------------------------------------------------
// 2026-07 Tier-3 fast paths: pin that the integer folds stay engaged for the
// shapes they were built for — and stay OUT of the fp-raw shapes (a double
// raw has no integer offset; test_real_exact caught exactly that during
// development).
//---------------------------------------------------------------------------
TEST_CASE("tier-3 integer fast paths stay engaged (and fp stays excluded)",
          "[cross][arith][perf-paths]")
{
  using whole    = bound<{0, 100}>;
  using quarters = bound<{{0, 1}, notch<1, 4>}>;
  STATIC_REQUIRE(detail::addition<whole, quarters>::mixed_offset_ok);

  using tenths       = bound<{{0, 100}, notch<1, 10>}>;
  using quarter_grid = bound<{{0, 100}, notch<1, 4>}, round_nearest>;
  STATIC_REQUIRE(detail::assignment<quarter_grid, tenths>::affine_map.ok);

  // fp-backed operands must not take the integer offset path.
  using coarse_real = bound<{{0, (umax{1} << 40)}, notch<1, 2>}, real>;
  using fine_real   = bound<{{0, 1}, notch<1, (1u << 20)>}, real>;
  if constexpr (detail::fp_raw<coarse_real>)
    STATIC_REQUIRE_FALSE(detail::addition<coarse_real, fine_real>::mixed_offset_ok);

  // value check across a negative Lower, at compile time (constexpr path).
  using signed_whole = bound<{-50, 50}>;
  using eighths      = bound<{{-2, 2}, notch<1, 8>}>;
  STATIC_REQUIRE(rational{signed_whole{-7} + eighths{-0.625_b}}
                 == rational{umax{61}, imax{-8}});
}

TEST_CASE("regression: cross-grid assign onto rational storage keeps the value",
          "[cross][assign][regression][exact]")
{
  // The boundable-rhs store used the notch-index machinery for rational-raw
  // targets: an index-raw source had its VALUE rounded to a whole number
  // (7/3 -> 2/1) and a rational-raw source had the grid transform applied to
  // a raw that already was the value (5/3 -> corrupted). Both must store the
  // exact source value.
  using exact_t = bound<{{0, 4}, notch<1, 3>}, exact | round_nearest>;

  using index_src = bound<{{0, 4}, notch<1, 3>}, round_nearest>;
  exact_t from_index;
  from_index = index_src{rational{7, 3}};
  REQUIRE(from_index.raw() == rational{7, 3});

  using exact_wide = bound<{{-4, 4}, notch<1, 3>}, exact | round_nearest>;
  exact_t from_exact;
  from_exact = exact_wide{rational{5, 3}};
  REQUIRE(from_exact.raw() == rational{5, 3});

  using value_src = bound<{0, 4}, snap>;
  exact_t from_value_raw;
  from_value_raw = value_src{3};
  REQUIRE(from_value_raw.raw() == rational{3});

  using real_src = bound<{{0, 4}, notch<1, 256>}, real | round_nearest>;
  using exact_dyadic = bound<{{0, 4}, notch<1, 256>}, exact | round_nearest>;
  exact_dyadic from_real;
  from_real = real_src{rational{513, 256}};
  REQUIRE(from_real.raw() == rational{513, 256});

  // rounding still happens when the source is off the target grid
  using exact_coarse = bound<{{0, 4}, 1}, exact | round_nearest>;
  exact_coarse rounded;
  rounded = index_src{rational{7, 3}};     // 2.33 -> 2 on the unit grid
  REQUIRE(rounded.raw() == rational{2});
}

TEST_CASE("scalar comparison integer arm agrees with the rational decode",
          "[cross][compare][perf-paths]")
{
  // Q-format (index raw) vs integral scalar takes the cross-multiplied
  // integer arm; every verdict must match the exact rational comparison,
  // including at A's numeric_limits extremes.
  using q88 = bound<{{0, 255}, notch<1, 256>}, round_nearest>;
  STATIC_REQUIRE(scalar_index_cmp_fits<q88, int>);

  auto agree = [](auto probe, auto scalar) {
    rational exact_lhs = as_rational(probe);
    rational exact_rhs{scalar};
    REQUIRE((probe == scalar) == (exact_lhs == exact_rhs));
    REQUIRE((probe <  scalar) == (exact_lhs <  exact_rhs));
    REQUIRE((probe >  scalar) == (exact_lhs >  exact_rhs));
  };
  for (int scalar : {std::numeric_limits<int>::min(), -1, 0, 41, 42, 43, 255,
                     std::numeric_limits<int>::max()})
  {
    agree(q88{42}, scalar);
    agree(q88{rational{10753, 256}}, scalar);   // 42 + 1/256: != 42, > 42
    agree(q88{rational{10751, 256}}, scalar);   // 42 - 1/256: != 42, < 42
    agree(q88{0}, scalar);
    agree(q88{255}, scalar);
  }

  // offset index grid (negative Lower -> nonzero bias)
  using offset_q = bound<{{-8, 8}, notch<1, 16384>}, round_nearest>;
  STATIC_REQUIRE(scalar_index_cmp_fits<offset_q, int>);
  for (int scalar : {std::numeric_limits<int>::min(), -9, -8, -1, 0, 1, 8,
                     std::numeric_limits<int>::max()})
  {
    agree(offset_q{-8}, scalar);
    agree(offset_q{rational{-1, 16384}}, scalar);
    agree(offset_q{0}, scalar);
    agree(offset_q{8}, scalar);
  }

  // 64-bit scalars whose cross term c·d can overflow imax are excluded and
  // fall back to the exact rational path — still correct.
  STATIC_REQUIRE(!scalar_index_cmp_fits<q88, long long>);
  REQUIRE(q88{42} < std::numeric_limits<long long>::max());
  REQUIRE(q88{42} > std::numeric_limits<long long>::min());
  REQUIRE(!(q88{42} == std::numeric_limits<long long>::max()));
}
