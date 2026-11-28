// EXPECT: inner and geometric basis vectors do not multiply
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An inner basis vector and a geometric one have no product.
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
using namespace ins::literals;

inline constexpr auto b1 = ins::basis_vector<"b1", ins::inner>;
inline constexpr auto e1 = ins::basis_vector<"e1", ins::geometric<+1>>;

int main() {
    auto p = (1_q * b1) * (1_q * e1);
    (void)p;
}
