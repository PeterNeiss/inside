// EXPECT: units: the result may fall outside this quantity's number
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A quantity's += the grids cannot prove names its channel, as an inside's.
#include <beman/inside/units.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

int main() {
    quantity<si::metre, inside<{0, 100}>> d{50, si::metre};
    d += just<1> * si::metre;
}
