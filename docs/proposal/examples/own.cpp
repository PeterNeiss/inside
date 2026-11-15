// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour: a base dimension, a quantity kind and units of your own.
#include <beman/inside/units.hpp>

#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::units;

template <class A, class B>
concept addable = requires(A a, B b) { a + b; };

// [listing]
inline constexpr auto euro = base_unit<"EUR", "currency">;
inline constexpr auto           cent = unit<"ct", 0.01_ins * euro>;

using price = quantity<euro / si::kilogram, inside<{{0, 100}, per<100>}>>;
using mass  = quantity<si::kilogram, inside<{{0, 10}, per<1000>}>>;

constexpr auto cost  = price{3.49_ins, euro / si::kilogram} * mass{1.25_ins, si::kilogram}; // 4.3625 EUR, exactly
constexpr auto cents = cost.in(cent);                                                       // 436.25 ct: the same raw
static_assert(!addable<decltype(cost), mass>);                                              // money never adds to a mass
// [/listing]

int main() {
    if (cost.numerical_value_in(euro) != 4.3625_ins || cents.numerical_value_in(cent) != 436.25_ins)
        return 1;
    return 0;
}
