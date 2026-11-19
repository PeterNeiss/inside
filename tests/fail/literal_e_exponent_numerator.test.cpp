// EXPECT: literal: e exponent numerator overflow
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 10^20 passes the 64-bit numerator.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    using ins::detail::operator""_r; // the library's own literal
    constexpr auto x = 1e20_r;
    (void)x;
}
