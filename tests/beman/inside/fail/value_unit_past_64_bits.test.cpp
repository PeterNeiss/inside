// EXPECT: grid: the value unit gcd
// REQUIRES: 64-bit-grids
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Lower, Upper and the notch fit, but every value is a multiple of
// gcd(1/5, 2^-61) = 1/(5·2^61), past the 64-bit rational. The grid says so
// itself instead of failing deep in the storage. C++26 big grids compile it.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

using odd = inside<{{rational{1, 5}, rational{6, 5}}, rational{1, 1ll << 61}}>;
odd make() { return odd{}; }
