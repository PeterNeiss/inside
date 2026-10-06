// EXPECT: cannot be combined with a raw scalar
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Grid-less scalars are rejected with a fix-it (write `a + 1_ins`), not a wall
// of overload-resolution noise — the guidance overload's static_assert fires.
#include <beman/inside/inside.hpp>

int main() {
    beman::inside::inside<{0, 100}> percent{50};
    auto                            sum = percent + 1; // ill-formed: scalar has no grid
    (void)sum;
}
