// EXPECT: no closed form
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// e₁e₂ + e₃e₄ in G(4,0): the two bivectors commute, so the argument's square
// is no scalar and exp has no closed form.
#include <beman/inside/units.hpp>
#include <beman/inside/units/geoalg.hpp>
#include <beman/inside/units/math.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    constexpr auto e1 = beman::inside::units::basis_vector<"e1", 1>;
    constexpr auto e2 = beman::inside::units::basis_vector<"e2", 1>;
    constexpr auto e3 = beman::inside::units::basis_vector<"e3", 1>;
    constexpr auto e4 = beman::inside::units::basis_vector<"e4", 1>;
    using I           = inside<{{-1, 1}, per<64>}, round_half_away>;
    using dom         = multivector<quantity<e1 * e2, I>, quantity<e3 * e4, I>>;
    using cod         = multivector<I, quantity<e1 * e2, I>, quantity<e3 * e4, I>, quantity<e1 * e2 * e3 * e4, I>>;
    const dom x{I{1} * (e1 * e2) + I{1} * (e3 * e4)};
    (void)math::exp<cod>(x);
}
