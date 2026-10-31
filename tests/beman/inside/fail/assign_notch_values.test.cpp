// EXPECT: lhs notch 1, rhs notch 1/2
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// explain_assignable names both notches: halves do not store into whole
// numbers without a rounding mode. (The store itself is a deleted overload
// that names the clause; notch_mismatch_assign pins that message.)
#include <beman/inside/inside.hpp>

using namespace beman::inside;

static_assert(explain_assignable<inside<{0, 10}>, inside<{{0, 10}, rational{1, 2}}>>);

int main() {}
