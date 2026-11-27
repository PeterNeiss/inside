// EXPECT: is not a constant expression
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An int may not fit id<"[0..100]">: a run-time int does not construct
// one (a constant does, checked while compiling); try_make reports instead.
#include <inside/inside.hpp>

using namespace ins;

int main(int argc, char**) {
    id<"[0..100]"> x{argc};
    (void)x;
}
