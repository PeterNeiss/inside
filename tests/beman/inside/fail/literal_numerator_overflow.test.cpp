// EXPECT: literal: numerator overflow
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A literal's value is a 64-bit rational: 10^20 passes its numerator.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    constexpr auto x = 100000000000000000000_r;
    (void)x;
}
