// EXPECT: T has no notch
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// cursor<T, Step> checks its step against T at compile time.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    using bad = cursor<inside<{{0, 1}, max_denominator{10}}>, 1>;
    bad b;
    (void)b;
}
