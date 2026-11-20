// EXPECT: read a floating point with x.to<double>()
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An inside has no operator double: a double is read with to<double>(), which names its rounding.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    const inside<{{0, 1}, per<1024>}> x{just<1, 4>};
    const double d = static_cast<double>(x);
    (void)d;
}
