// EXPECT: names the output's notch
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A template argument of a math call is the output's notch (per<K>), a policy
// flag, or the output type: an integer is none of them.
#include <beman/inside/math.hpp>

using namespace beman::inside;

int main() { (void)math::exp<2>(inside<{{1, 16}, per<64>}>{2}); }
