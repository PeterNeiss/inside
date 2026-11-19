// EXPECT: points of different absolute origins
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Two points counted from unrelated absolute origins have no difference.
#include <inside/units.hpp>

inline constexpr auto other_zero = ins::origin<"other_zero", ins::base_dimension<"thermodynamic_temperature">>;

int main() {
    using namespace ins;
    const auto a = other_zero + just<5> * si::kelvin;
    const auto b = point<si::kelvin>(just<1>);
    auto       d = a.quantity_from(si::absolute_zero);
    (void)b;
    (void)d;
}
