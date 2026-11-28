// EXPECT: the step must be a whole number of notches of T
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// cursor<T, Step> checks its step against T at compile time.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    using bad = cursor<id<"[0..1] 1/4">, detail::rational{1, 8}>;
    bad b;
    (void)b;
}
