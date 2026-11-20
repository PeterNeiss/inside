// EXPECT: d is a unit_def, kind_def, origin_def or basis_vector_def
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// defined_by takes a named definition object, not any constant.
#include <inside/units.hpp>

inline constexpr int not_a_definition = 1;

int main() {
    auto u = ins::defined_by<not_a_definition>;
    (void)u;
}
