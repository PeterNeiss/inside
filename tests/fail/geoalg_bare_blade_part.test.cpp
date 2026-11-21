// EXPECT: a blade's part is a quantity
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A multivector's blade part is a quantity: {3, just<4> * i}, not {3, 4}.
#include <inside/units/geoalg.hpp>

inline constexpr auto e1 = ins::basis_vector<"e1", ins::geometric<1>>;
inline constexpr auto e2 = ins::basis_vector<"e2", ins::geometric<1>>;

int main() {
    using namespace ins;
    constexpr auto i = e1 * e2;
    using I          = inside<{-10, 10}>;
    const multivector<I, quantity<i, I>> z{3, 4};
    (void)z;
}
