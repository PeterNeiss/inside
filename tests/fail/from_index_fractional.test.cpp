// EXPECT: takes a notch index
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A notch index is an integer.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    using halves = id<"[0..5] 0.5">;
    (void)halves::from_step(id<"[0..1] 0.5">{0.5});
}
