// EXPECT: _g literal: past the 64-bit grid numbers
// REQUIRES: 64-bit-grids
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// With 64-bit grid numbers a _g literal must fit a 64-bit rational; 2^100 does
// not. C++26 big grids take it.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

inside<{0, 1267650600228229401496703205376_g}> huge;
