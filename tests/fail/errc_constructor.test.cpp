// EXPECT: was removed: use
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Fallible construction is try_make (std::expected); the former
// `inside(value, error&)` constructor names the replacement.
#include <inside/inside.hpp>

using namespace ins;

int main() {
    error            ec{};
    id<"[0..100]"> x(150, ec);
    (void)x;
}
