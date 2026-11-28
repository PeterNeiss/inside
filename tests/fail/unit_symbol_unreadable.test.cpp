// EXPECT: a unit's symbol holds no space
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A unit's symbol is a word a quantity's type can spell: [km/h] reads no unit named "fl oz".
#include <inside/units.hpp>
using namespace ins::literals;
inline constexpr auto fl_oz = ins::unit<"fl oz", 0.029573_q * ins::si::litre>;
int main() {
    (void)fl_oz;
}
