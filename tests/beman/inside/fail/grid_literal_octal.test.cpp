// EXPECT: _g/_ins literal: a leading 0 makes an octal literal
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A leading 0 would make an octal literal: the _g literal rejects it.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    constexpr auto x = 010_g;
    (void)x;
}
