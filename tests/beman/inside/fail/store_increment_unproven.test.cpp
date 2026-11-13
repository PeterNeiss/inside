// EXPECT: may step out of this grid
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// ++ on a checked grid may step past Upper: it names a channel.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    inside<{0, 9}> i{0};
    ++i;
}
