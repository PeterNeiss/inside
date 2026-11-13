// EXPECT: takes one notch
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A math call names at most one notch for its output.
#include <beman/inside/math.hpp>

using namespace beman::inside;

int main() { (void)math::sin<per<64>, per<128>>(inside<{{-1, 1}, per<64>}, round_nearest>{0.5}); }
