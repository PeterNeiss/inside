// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::detail;

// grid structured binding
TEST(GridTest, grid_structured_binding)
{

  grid g{{0, 100}, 2};
  auto [iv, notch] = g;
  ASSERT_EQ(iv.Lower, 0);
  ASSERT_EQ(iv.Upper, 100);
  ASSERT_EQ(notch, 2);
}

// grid construction and max_index
TEST(GridTest, grid_construction_and_max_notch)
{
  static_assert(grid{{0, 100}, 1}.max_index() == 100);
  static_assert(grid{{1, 5},   0.25}.max_index() == 16);
  static_assert(grid{{0, std::numeric_limits<umax>::max()}, 1}.max_index()
                 == std::numeric_limits<umax>::max());
}

// just<>/inside values work as grid corners
TEST(GridTest, just_inside_values_work_as_grid_corners)
{
  // Corners are `rational`, so an inside converts via its implicit operator rational().
  static_assert(grid{0, just<30>}            == grid{0, 30});
  static_assert(grid{just<5>, just<10>}      == grid{5, 10});
  static_assert(grid{just<2>}                == grid{2});       // 1-arg point grid
  static_assert(grid{just<0>, just<8>, just<2>} == grid{0, 8, 2});
  // ...and as inside<> grid-spec corners (the rifle.cpp use case).
  static_assert(grid_of<inside<{0, just<30>}, wrap>> == grid_of<inside<{0, 30}, wrap>>);
}

// grid storage_min selection
TEST(GridTest, grid_storage_min_selection)
{
  // Notch 0 -> rational
  static_assert(std::is_same_v<storage_min_t<grid{{0,10}, 0}>, rational>);

  // Notch 1, lower 0 -> smallest unsigned that fits Upper
  static_assert(std::is_same_v<storage_min_t<grid{0, 100,  1}>, std::uint8_t>);
  static_assert(std::is_same_v<storage_min_t<grid{0, 1000, 1}>, std::uint16_t>);
  static_assert(std::is_same_v<storage_min_t<grid{0, 100000, 1}>, std::uint32_t>);

  // Signed integer case (notch 1, negative lower)
  static_assert(std::is_same_v<storage_min_t<grid{-127, 127, 1}>,    std::int8_t>);
  static_assert(std::is_same_v<storage_min_t<grid{-32000, 32000, 1}>, std::int16_t>);
}

// grid arithmetic
TEST(GridTest, grid_arithmetic)
{
  {
    SCOPED_TRACE("add aligns notches via gcd");
    grid a{{0, 10}, 1};
    grid b{{0,  5}, 1};
    auto r = a + b;
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->Interval.Lower == 0);
    ASSERT_TRUE(r->Interval.Upper == 15);
    ASSERT_TRUE(r->Notch == 1);
  }

  {
    SCOPED_TRACE("multiply takes bounding box and lcm-ish notch");
    grid a{{0, 10}, 1};
    grid b{{0,  5}, 1};
    auto r = a * b;
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->Interval.Lower == 0);
    ASSERT_TRUE(r->Interval.Upper == 50);
  }

  {
    SCOPED_TRACE("divide by zero-only interval yields division_by_zero");
    grid a{{0, 10}, 1};
    grid zero{{0, 0}, 0};         // pure zero-point grid
    auto r = a / zero;
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error(), errc::division_by_zero);
  }

  {
    SCOPED_TRACE("divide by positive interval excluding zero");
    grid a{{0, 10}, 1};
    grid b{{2,  5}, 1};
    auto r = a / b;
    ASSERT_TRUE(r.has_value());
  }

  {
    SCOPED_TRACE("divide by interval [0, U] excludes zero from divisor by stepping in by notch");
    grid a{{0, 10}, 1};
    grid b{{0, 5}, 1};   // includes zero, positive upper
    auto r = a / b;
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->Interval.Lower == 0);
    ASSERT_TRUE(r->Interval.Upper == 10);    // 10 / 1 = 10
    ASSERT_TRUE(r->Notch == 0);
  }

  {
    SCOPED_TRACE("divide by interval [L, 0] (negative side only)");
    grid a{{0, 10}, 1};
    grid b{{-5, 0}, 1};   // includes zero, negative lower
    auto r = a / b;
    ASSERT_TRUE(r.has_value());
    // dividing positives by negatives gives non-positive result
    ASSERT_TRUE(r->Interval.Upper <= 0);
    ASSERT_TRUE(r->Notch == 0);
  }

  {
    SCOPED_TRACE("divide by interval that straddles zero");
    grid a{{1, 10}, 1};
    grid b{{-5, 5}, 1};   // straddles zero
    auto r = a / b;
    ASSERT_TRUE(r.has_value());
    // result must span both signs once we exclude zero
    ASSERT_TRUE(r->Interval.Lower < 0);
    ASSERT_TRUE(r->Interval.Upper > 0);
    ASSERT_TRUE(r->Notch == 0);
  }

  {
    SCOPED_TRACE("divide by zero-notch interval that straddles zero");
    grid a{{1, 10}, 1};
    grid b{{-5, 5}, 0};   // notch=0, so step defaults to 1
    auto r = a / b;
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->Interval.Lower < 0);
    ASSERT_TRUE(r->Interval.Upper > 0);
  }
}

