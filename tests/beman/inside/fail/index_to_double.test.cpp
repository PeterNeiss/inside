// EXPECT: an index converts to the builtin integers that hold its whole grid
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An index converts to builtin integers only: not to a floating type.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    inside<{0, 7}> i{};
    double         d = i;
    (void)d;
}
