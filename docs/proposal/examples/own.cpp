// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour: a base dimension, a quantity kind and units of your own.
#include <beman/inside/units.hpp>

#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::units;

template <class A, class B>
concept addable = requires(A a, B b) { a + b; };

// [listing]
inline constexpr base_dimension currency_def{"currency"};
inline constexpr quantity_spec  money_def{"money", currency_def};
inline constexpr named_unit     euro_def{"EUR", spec_c<money_def>};
inline constexpr named_unit     cent_def{"ct", mag<rational{1, 100}> * unit_c<euro_def>};
inline constexpr auto           euro = unit_c<euro_def>;
inline constexpr auto           cent = unit_c<cent_def>;

using price = quantity<euro / si::kilogram, inside<{{0, 100}, per<100>}>>;
using mass  = quantity<si::kilogram, inside<{{0, 10}, per<1000>}>>;

constexpr auto cost  = price{3.49_ins} * mass{1.25_ins};     // 4.3625 EUR, exactly
constexpr auto cents = cost.in(cent);                        // 436.25 ct: the same raw
static_assert(!addable<decltype(cost), mass>);               // money never adds to a mass
// [/listing]

int main() {
    if (cost.numerical_value() != 4.3625_ins || cents.numerical_value() != 436.25_ins)
        return 1;
    return 0;
}
