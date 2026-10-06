// EXPECT: modulo requires integer-valued grids and snap
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// `%` is integer-only by design — non-integer remainders are not representable
// on a fractional grid without a rounding story.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    inside<{{0, 8}, per<2>}> halves{2.5_ins};
    inside<{{1, 4}, per<2>}> divisor{1.5_ins};
    auto                     rem = halves % divisor; // ill-formed: fractional grids
    (void)rem;
}
