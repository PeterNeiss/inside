// EXPECT: has no inverse
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A null vector (it squares to 0) has no inverse: dividing by it is an error.
#include <beman/inside/units.hpp>

inline constexpr auto eps_def = beman::inside::units::basis_vector<"eps", 0>;

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto q = just<1> / (just<1> * eps_def);
    (void)q;
}
