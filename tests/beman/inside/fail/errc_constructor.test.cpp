// EXPECT: was removed: use
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Fallible construction is try_make (std::expected); the former
// `inside(value, error&)` constructor names the replacement.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    error            ec{};
    inside<{0, 100}> x(150, ec);
    (void)x;
}
