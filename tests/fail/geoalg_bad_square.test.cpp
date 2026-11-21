// EXPECT: a basis vector squares to
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A basis vector squares to +1, -1 or 0, nothing else.
#include <inside/units.hpp>

inline constexpr auto e1 = ins::basis_vector<"e1", ins::geometric<2>>;

int main() {}
