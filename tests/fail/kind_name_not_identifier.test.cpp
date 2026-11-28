// EXPECT: a kind's name is letters, digits and _
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A kind's name is spelled by a quantity's type, so it is an identifier.
#include <inside/units.hpp>
inline constexpr auto depth = ins::kind<"water depth", ins::isq::length>;
int main() {
    auto x = depth;
    (void)x;
}
