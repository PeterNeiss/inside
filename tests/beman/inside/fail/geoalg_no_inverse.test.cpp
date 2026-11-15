// EXPECT: no inverse of the form
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An even element of G(4,0) with two commuting bivectors: x times its reverse
// has a grade-4 part, so inverse() does not apply.
#include <beman/inside/units.hpp>
#include <beman/inside/units/geoalg.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    constexpr auto e1 = beman::inside::units::basis_vector<"e1", 1>;
    constexpr auto e2 = beman::inside::units::basis_vector<"e2", 1>;
    constexpr auto e3 = beman::inside::units::basis_vector<"e3", 1>;
    constexpr auto e4 = beman::inside::units::basis_vector<"e4", 1>;
    auto           x  = just<1> + just<1> * e1 * e2 + just<1> * e3 * e4;
    auto           y  = inverse(x);
    (void)y;
}
