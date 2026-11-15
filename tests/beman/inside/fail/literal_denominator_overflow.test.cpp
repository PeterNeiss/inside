// EXPECT: literal: denominator overflow
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 10^-22 needs a denominator past 2^63.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    using beman::inside::detail::operator""_r; // the library's own literal
    constexpr auto x = 0.0000000000000000000001_r;
    (void)x;
}
