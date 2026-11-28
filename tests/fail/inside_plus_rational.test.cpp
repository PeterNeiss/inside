// EXPECT: an inside cannot be combined with a rational
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Through operator rational() an inside plus a rational would become rational
// arithmetic, an expected<rational, error> without a grid; it is deleted, with
// the spelling that keeps the grid (0.5_q, "1/3"_q).
#include <inside/inside.hpp>

using namespace ins;

using ins::detail::operator""_r; // the library's own literal

int main() {
    id<"[0..10]"> x{3};
    auto            y = x + 0.5_r;
    (void)y;
}
