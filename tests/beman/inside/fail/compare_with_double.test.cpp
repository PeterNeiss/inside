// EXPECT: no comparison with a floating-point value
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 0.1 is not 1/10 in binary: an inside holding 1/10 would compare unequal to
// 0.1, so the comparison is deleted. Compare with 0.1_r or just<1, 10>.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    constexpr inside<{{0, 1}, per<10>}> x{just<1, 10>};
    return x == 0.1 ? 0 : 1;
}
