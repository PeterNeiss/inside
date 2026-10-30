// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour: points count from an origin, and change origin exactly.
#include <beman/inside/units.hpp>

#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::units;

template <class A, class B>
concept addable = requires(A a, B b) { a + b; };

int main() {
    // [listing]
    auto room = point<si::degree_Celsius>(inside<{{-50, 50}, per<10>}>{20});
    auto k    = room.point_for(si::absolute_zero).in(si::kelvin);      // 293.15 K, exactly
    auto f    = room.point_in(international::degree_Fahrenheit);       // 68 ℉, exactly
    auto warm = room + just<5> * si::kelvin;                           // 25 ℃: a point plus a difference
    auto rise = warm - room;                                           // 5 ℃, a quantity: a difference
    static_assert(!addable<decltype(room), decltype(room)>);           // points do not add
    // [/listing]
    if (k.quantity_from_zero().numerical_value() != 293.15_ins)
        return 1;
    if (f.quantity_from_origin().numerical_value() != just<68> || rise.numerical_value() != just<5>)
        return 1;
    return 0;
}
