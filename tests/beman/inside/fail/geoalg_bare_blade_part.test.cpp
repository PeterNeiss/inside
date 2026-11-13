// EXPECT: a blade's part is a quantity
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A multivector's blade part is a quantity: {3, just<4> * i}, not {3, 4}.
#include <beman/inside/units/geoalg.hpp>

inline constexpr beman::inside::units::basis_vector e1_def{"e1", 1};
inline constexpr beman::inside::units::basis_vector e2_def{"e2", 1};

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    constexpr auto i = unit_c<e1_def> * unit_c<e2_def>;
    using I          = inside<{-10, 10}>;
    const multivector<I, quantity<i, I>> z{3, 4};
    (void)z;
}
