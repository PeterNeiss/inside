// EXPECT: multiplies by a power of pi
// REQUIRES: delete-reason
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// degree → radian has no exact grid: .in() refuses it, value_cast rounds.
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto r = (just<90> * si::degree).in(si::radian);
    (void)r;
}
