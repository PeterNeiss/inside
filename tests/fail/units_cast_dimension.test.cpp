// EXPECT: kind_cast - another dimension
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// kind_cast keeps the dimension: a length is no duration.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    auto t = kind_cast<isq::duration>(1_q * si::metre);
    (void)t;
}
