// EXPECT: the rounded integer is past int64
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// trunc of a rational is an int64: a rational's integer part reaches
// 2^64 - 1, and one past int64 raises overflow (here while compiling), not a
// wrapped value.
#include <inside/inside.hpp>

int main() {
    using ins::detail::rational; // the library's own exact fraction
    constexpr auto t = trunc(rational{18446744073709551615u});
    (void)t;
}
