// EXPECT: no inverse of the form
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An even element of G(4,0) with two commuting bivectors: x times its reverse
// has a grade-4 part, so inverse() does not apply.
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>

inline constexpr auto e1 = ins::basis_vector<"e1", ins::geometric<1>>;
inline constexpr auto e2 = ins::basis_vector<"e2", ins::geometric<1>>;
inline constexpr auto e3 = ins::basis_vector<"e3", ins::geometric<1>>;
inline constexpr auto e4 = ins::basis_vector<"e4", ins::geometric<1>>;

int main() {
    using namespace ins;
    auto x = 1_q + 1_q * e1 * e2 + 1_q * e3 * e4;
    auto y = inverse(x);
    (void)y;
}
