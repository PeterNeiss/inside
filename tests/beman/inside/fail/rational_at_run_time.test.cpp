// EXPECT: is not a constant expression
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A rational's arithmetic is compile time only, like a grid number's: at run
// time an exact value is an inside, whose grid bounds every result.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main(int argc, char**) {
    const rational a{argc, 3};
    const auto     b = a + rational{1, 3};
    (void)b;
}
