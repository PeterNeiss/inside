// EXPECT: literal: e exponent denominator overflow
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 10^-20 needs a denominator past 2^63.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    constexpr auto x = 1e-20_r;
    (void)x;
}
