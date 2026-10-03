// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/numeric_limits.hpp>

#include <gtest/gtest.h>

#include <type_traits>

using namespace beman::inside;

// Dependent context: a non-dependent invalid requirement inside `requires{}`
// would be a hard error, so probe through a variable template.
template <typename Lhs, typename Rhs>
inline constexpr bool has_common_type = requires { typename std::common_type_t<Lhs, Rhs>; };
template <typename Lhs, typename Rhs>
inline constexpr bool has_common_inside = requires { typename common_inside_t<Lhs, Rhs>; };

//---------------------------------------------------------------------------
// grid hull
//---------------------------------------------------------------------------
// hull is the interval hull with the notch gcd
TEST(CommonTypeTest, hull_is_the_interval_hull_with_the_notch_gcd)
{
  constexpr grid percent{0, 100};                          // notch 1
  constexpr grid halves{interval{-50, 50}, notch<1, 2>};

  constexpr auto hulled = hull(percent, halves);
  static_assert(hulled.has_value());
  static_assert(hulled->Interval.Lower == detail::rational{-50});
  static_assert(hulled->Interval.Upper == detail::rational{100});
  static_assert(hulled->Notch == (notch<1, 2>));

  // Coprime denominators combine to the lcm.
  constexpr grid half_steps{interval{0, 1}, notch<1, 2>};
  constexpr grid third_steps{interval{0, 1}, notch<1, 3>};
  static_assert(hull(half_steps, third_steps)->Notch == (notch<1, 6>));

  // hull(G, G) == G.
  static_assert(hull(percent, percent)->Interval.Lower == percent.Interval.Lower);
  static_assert(hull(percent, percent)->Interval.Upper == percent.Interval.Upper);
  static_assert(hull(percent, percent)->Notch == percent.Notch);

  // A continuous operand makes the hull continuous.
  constexpr grid continuous{interval{-1, 1}, detail::rational{0}};
  static_assert(hull(percent, continuous)->Notch == detail::rational{0});
  static_assert(hull(percent, continuous)->Interval.Upper == detail::rational{100});
}

// hull result is a valid grid by construction
TEST(CommonTypeTest, hull_result_is_a_valid_grid_by_construction)
{
  // Anchored lattices: both Lowers are notch multiples, so the hull is too.
  constexpr grid quarter_steps{interval{-8, 8}, notch<1, 4>};
  constexpr grid unit_steps{interval{-3, 7}, detail::rational{1}};
  constexpr auto hulled = hull(quarter_steps, unit_steps);
  static_assert(hulled.has_value());
  static_assert(grid::try_make(hulled->Interval, hulled->Notch).has_value());
}

//---------------------------------------------------------------------------
// std::common_type / common_inside_t
//---------------------------------------------------------------------------
// common_type of an inside with itself is the inside, policy included
TEST(CommonTypeTest, common_type_of_an_inside_with_itself_is_the_inside_policy_included)
{
  using percent = inside<{0, 100}>;
  static_assert(std::same_as<std::common_type_t<percent, percent>, percent>);

  using clamped_percent = inside<{0, 100}, clamp>;
  static_assert(std::same_as<std::common_type_t<clamped_percent, clamped_percent>, clamped_percent>);
}

// common_type of mixed grids is the hull type
TEST(CommonTypeTest, common_type_of_mixed_grids_is_the_hull_type)
{
  using percent = inside<{0, 100}>;                    // notch 1
  using halves  = inside<{{-50, 50}, notch<1, 2>}>;
  using common  = std::common_type_t<percent, halves>;

  static_assert(Lower<common> == detail::rational{-50});
  static_assert(Upper<common> == detail::rational{100});
  static_assert(Notch<common> == (notch<1, 2>));
  static_assert(std::same_as<common, common_inside_t<percent, halves>>);
  static_assert(std::same_as<common, std::common_type_t<halves, percent>>);

  // Both operand types convert into the hull losslessly.
  constexpr common from_percent{percent{42}};
  constexpr common from_halves{halves{-0.5_ins}};
  static_assert(from_percent == common{42});
  static_assert(from_halves == common{-0.5_ins});
}

// common_type SFINAEs away when the hull notch is unrepresentable
TEST(CommonTypeTest, common_type_sfinaes_away_when_the_hull_notch_is_unrepresentable)
{
  // Coprime ~2^32 denominators: the notch gcd's lcm denominator exceeds imax.
  using prime_notch_a = inside<{{0, 1}, notch<1, 4294967291>}>;
  using prime_notch_b = inside<{{0, 1}, notch<1, 4294967311>}>;
  static_assert(!has_common_type<prime_notch_a, prime_notch_b>);
  static_assert(!has_common_inside<prime_notch_a, prime_notch_b>);
}

//---------------------------------------------------------------------------
// mixed-grid min / max
//---------------------------------------------------------------------------
// mixed-grid min/max return the hull type
TEST(CommonTypeTest, mixed_grid_min_max_return_the_hull_type)
{
  using percent = inside<{0, 100}>;
  using halves  = inside<{{-50, 50}, notch<1, 2>}>;
  using common  = common_inside_t<percent, halves>;

  constexpr percent three{3};
  constexpr halves  minus_half{-0.5_ins};

  constexpr auto lo = beman::inside::min(three, minus_half);
  constexpr auto hi = beman::inside::max(three, minus_half);
  static_assert(std::same_as<decltype(lo), const common>);
  static_assert(lo == common{-0.5_ins});
  static_assert(hi == common{3});

  // Same-type overload still returns the operand type.
  constexpr auto same = beman::inside::min(percent{7}, percent{5});
  static_assert(std::same_as<decltype(same), const percent>);
  static_assert(same == percent{5});
}
