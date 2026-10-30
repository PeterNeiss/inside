// EXPECT: rounding_mode
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The mode on a deduced math call must be a rounding mode.
#include <beman/inside/cmath.hpp>

using namespace beman::inside;

int main() {
    (void)math::exp<clamp>(inside<{{1, 16}, per<64>}>{2});
}
