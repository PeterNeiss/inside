// EXPECT: sqrt: input must be non-negative
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// sqrt checks its domain on the input grid, as log does: a grid that reaches
// below 0 is rejected at compile time instead of a domain_error at run time.
#include <inside/math.hpp>

using namespace ins;

int main() {
    using level = id<"[-4..4] 1/256 round_half_away">;
    auto r      = math::sqrt(level{2.25});
    (void)r;
}
