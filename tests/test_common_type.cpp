#include "bound/bound.hpp"
#include "bound/numeric_limits.hpp"

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

using namespace bnd;

// Dependent context: a non-dependent invalid requirement inside `requires{}`
// would be a hard error, so probe through a variable template.
template <typename Lhs, typename Rhs>
inline constexpr bool has_common_type = requires { typename std::common_type_t<Lhs, Rhs>; };
template <typename Lhs, typename Rhs>
inline constexpr bool has_common_bound = requires { typename common_bound_t<Lhs, Rhs>; };

//---------------------------------------------------------------------------
// grid hull
//---------------------------------------------------------------------------
TEST_CASE("hull is the interval hull with the notch gcd", "[grid][hull]")
{
  constexpr grid percent{0, 100};                          // notch 1
  constexpr grid halves{interval{-50, 50}, notch<1, 2>};

  constexpr auto hulled = hull(percent, halves);
  STATIC_REQUIRE(hulled.has_value());
  STATIC_REQUIRE(hulled->Interval.Lower == detail::rational{-50});
  STATIC_REQUIRE(hulled->Interval.Upper == detail::rational{100});
  STATIC_REQUIRE(hulled->Notch == (notch<1, 2>));

  // Coprime denominators combine to the lcm.
  constexpr grid half_steps{interval{0, 1}, notch<1, 2>};
  constexpr grid third_steps{interval{0, 1}, notch<1, 3>};
  STATIC_REQUIRE(hull(half_steps, third_steps)->Notch == (notch<1, 6>));

  // hull(G, G) == G.
  STATIC_REQUIRE(hull(percent, percent)->Interval.Lower == percent.Interval.Lower);
  STATIC_REQUIRE(hull(percent, percent)->Interval.Upper == percent.Interval.Upper);
  STATIC_REQUIRE(hull(percent, percent)->Notch == percent.Notch);

  // A continuous operand makes the hull continuous.
  constexpr grid continuous{interval{-1, 1}, detail::rational{0}};
  STATIC_REQUIRE(hull(percent, continuous)->Notch == detail::rational{0});
  STATIC_REQUIRE(hull(percent, continuous)->Interval.Upper == detail::rational{100});
}

TEST_CASE("hull result is a valid grid by construction", "[grid][hull]")
{
  // Anchored lattices: both Lowers are notch multiples, so the hull is too.
  constexpr grid quarter_steps{interval{-8, 8}, notch<1, 4>};
  constexpr grid unit_steps{interval{-3, 7}, detail::rational{1}};
  constexpr auto hulled = hull(quarter_steps, unit_steps);
  STATIC_REQUIRE(hulled.has_value());
  STATIC_REQUIRE(grid::try_make(hulled->Interval, hulled->Notch).has_value());
}

//---------------------------------------------------------------------------
// std::common_type / common_bound_t
//---------------------------------------------------------------------------
TEST_CASE("common_type of a bound with itself is the bound, policy included",
          "[bound][common_type]")
{
  using percent = bound<{0, 100}>;
  STATIC_REQUIRE(std::same_as<std::common_type_t<percent, percent>, percent>);

  using clamped_percent = bound<{0, 100}, clamp>;
  STATIC_REQUIRE(
      std::same_as<std::common_type_t<clamped_percent, clamped_percent>, clamped_percent>);
}

TEST_CASE("common_type of mixed grids is the hull type", "[bound][common_type]")
{
  using percent = bound<{0, 100}>;                    // notch 1
  using halves  = bound<{{-50, 50}, notch<1, 2>}>;
  using common  = std::common_type_t<percent, halves>;

  STATIC_REQUIRE(Lower<common> == detail::rational{-50});
  STATIC_REQUIRE(Upper<common> == detail::rational{100});
  STATIC_REQUIRE(Notch<common> == (notch<1, 2>));
  STATIC_REQUIRE(std::same_as<common, common_bound_t<percent, halves>>);
  STATIC_REQUIRE(std::same_as<common, std::common_type_t<halves, percent>>);

  // Both operand types convert into the hull losslessly.
  constexpr common from_percent{percent{42}};
  constexpr common from_halves{halves{-0.5_b}};
  STATIC_REQUIRE(from_percent == common{42});
  STATIC_REQUIRE(from_halves == common{-0.5_b});
}

TEST_CASE("common_type SFINAEs away when the hull notch is unrepresentable",
          "[bound][common_type]")
{
  // Coprime ~2^32 denominators: the notch gcd's lcm denominator exceeds imax.
  using prime_notch_a = bound<{{0, 1}, notch<1, 4294967291>}>;
  using prime_notch_b = bound<{{0, 1}, notch<1, 4294967311>}>;
  STATIC_REQUIRE(!has_common_type<prime_notch_a, prime_notch_b>);
  STATIC_REQUIRE(!has_common_bound<prime_notch_a, prime_notch_b>);
}

//---------------------------------------------------------------------------
// mixed-grid min / max
//---------------------------------------------------------------------------
TEST_CASE("mixed-grid min/max return the hull type", "[bound][min][max]")
{
  using percent = bound<{0, 100}>;
  using halves  = bound<{{-50, 50}, notch<1, 2>}>;
  using common  = common_bound_t<percent, halves>;

  constexpr percent three{3};
  constexpr halves  minus_half{-0.5_b};

  constexpr auto lo = bnd::min(three, minus_half);
  constexpr auto hi = bnd::max(three, minus_half);
  STATIC_REQUIRE(std::same_as<decltype(lo), const common>);
  STATIC_REQUIRE(lo == common{-0.5_b});
  STATIC_REQUIRE(hi == common{3});

  // Same-type overload still returns the operand type.
  constexpr auto same = bnd::min(percent{7}, percent{5});
  STATIC_REQUIRE(std::same_as<decltype(same), const percent>);
  STATIC_REQUIRE(same == percent{5});
}
