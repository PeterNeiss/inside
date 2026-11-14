// EXPECT: each I is an index type
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// extents_of takes index types: insides on non-negative integers.
#include <beman/inside/inside.hpp>
#include <beman/inside/inside/mdspan.hpp>

int main() {
    using namespace beman::inside;
    extents_of<inside<{-1, 5}>> e;
    (void)e;
}
