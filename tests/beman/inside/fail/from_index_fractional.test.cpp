// EXPECT: takes a notch index
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A notch index is an integer.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    using halves = inside<{{0, 5}, 0.5}>;
    (void)halves::from_index(inside<{{0, 1}, 0.5}>{0.5});
}
