// EXPECT: adding multivectors of different dimensions
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A multivector's parts have one dimension but for their blades: a vector in
// metres does not take a part in seconds.
#include <beman/inside/units.hpp>
#include <beman/inside/units/geoalg.hpp>

inline constexpr auto e1 = beman::inside::units::basis_vector<"e1", 1>;
inline constexpr auto e2 = beman::inside::units::basis_vector<"e2", 1>;
inline constexpr auto e3 = beman::inside::units::basis_vector<"e3", 1>;

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto v = just<1> * si::metre * e1 + just<1> * si::metre * e2;
    auto w = v + just<1> * si::second * e3;
    (void)w;
}
