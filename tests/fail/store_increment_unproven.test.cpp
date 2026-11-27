// EXPECT: may step out of this grid
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// ++ on a checked grid may step past Upper: it names a channel.
#include <inside/inside.hpp>

using namespace ins;

int main() {
    id<"[0..9]"> i{0};
    ++i;
}
