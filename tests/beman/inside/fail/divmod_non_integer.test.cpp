// EXPECT: divmod requires integer-valued grids
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// divmod rounds an integer quotient, so like `%` it is integer-only.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    inside<{{0, 8}, per<2>}> halves{2.5_ins};
    inside<{1, 4}>           b{2};
    auto [q, r] = divmod(halves, b); // ill-formed: a fractional grid
    (void)q;
    (void)r;
}
