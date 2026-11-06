// EXPECT: result out of the 64-bit range
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A rational's arithmetic is compile time only: an overflow of its 64-bit
// fields is a compile error, not an error at run time.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    constexpr auto     mx = std::numeric_limits<std::int64_t>::max();
    constexpr rational a{1u, mx}, b{1u, mx - 1};
    constexpr rational c = a + b; // 1/M + 1/(M-1): the denominator passes 2^63
    (void)c;
}
