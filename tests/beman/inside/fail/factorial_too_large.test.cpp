// EXPECT: the result would pass 16384 bits
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The result's limit is computed while compiling: 2000! has 19053 bits, past
// math::detail::max_count_bits.
#include <beman/inside/inside.hpp>
#include <beman/inside/math.hpp>

int main() {
    using namespace beman::inside;
    (void)math::factorial(inside<{0, 2000}>{});
}
