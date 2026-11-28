// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Units, kinds, origins and base dimensions of your own:
// each is one declaration, or a named definition and its name for short type names,
// and from then on it works like the catalog.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;

namespace
{
  // 1. A kind under length, and a unit defined by the metre.
  inline constexpr auto depth  = kind<"depth", isq::length>;
  inline constexpr auto fathom = unit<"ftm", 1.8288_q * si::metre>;

  // 2. A temperature unit whose points count from absolute zero; the options name its ASCII symbol and origin.
  inline constexpr auto rankine = unit<"°R", "5/9"_q * si::kelvin, {.ascii = "deg_R", .origin = si::absolute_zero}>;

  // 3. Currency: a base dimension, its base quantity and two units, as named definitions:
  //    diagnostics and symbols spell them money_def, euro_def and cent_def, not their content.
  inline constexpr auto     currency = base_dimension<"currency">;
  inline constexpr kind_def money_def{"money", currency};
  inline constexpr auto     money = defined_by<money_def>;
  inline constexpr unit_def euro_def{"EUR", money};
  inline constexpr auto     euro = defined_by<euro_def>;
  inline constexpr unit_def cent_def{"ct", 0.01_q * euro};
  inline constexpr auto     cent = defined_by<cent_def>;

  // 4. Information: bit, byte (8 bit), KiB (1024 B).
  inline constexpr auto information = base_dimension<"information">;
  inline constexpr auto amount      = kind<"amount_of_information", information>;
  inline constexpr auto bit         = unit<"bit", amount>;
  inline constexpr auto byte        = unit<"B"  , 8_q * bit>;
  inline constexpr auto kibibyte    = unit<"KiB", 1024_q * byte>;

  // 5. A prefixed unit of your own is named by a tag of it: a quantity's type spells its unit by symbol (kftm, Mbit).
  inline constexpr auto kilofathom = si::kilo(fathom);
  inline constexpr auto megabit    = si::mega(bit);

  template <class A, class B>
  concept addable = requires(const A& a, const B& b) { a + b; };
} // namespace

int main()
{
  // 1. A depth of 12 fathoms is 21.9456 m, exactly; prefixes apply: a kilofathom is 1828.8 m.
  using fathoms = id<"[0..100] [ftm]{depth}">;
  const fathoms sounding{12, depth[fathom]};
  std::cout << "sounding   " << sounding << " = " << sounding.in(si::metre) << '\n';
  if (to_string(sounding.in(si::metre)) != "21.9456 m"
      || to_string((1_q * si::kilo(fathom)).in(si::metre)) != "1828.8 m")
    return 1;
  // Text names it too: parse finds every unit declared where it is called.
  using metres = id<"[0..1000] 1/10'000 [m]">;
  const auto read{parse<metres>("12 ftm")};
  std::cout << "read       12 ftm = " << *read << '\n';
  if (to_string(*read) != "21.9456 m")
    return 1;
  static_assert(sounding.kind == depth);

  // 2. Water freezes at 491.67 °R: the same point as 0 ℃ and 32 ℉.
  using rankine_point = id<"[0..1000] 1/100 [deg_R] @absolute_zero">;
  const rankine_point freezing{491.67_q, rankine};
  std::cout << "freezing   " << freezing << " = " << freezing.point_in(si::degree_Celsius) << " = "
            << freezing.point_in(international::degree_Fahrenheit) << '\n';
  if (!(freezing == point<si::degree_Celsius>(0_q))
      || to_string(freezing.point_in(international::degree_Fahrenheit)) != "32.00 ℉"
      || to_string(freezing, text_encoding::ascii) != "491.67 deg_R")
    return 1;

  // 3. 1.25 kg of coffee at 3.99 EUR/kg is 4.9875 EUR exactly; with a 50 ct tip the sum runs in cents.
  using euros_per_kg = id<"[0..100] 1/100 [EUR/kg]">;
  using kilograms    = id<"[0..10] 1/1000 [kg]">;

  const euros_per_kg price {3.99_q * (euro / si::kilogram)};
  const kilograms    coffee{1.25_q * si::kilogram};
  const auto         cost  {price * coffee};
  const auto         paid  {cost + 50_q * cent};
  std::cout << "coffee     " << cost << ", with tip " << paid << '\n';
  if (to_string(cost) != "4.98750 EUR" || to_string(paid) != "548.750 ct")
    return 1;
  static_assert(!addable<decltype(cost), decltype(1_q * si::metre)>); // money never adds to a length

  // 4. A file size over a bandwidth is a time: 64 KiB at 1 Mbit/s takes 64·1024·8 / 10^6 s = 0.524288 s.
  using kibibytes      = id<"[0..1024] [KiB]">;
  using megabits_per_s = id<"[1..100] [Mbit/s]">;

  const kibibytes      file{64, kibibyte};
  const megabits_per_s link{1 , si::mega(bit) / si::second};
  const auto           time{(file / link).in(si::second)};
  std::cout << "transfer   " << file << " at " << link << " takes " << time << '\n';
  if (to_string(time) != "0.524288 s" || to_string(file.in(bit)) != "524288 bit")
    return 1;
  return 0;
}
