// EXPECT: literal: p exponent too large
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 2^100 passes the 64-bit numerator.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    constexpr auto x = 0x1p100_r;
    (void)x;
}
