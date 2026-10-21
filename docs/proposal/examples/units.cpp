// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour 6: a quantity carries its unit, and a conversion rescales the grid.
#include <beman/inside/units.hpp>

#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::units;

template <class A, class B>
concept addable = requires(A a, B b) { a + b; };

int main() {
    // [listing]
    quantity<si::kilo(si::metre), inside<{{0, 500}, per<1000>}>> trip{12.5};
    auto m = trip.in(si::metre);                     // 12500 m: the same raw, grid × 1000
    static_assert(grid_of<decltype(m)::rep> == grid{0, 500'000});

    auto room = point<si::degree_Celsius>(inside<{{-50, 50}, per<10>}>{20});
    auto k    = room.point_for(si::absolute_zero).in(si::kelvin); // 293.15 K, exactly

    quantity<si::hertz, inside<{0, 1000}>>     f{50};
    quantity<si::becquerel, inside<{0, 1000}>> a{50};
    static_assert(!addable<decltype(f), decltype(a)>); // both 1/s, but not one kind

    quantity<isq::width[si::metre], inside<{1, 10}>>  w{3};
    quantity<isq::height[si::metre], inside<{1, 10}>> h{4};
    quantity<isq::area[square(si::metre)], inside<{1, 100}>> area = w * h;
    inside<{0, 10'000}, round_nearest> ratio = trip / w; // km / m: a plain number
    // [/listing]
    if (m.numerical_value().raw() != trip.numerical_value().raw() || m.numerical_value() != just<12'500>)
        return 1;
    if (k.quantity_from_zero().numerical_value() != 293.15_ins)
        return 1;
    if (area.numerical_value() != just<12> || ratio != just<4'167>)
        return 1;
    return 0;
}
