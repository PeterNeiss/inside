// EXPECT: notch 3, Upper − Lower 10
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The diagnostic names the values: a notch of 3 does not divide
// the span 10.
#include <inside/inside.hpp>

using namespace ins;

int main() {
    id<key{grid{{0, 10}, 3}}> x{};
    (void)x;
}
