// EXPECT: adding multivectors of different dimensions
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A multivector's parts have one dimension but for their blades: a vector in
// metres does not take a part in seconds.
#include <beman/inside/units.hpp>
#include <beman/inside/units/geoalg.hpp>

inline constexpr beman::inside::units::basis_vector e1_def{"e1", 1};
inline constexpr beman::inside::units::basis_vector e2_def{"e2", 1};
inline constexpr beman::inside::units::basis_vector e3_def{"e3", 1};

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto v = just<1> * si::metre * unit_c<e1_def> + just<1> * si::metre * unit_c<e2_def>;
    auto w = v + just<1> * si::second * unit_c<e3_def>;
    (void)w;
}
