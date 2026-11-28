// EXPECT: units: the result may fall outside this quantity's number
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A quantity's += the grids cannot prove names its channel, as an inside's.
#include <inside/units.hpp>

using namespace ins;

int main() {
    id<"[0..100] [m]"> d{50, si::metre};
    d += 1_q * si::metre;
}
