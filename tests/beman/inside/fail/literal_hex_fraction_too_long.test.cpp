// EXPECT: literal: hex fraction too long
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 16 hex fraction digits need a denominator of 2^64.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    constexpr auto x = 0x0.0000000000000001p0_r;
    (void)x;
}
