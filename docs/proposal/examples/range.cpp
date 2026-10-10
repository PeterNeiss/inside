// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour 1: the range is part of the type, and arithmetic widens it.
#include <beman/inside/inside.hpp>

#include <type_traits>

using namespace beman::inside;

int main() {
    // [listing]
    using percent = inside<{0, 100}>;   // the integers 0..100, in one byte

    percent a = 42, b = 58;
    auto    sum  = a + b;               // inside<{0, 200}>
    auto    diff = a - b;               // inside<{-100, 100}>
    auto    neg  = -a;                  // inside<{-100, 0}>: no -INT_MIN
    static_assert(std::is_same_v<decltype(sum), inside<{0, 200}>>);
    static_assert(sizeof(percent) == 1);

    percent c = sum - just<100>;        // a checked store: 0..100 fits
    auto    bad = percent::try_make(sum); // expected<percent, errc>
    // [/listing]
    if (sum != just<100> || diff != just<-16> || neg != just<-42> || c != just<0> || !bad.has_value())
        return 1;
    return 0;
}
