// EXPECT: has no text that names it here
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A unit of your own whose symbol the catalog reads (km, one of its prefixed units) has no quantity type: the
// text km names the catalog's kilometre.
#include <inside/units.hpp>
inline constexpr auto own_km = ins::base_unit<"km", "own_length">;
int main() {
    ins::id<ins::quantity_key<own_km, ins::id<ins::key{ins::grid{0, 9}}>>> x;
    (void)x;
}
