// EXPECT: takes one notch
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A math call names at most one notch for its output.
#include <inside/math.hpp>

using namespace ins;

int main() { (void)math::sin<per<64>, per<128>>(id<"[-1..1] 1/64 round_half_away">{0.5}); }
