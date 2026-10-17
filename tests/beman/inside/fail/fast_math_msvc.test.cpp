// EXPECT: /fp:fast
// REQUIRES: msvc
// FLAGS: /fp:fast
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// MSVC's /fp:fast lets the compiler reassociate and assume no NaN, as
// -ffast-math does; a /fp:fast build stops at the first include.
#include <beman/inside/inside.hpp>

int main() {}
