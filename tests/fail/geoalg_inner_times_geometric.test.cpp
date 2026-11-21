// EXPECT: inner and geometric basis vectors do not multiply
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An inner basis vector and a geometric one have no product.
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>

inline constexpr auto b1 = ins::basis_vector<"b1", ins::inner>;
inline constexpr auto e1 = ins::basis_vector<"e1", ins::geometric<+1>>;

int main() {
    auto p = (ins::just<1> * b1) * (ins::just<1> * e1);
    (void)p;
}
