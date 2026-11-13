// EXPECT: is not a constant expression
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An int may not fit inside<{0, 100}>: a run-time int does not construct
// one (a constant does, checked while compiling); try_make reports instead.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main(int argc, char**) {
    inside<{0, 100}> x{argc};
    (void)x;
}
