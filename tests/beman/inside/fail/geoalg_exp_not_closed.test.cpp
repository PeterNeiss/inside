// EXPECT: no closed form
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// e₁e₂ + e₃e₄ in G(4,0): the two bivectors commute, so the argument's square
// is no scalar and exp has no closed form.
#include <beman/inside/units.hpp>
#include <beman/inside/units/geoalg.hpp>
#include <beman/inside/units/math.hpp>

inline constexpr beman::inside::units::basis_vector e1_def{"e1", 1};
inline constexpr beman::inside::units::basis_vector e2_def{"e2", 1};
inline constexpr beman::inside::units::basis_vector e3_def{"e3", 1};
inline constexpr beman::inside::units::basis_vector e4_def{"e4", 1};

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    constexpr auto e1 = unit_c<e1_def>;
    constexpr auto e2 = unit_c<e2_def>;
    constexpr auto e3 = unit_c<e3_def>;
    constexpr auto e4 = unit_c<e4_def>;
    using I           = inside<{{-1, 1}, per<64>}, round_nearest>;
    using dom         = multivector<quantity<e1 * e2, I>, quantity<e3 * e4, I>>;
    using cod         = multivector<I, quantity<e1 * e2, I>, quantity<e3 * e4, I>, quantity<e1 * e2 * e3 * e4, I>>;
    const dom x{I{1} * (e1 * e2) + I{1} * (e3 * e4)};
    (void)math::exp<cod>(x);
}
