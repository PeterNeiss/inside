// EXPECT: an index converts to the builtin integers that hold its whole grid
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An index converts to a builtin integer that holds its grid, never to a
// smaller one.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    inside<{0, 300}> i{};
    unsigned char    c = i;
    (void)c;
}
