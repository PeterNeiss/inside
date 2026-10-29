// EXPECT: the step must divide the range of T evenly
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// cursor<T, Step> checks its step against T at compile time.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    using bad = cursor<inside<{0, 10}>, 3>;
    bad b;
    (void)b;
}
