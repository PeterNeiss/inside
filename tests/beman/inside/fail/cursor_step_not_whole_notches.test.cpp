// EXPECT: the step must be a whole number of notches of T
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// cursor<T, Step> checks its step against T at compile time.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    using bad = cursor<inside<{{0, 1}, per<4>}>, rational{1, 8}>;
    bad b;
    (void)b;
}
