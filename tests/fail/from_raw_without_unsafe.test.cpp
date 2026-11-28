// EXPECT: from_raw is unchecked and says so
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// from_raw validates nothing, so the call names unsafe.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    auto x = id<"[0..10]">::from_raw(200);
    (void)x;
}
