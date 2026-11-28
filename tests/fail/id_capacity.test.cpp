// EXPECT: an id's text passes 2048 characters
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A type whose text passes the key capacity (detail::key_capacity) has no name.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    // 2^9000 - 1: 2250 hexadecimal digits
    id<key{grid{0, grid_number{(detail::grid_int{1} << 9000) - detail::grid_int{1}}}}> x;
    (void)x;
}
