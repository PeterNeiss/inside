// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The two layers at a glance: quantities whose number is an inside. A
// percentage, money in euros and cents, an angle in degrees that converts
// to radians, a sample rate, a distance and a height.

#include <iostream>

#include <beman/inside/io.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units_math.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

// Units of our own: per cent, a currency, samples per second.
inline constexpr named_unit     percent_def{"%", mag<rational{1, 100}> * one,
                                            {.space_before = false}};
inline constexpr auto           percent = unit_c<percent_def>;
inline constexpr base_dimension currency_def{"currency"};
inline constexpr quantity_spec  amount_def{"amount_of_money", currency_def};
inline constexpr named_unit     euro_def{"EUR", spec_c<amount_def>};
inline constexpr auto           euro = unit_c<euro_def>;
inline constexpr named_unit     cent_def{"ct", mag<rational{1, 100}> * euro};
inline constexpr auto           cent = unit_c<cent_def>;
inline constexpr named_unit     sps_def{"SPS", one / si::second, {.kind = isq::frequency}};
inline constexpr auto           sps = unit_c<sps_def>;

using pct         = quantity<percent, inside<{0, 100}>>;
using money       = quantity<euro, inside<{{0, 1'000'000}, per<100>}, round_nearest>>;
using angle       = quantity<si::degree, inside<{0, 359}, wrap>>;   // modular
using rad_number  = inside<{{0, 7}, per<10'000>}, round_nearest>;
using deg_number  = inside<{{0, 360}, per<100>}, round_nearest>;
using sample_rate = quantity<sps, inside<{0, 192'000}>>;
using trip        = quantity<si::kilo(si::metre), inside<{{0, 500}, per<1000>}>>;
using height      = quantity<isq::height[si::metre], inside<{0, 9000}>>;

int main()
{
  std::cout << "pct          = " << pct{42} << "\n";
  money price{19.99};
  std::cout << "money        = " << price << " = " << price.in(cent) << "\n";
  std::cout << "angle        = " << angle{370} << "   (370° wraps)\n";
  // Degrees <-> radians is a factor of pi: exact, then rounded once.
  auto rad = value_cast<si::radian, rad_number>(angle{90});
  auto deg = value_cast<si::degree, deg_number>(quantity<si::radian, inside<{0, 6}>>{1});
  std::cout << "90° in rad   = " << rad << "\n";
  std::cout << "1 rad in deg = " << deg << "\n";
  std::cout << "sample_rate  = " << sample_rate{48'000} << "\n";
  std::cout << "trip         = " << trip{12.5} << " = " << trip{12.5}.in(si::metre) << "\n";
  std::cout << "height       = " << height{8849} << "\n";
  return 0;
}
