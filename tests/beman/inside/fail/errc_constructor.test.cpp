// EXPECT: was removed: use
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Fallible construction is try_make (std::expected); the former
// `inside(value, errc&)` constructor names the replacement.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    errc             ec{};
    inside<{0, 100}> x(150, ec);
    (void)x;
}
