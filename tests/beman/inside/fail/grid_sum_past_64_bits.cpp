// EXPECT: the result grid exceeds the 64-bit grid numbers
// REQUIRES: 64-bit-grids
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// With 64-bit grid numbers the sum of two qwords has no result grid (its upper
// bound 2^65 − 2 passes the rational range). The addition says so itself,
// instead of a bare rational overflow. C++26 big grids compile it.
#include <beman/inside/inside.hpp>
#include <beman/inside/formats.hpp>

using namespace beman::inside;

auto sum(qword a, qword b) { return a + b; }
