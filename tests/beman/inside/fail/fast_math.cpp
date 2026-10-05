// EXPECT: does not support -ffast-math
// FLAGS: -ffast-math
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The library's exact and correctly rounded results rest on IEEE arithmetic
// as written; a -ffast-math build stops at the first include.
#include <beman/inside/inside.hpp>

int main() {}
