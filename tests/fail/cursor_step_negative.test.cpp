// EXPECT: the step must be positive
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// cursor<T, Step> checks its step against T at compile time.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    using bad = cursor<id<"[0..10]">, -1>;
    bad b;
    (void)b;
}
