// EXPECT: no comparison with a floating-point value
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 0.1 is not 1/10 in binary: an inside holding 1/10 would compare unequal to
// 0.1, so the comparison is deleted. Compare with 0.1_r or (0.1_q).
#include <inside/inside.hpp>

using namespace ins;

int main() {
    constexpr id<"[0..1] 1/10"> x{0.1_q};
    return x == 0.1 ? 0 : 1;
}
