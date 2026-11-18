// EXPECT: literal: p exponent too large
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 2^100 passes the 64-bit numerator.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    using ins::detail::operator""_r; // the library's own literal
    constexpr auto x = 0x1p100_r;
    (void)x;
}
