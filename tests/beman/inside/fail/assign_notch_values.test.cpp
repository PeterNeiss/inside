// EXPECT: lhs notch 1, rhs notch 1/2
// REQUIRES: valued-diagnostics
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Under C++26 the diagnostic names both notches: halves do not store into
// whole numbers without a rounding mode.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    inside<{0, 10}>                         a{1};
    const inside<{{0, 10}, rational{1, 2}}> b{1};
    a = b;
    (void)a;
}
