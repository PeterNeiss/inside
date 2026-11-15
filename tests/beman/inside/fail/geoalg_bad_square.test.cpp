// EXPECT: a basis vector squares to
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A basis vector squares to +1, -1 or 0, nothing else.
#include <beman/inside/units.hpp>

inline constexpr auto e1_def = beman::inside::units::basis_vector<"e1", 2>;

int main() {}
