// EXPECT: literal: p exponent denominator overflow
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 3·2^-64 needs a denominator of 2^64.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    constexpr auto x = 0x1.8p-63_r;
    (void)x;
}
