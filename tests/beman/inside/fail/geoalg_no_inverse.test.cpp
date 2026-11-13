// EXPECT: no inverse of the form
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An even element of G(4,0) with two commuting bivectors: x times its reverse
// has a grade-4 part, so inverse() does not apply.
#include <beman/inside/units.hpp>
#include <beman/inside/units/geoalg.hpp>

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
    auto           x  = just<1> + just<1> * e1 * e2 + just<1> * e3 * e4;
    auto           y  = inverse(x);
    (void)y;
}
