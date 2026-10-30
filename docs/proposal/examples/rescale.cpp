// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour: a conversion rescales the grid and keeps the raw; a mixed-unit sum is exact.
#include <beman/inside/units.hpp>

#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::units;

template <class A, class B>
concept addable = requires(A a, B b) { a + b; };

int main() {
    // [listing]
    constexpr auto km = si::kilo(si::metre);
    quantity<km, inside<{{0, 500}, per<1000>}>> trip{12.5};    // 12.5 km, to the metre
    auto m = trip.in(si::metre);                               // 12500 m: the same raw, grid × 1000
    static_assert(grid_of<decltype(m)::rep> == grid{0, 500'000});

    auto total = trip + just<800> * si::metre;                 // 13300 m: the sum runs in metres
    auto speed = total / quantity<si::minute, inside<{1, 600}>>{20};
    auto kmh   = speed.in(km / si::hour);                      // 39.9 km/h, exactly
    inside<{0, 1000}, round_nearest> laps = trip / (just<400> * si::metre); // 31: km / m is a number
    static_assert(!addable<decltype(trip), decltype(just<1> * si::second)>); // another dimension
    // [/listing]
    if (m.numerical_value().raw() != trip.numerical_value().raw() || m.numerical_value() != just<12'500>)
        return 1;
    if (total.numerical_value_in(si::metre) != just<13'300> || kmh.numerical_value() != 39.9_ins || laps != just<31>)
        return 1;
    return 0;
}
