// EXPECT: an input with more than 2
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The math functions work in 64 bits: an input with more than 2^64 slots (or,
// under C++26, grid numbers past 64 bits) is rejected with one readable message.
#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

using namespace beman::inside;

using fine = inside<{{0, 16}, per<(1ull << 62)>}, round_nearest>;   // 2^66 slots
auto r = math::sqrt(fine{2.25});
