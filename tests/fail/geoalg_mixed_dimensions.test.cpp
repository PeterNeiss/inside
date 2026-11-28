// EXPECT: adding multivectors of different dimensions
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A multivector's parts have one dimension but for their blades: a vector in
// metres does not take a part in seconds.
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>

inline constexpr auto e1 = ins::basis_vector<"e1", ins::geometric<1>>;
inline constexpr auto e2 = ins::basis_vector<"e2", ins::geometric<1>>;
inline constexpr auto e3 = ins::basis_vector<"e3", ins::geometric<1>>;

int main() {
    using namespace ins;
    auto v = 1_q * si::metre * e1 + 1_q * si::metre * e2;
    auto w = v + 1_q * si::second * e3;
    (void)w;
}
