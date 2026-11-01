// EXPECT: an inside cannot be combined with a rational
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Through operator rational() an inside plus a rational would become rational
// arithmetic, an expected<rational, errc> without a grid; it is deleted, with
// the spelling that keeps the grid (just<1, 2>, 0.5_ins).
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    inside<{0, 10}> x{3};
    auto            y = x + 0.5_r;
    (void)y;
}