// grid validate
TEST(GridTest, grid_validate)
{
  static_assert(grid::validate<grid{{0, 10},  1}>());
  static_assert(grid::validate<grid{{0, 10},  rational{1u, 2}}>());
  static_assert(grid::validate<grid{0_r}>());                   // point grid, notch=0
}


// grid{lo, hi} derives the notch as gcd(1, Lower, Upper): the coarsest step 1/k
// keeping every integer and both limits on the lattice.
TEST(GridTest, two_limit_ctor_derives_the_notch)
{
  static_assert(per<16> == rational{1, 16});                       // per<D> is the step 1/D
  static_assert(frac<-6, 5> == rational{-6, 5});
  static_assert(grid{0, 100}.Notch == 1);                          // integers: unchanged
  static_assert(grid{-40, 85}.Notch == 1);
  static_assert(grid{0.0, 1.0}.Notch == 1);
  static_assert(grid{0.5, 10}.Notch == rational{1, 2});
  static_assert(grid{-2.5, 0.75}.Notch == rational{1, 4});
  static_assert(grid{frac<-6, 5>, frac<3, 5>}.Notch == rational{1, 5});
  static_assert(grid{0.1_r, 1}.Notch == rational{1, 10});
  static_assert(grid{0, 0x1p-10}.Notch == rational{1, 1024});      // at the double bound
  static_assert(grid{0, 0x1p-11_r}.Notch == rational{1, 2048});    // exact spelling: no bound
#if BEMAN_INSIDE_BIG_GRIDS
  // Big grid numbers: the exact notch 1/(2^62·(2^62−1)), past 64 bits.
  static_assert(grid{frac<1, (1ll << 62)>, frac<1, (1ll << 62) - 1>}.Notch
                == detail::grid_rational{detail::big_int{1},
                                         detail::big_int{1ll << 62} * detail::big_int{(1ll << 62) - 1}});
#else
  static_assert(grid{frac<1, (1ll << 62)>, frac<1, (1ll << 62) - 1>}.Notch == 0);   // no rational notch
#endif
  static_assert(grid::validate<grid{frac<-6, 5>, frac<3, 5>}>());

  using half = inside<{0.5, 10}>;
  static_assert(std::is_same_v<half::raw_type, std::uint8_t>);
  static_assert(max_index_v<half> == 19);
  EXPECT_EQ(rational{half{2.5}}, (rational{5, 2}));
}

namespace
{
  // The derived notch 1/k must keep 1, Lower and Upper on the lattice, and be the
  // coarsest such step. A coarser 1/m (m a proper divisor of k) works only if
  // 1/(k/p) works for some prime p | k, so checking those suffices.
  template <grid G>
  constexpr bool derived_notch_is_coarsest()
  {
    const rational n = G.Notch;
    if (n.Numerator != 1) return false;                       // always 1/k
    auto on = [](rational v, rational step) { return divides_evenly(v, step); };
    if (!on(1, n) || !on(G.Interval.Lower, n) || !on(G.Interval.Upper, n))
      return false;
    const umax k = abs_den(n.Denominator);
    umax rest = k;
    for (umax p = 2; p * p <= rest || rest > 1; ++p)
    {
      if (p * p > rest) p = rest;                             // rest is prime
      if (rest % p != 0) continue;
      while (rest % p == 0) rest /= p;
      const rational coarser{umax{1}, static_cast<imax>(k / p)};
      if (on(G.Interval.Lower, coarser) && on(G.Interval.Upper, coarser))
        return false;                                         // a coarser step works
    }
    return true;
  }

  template <grid G>
  constexpr bool slot_count_matches()
  {
    const rational span = (detail::to_rational(G.Interval.Upper) - detail::to_rational(G.Interval.Lower)).value();
    return rational{G.max_index()} == (span / detail::to_rational(G.Notch)).value();
  }
}

