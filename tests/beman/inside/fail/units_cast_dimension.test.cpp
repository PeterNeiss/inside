// EXPECT: quantity_cast - another dimension
// REQUIRES: delete-reason
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// quantity_cast keeps the dimension: a length is no duration.
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto t = quantity_cast<isq::duration>(just<1> * si::metre);
    (void)t;
}
