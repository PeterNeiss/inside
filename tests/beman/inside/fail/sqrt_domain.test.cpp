// EXPECT: sqrt: input must be non-negative
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// sqrt checks its domain on the input grid, as log does: a grid that reaches
// below 0 is rejected at compile time instead of a domain_error at run time.
#include <beman/inside/math.hpp>

using namespace beman::inside;

int main() {
    using level = inside<{{-4, 4}, per<256>}, round_nearest>;
    auto r      = math::sqrt(level{2.25});
    (void)r;
}