// grid{lo, hi} with fractional limits: the notch is 1/lcm of the limits'
// denominators (after reduction), the grid validates, and inside works on it.
TEST(GridTest, derived_notch_of_fraction_pairs)
{
  // {13/16, 3/10} in valid order: lcm(16, 10) = 80 → slots 24/80 .. 65/80.
  constexpr grid a{frac<3, 10>, frac<13, 16>};
  static_assert(a.Notch == rational{1, 80});
  static_assert(a.max_index() == 41);
  static_assert(grid::validate<a>() && derived_notch_is_coarsest<a>() && slot_count_matches<a>());

  // Reversed order is not an interval: the notch still derives, the grid is invalid.
  static_assert(grid{frac<13, 16>, frac<3, 10>}.Notch == rational{1, 80});
  static_assert(!grid::try_make(interval{frac<13, 16>, frac<3, 10>}, rational{1, 80}).has_value());

  // Shared factors reduce first: 6/8 is 3/4, 10/12 is 5/6 → lcm(4, 6) = 12.
  constexpr grid b{frac<6, 8>, frac<10, 12>};
  static_assert(b.Notch == rational{1, 12});
  static_assert(derived_notch_is_coarsest<b>() && slot_count_matches<b>());

  // Mixed signs, coprime denominators: lcm(7, 9) = 63.
  constexpr grid c{frac<-5, 7>, frac<4, 9>};
  static_assert(c.Notch == rational{1, 63});
  static_assert(c.max_index() == 73);                             // (4/9 + 5/7)·63 = 28 + 45
  static_assert(grid::validate<c>() && derived_notch_is_coarsest<c>() && slot_count_matches<c>());

  // Both negative.
  constexpr grid d{frac<-13, 16>, frac<-3, 10>};
  static_assert(d.Notch == rational{1, 80});
  static_assert(derived_notch_is_coarsest<d>() && slot_count_matches<d>());

  // Integer and fraction: the integer contributes nothing finer.
  constexpr grid e{-3, frac<7, 3>};
  static_assert(e.Notch == rational{1, 3});
  static_assert(e.max_index() == 16);
  static_assert(derived_notch_is_coarsest<e>() && slot_count_matches<e>());

  // One denominator divides the other: lcm(4, 12) = 12.
  constexpr grid f{frac<1, 4>, frac<11, 12>};
  static_assert(f.Notch == rational{1, 12});
  static_assert(derived_notch_is_coarsest<f>() && slot_count_matches<f>());

  // A whole number written as a fraction stays an integer grid.
  constexpr grid g{frac<6, 3>, frac<20, 4>};
  static_assert(g.Notch == 1);
  static_assert(g.Interval.Lower == 2 && g.Interval.Upper == 5);

  // Equal limits: a point; the two-limit form still derives a valid notch.
  constexpr grid h{frac<3, 10>, frac<3, 10>};
  static_assert(h.Notch == rational{1, 10});
  static_assert(grid::validate<h>() && h.max_index() == 0);

  // Large coprime denominators that still fit: lcm(1009, 1013) = 1022117.
  constexpr grid i{frac<1, 1013>, frac<1, 1009>};
  static_assert(i.Notch == rational{1, 1022117});
  static_assert(i.max_index() == 4);                              // 1013 − 1009
  static_assert(grid::validate<i>() && derived_notch_is_coarsest<i>() && slot_count_matches<i>());

  // inside on derived fractional grids: storage, exact values, assignment.
  using eightieths = inside<{frac<3, 10>, frac<13, 16>}>;
  static_assert(std::is_same_v<eightieths::raw_type, std::uint8_t>);
  EXPECT_EQ(rational{eightieths{0.5}}, (rational{1, 2}));         // 40/80
  EXPECT_EQ((rational{eightieths{frac<13, 16>}}), (rational{13, 16}));
  EXPECT_EQ(eightieths::try_make(frac<1, 3>).error(), errc::rounding_error);   // 1/3 is not k/80

  using signed_63 = inside<{frac<-5, 7>, frac<4, 9>}, round_nearest>;
  EXPECT_EQ((rational{signed_63{frac<-5, 7>}}), (rational{-5, 7}));
  EXPECT_EQ(rational{signed_63{0.0}}, rational{0});
  EXPECT_EQ((rational{signed_63{frac<1, 4>}}), (rational{16, 63}));   // 1/4 = 15.75/63 → nearest 16/63
}
