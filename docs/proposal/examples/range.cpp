// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour 1: the range is part of the type, and arithmetic widens it.
#include <inside/inside.hpp>

#include <type_traits>

using namespace ins;

int main() {
    // [listing]
    using percent = id<"[0..100]">;   // the integers 0..100, in one byte

    percent a = 42, b = 58;
    auto    sum  = a + b;               // id<"[0..200]">
    auto    diff = a - b;               // id<"[-100..100]">
    auto    neg  = -a;                  // id<"[-100..0]">: no -INT_MIN
    static_assert(std::is_same_v<decltype(sum), id<"[0..200]">>);
    static_assert(sizeof(percent) == 1);

    percent c{sum - 100_q, clamp};  // -100..100 into 0..100: the store names what happens
    auto    bad = percent::try_make(sum); // expected<percent, error>
    // [/listing]
    if (sum != 100 || diff != -16 || neg != -42 || c != 0 || !bad.has_value())
        return 1;
    return 0;
}
