// EXPECT: a continuous grid has none to count
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// index() counts notches.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    const inside<{{0, 1}, max_denominator{10}}> x = 0;
    (void)x.index();
}
