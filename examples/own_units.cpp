// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Units, specs, origins and base dimensions of your own. Each is a static
// object plus a tag made from it, and from then on works like the catalog.
//   1. A unit from existing ones: the fathom (1.8288 m) for a `depth` spec
//      under length. Conversions are exact, and prefixes apply.
//   2. A unit with an origin: the Rankine scale (°R, 5/9 K from absolute
//      zero), defined with unit_options. Its points meet ℃ and ℉ exactly.
//   3. A new base dimension, currency: EUR and ct. A price per kilogram
//      times a mass is an amount of money, exactly; money never adds to a
//      length.
//   4. A second one, information: bit, byte and KiB. A file size over a
//      bandwidth is a time, exactly.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/units.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
// 1. A spec under length and a unit defined by the metre.
inline constexpr quantity_spec depth_def{"depth", isq::length};
inline constexpr auto          depth = spec_c<depth_def>;
inline constexpr named_unit    fathom_def{"ftm", mag<rational{18288, 10000}> * si::metre};
inline constexpr auto          fathom = unit_c<fathom_def>;

// 2. A temperature unit whose points count from absolute zero: the options
//    name its ASCII symbol and its origin.
inline constexpr named_unit rankine_def{"°R",
                                        mag<rational{5, 9}> * si::kelvin,
                                        {.ascii = "R", .origin = si::absolute_zero}};
inline constexpr auto       rankine = unit_c<rankine_def>;

// 3. Currency: a base dimension, its base quantity and two units.
inline constexpr base_dimension currency_def{"currency"};
inline constexpr quantity_spec  money_def{"money", currency_def};
inline constexpr auto           money = spec_c<money_def>;
inline constexpr named_unit     euro_def{"EUR", money};
inline constexpr auto           euro = unit_c<euro_def>;
inline constexpr named_unit     cent_def{"ct", mag<rational{1, 100}> * euro};
inline constexpr auto           cent = unit_c<cent_def>;

// 4. Information: bit, byte (8 bit), KiB (1024 B).
inline constexpr base_dimension information_def{"information"};
inline constexpr quantity_spec  amount_def{"amount_of_information", information_def};
inline constexpr auto           amount = spec_c<amount_def>;
inline constexpr named_unit     bit_def{"bit", amount};
inline constexpr auto           bit = unit_c<bit_def>;
inline constexpr named_unit     byte_def{"B", mag<8> * bit};
inline constexpr auto           byte = unit_c<byte_def>;
inline constexpr named_unit     kibibyte_def{"KiB", mag<1024> * byte};
inline constexpr auto           kibibyte = unit_c<kibibyte_def>;

template <class A, class B>
concept addable = requires(const A& a, const B& b) { a + b; };
} // namespace

int main() {
    // 1. A depth of 12 fathoms is 21.9456 m; a kilofathom is 1828.8 m.
    const quantity<depth[fathom], inside<{0, 100}>> sounding{12};
    std::cout << "sounding   " << sounding << " = " << sounding.in(si::metre) << '\n';
    if (to_string(sounding.in(si::metre)) != "21.9456 m" ||
        to_string((just<1> * si::kilo(fathom)).in(si::metre)) != "1828.8 m")
        return 1;
    static_assert(sounding.quantity_spec == depth);

    // 2. Water freezes at 491.67 °R: the same point as 0 ℃ and 32 ℉.
    const auto freezing = point<rankine>(inside<{{0, 1000}, per<100>}>{rational{49167, 100}});
    std::cout << "freezing   " << freezing << " = " << freezing.point_in(si::degree_Celsius) << " = "
              << freezing.point_in(international::degree_Fahrenheit) << '\n';
    if (!(freezing == point<si::degree_Celsius>(just<0>)) ||
        to_string(freezing.point_in(international::degree_Fahrenheit)) != "32.00 ℉" ||
        to_string(freezing, text_encoding::ascii) != "491.67 R")
        return 1;

    // 3. 1.25 kg of coffee at 3.99 EUR/kg is 4.9875 EUR, exactly; with a 50 ct
    //    tip the sum runs in cents.
    const quantity<euro / si::kilogram, inside<{{0, 100}, per<100>}>> price{rational{399, 100}};
    const quantity<si::kilogram, inside<{{0, 10}, per<1000>}>>        coffee{rational{1250, 1000}};
    const auto                                                        cost = price * coffee;
    const auto                                                        paid = cost + just<50> * cent;
    std::cout << "coffee     " << cost << ", with tip " << paid << '\n';
    if (to_string(cost) != "4.98750 EUR" || to_string(paid) != "548.750 ct")
        return 1;
    // Money and a length do not add: `cost + just<1> * si::metre` does not compile.
    static_assert(!addable<decltype(cost), decltype(just<1> * si::metre)>);

    // 4. 64 KiB over 1 Mbit/s takes 64·1024·8 / 10^6 s = 0.524288 s.
    const quantity<kibibyte, inside<{0, 1024}>>                       file{64};
    const quantity<si::mega(bit) / si::second, inside<{1, 100}>>      link{1};
    const auto                                                        time = (file / link).in(si::second);
    std::cout << "transfer   " << file << " at " << link << " takes " << time << '\n';
    if (to_string(time) != "0.524288 s" || to_string(file.in(bit)) != "524288 bit")
        return 1;
    return 0;
}
