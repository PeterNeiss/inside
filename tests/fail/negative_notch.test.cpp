// EXPECT: the notch must be non-negative
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A grid decodes as Lower + raw·Notch, so a negative notch would count
// downward; grid::validate rejects it.
#include <inside/inside.hpp>

using namespace ins;

int main() {
    id<key{grid{{0, 10}, -2}}> x{0};
    (void)x;
}
