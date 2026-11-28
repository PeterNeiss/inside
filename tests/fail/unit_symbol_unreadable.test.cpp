// EXPECT: a unit's symbol holds no space
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A unit's symbol is a word a quantity's type can spell: [km/h] reads no unit named "fl oz".
#include <inside/units.hpp>
inline constexpr auto fl_oz = ins::unit<"fl oz", ins::just<29573, 1000000> * ins::si::litre>;
int main() {
    (void)fl_oz;
}
