// EXPECT: a std::expected is not stored implicitly
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A division by a grid that holds 0 is a std::expected; an inside does not
// store it implicitly - check it and store *e, or use try_make.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    inside<{0, 10}>  a{4};
    inside<{0, 2}>   b{1};
    inside<{0, 100}> c = a / b;
    (void)c;
}
