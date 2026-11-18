// EXPECT: Lower must not pass Upper
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An interval runs from Lower up to Upper.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    using ins::detail::operator""_r; // the library's own literal
    inside<grid{interval{10, 0}, 1_r}> x{5};
    (void)x;
}
