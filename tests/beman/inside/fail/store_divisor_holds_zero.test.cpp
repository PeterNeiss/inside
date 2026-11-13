// EXPECT: the divisor's grid holds 0
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// /= by a divisor whose grid holds 0 names a channel for the zero.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    inside<{0, 100}, round_toward_zero> x{50};
    const inside<{0, 10}>               d{2};
    x /= d;
}
