// EXPECT: has no inverse
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A null vector (it squares to 0) has no inverse: dividing by it is an error.
#include <inside/units.hpp>

inline constexpr auto eps = ins::basis_vector<"eps", 0>;

int main() {
    using namespace ins;
    auto q = just<1> / (just<1> * eps);
    (void)q;
}
