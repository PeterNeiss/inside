// EXPECT: of different kinds
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Hz and Bq are both 1/s, but separate kinds: they do not add.
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto s = just<1> * si::hertz + just<1> * si::becquerel;
    (void)s;
}
