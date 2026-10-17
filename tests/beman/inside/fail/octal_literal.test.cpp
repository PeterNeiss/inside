// EXPECT: a leading 0 makes an octal literal
// REQUIRES: gnu-message
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// C++ reads 010 as eight; the literal grammar is decimal, so it would
// silently be ten: refused.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    auto x = 010_ins;
    (void)x;
}
