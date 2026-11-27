// EXPECT: each I is an index type
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// extents_of takes index types: insides on non-negative integers.
#include <inside/inside.hpp>
#include <inside/inside/mdspan.hpp>

int main() {
    using namespace ins;
    extents_of<id<"[-1..5]">> e;
    (void)e;
}
