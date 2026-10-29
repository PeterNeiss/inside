// EXPECT: notch 3, Upper − Lower 10
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Under C++26 the diagnostic names the values: a notch of 3 does not divide
// the span 10.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    inside<{{0, 10}, 3}> x{};
    (void)x;
}
