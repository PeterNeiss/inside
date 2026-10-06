// EXPECT: no 64-bit rational
// REQUIRES: 64-bit-grids
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// With 64-bit grid numbers a double limit of 2^64 or more has no rational form.
// C++26 big grids take it exactly.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

inside<{0, 0x1p100}> huge;
