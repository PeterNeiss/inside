// EXPECT: a std::expected is not stored implicitly
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A division by a grid that holds 0 is a std::expected; an inside does not
// store it implicitly - check it and store *e, or use try_make.
#include <inside/inside.hpp>

using namespace ins;

int main() {
    id<"[0..10]">  a{4};
    id<"[0..2]">   b{1};
    id<"[0..100]"> c = a / b;
    (void)c;
}
