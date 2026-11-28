// EXPECT: take geometric basis vectors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Inner basis vectors have only their dot product: no wedge, no contractions.
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
using namespace ins::literals;

inline constexpr auto b1 = ins::basis_vector<"b1", ins::inner>;
inline constexpr auto b2 = ins::basis_vector<"b2", ins::inner>;

int main() {
    auto w = ins::wedge(1_q * b1, 1_q * b2);
    (void)w;
}
