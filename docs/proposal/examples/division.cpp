// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour 5: division is exact, and fallible only when the divisor may be zero.
#include <beman/inside/inside.hpp>

#include <expected>
#include <type_traits>

using namespace beman::inside;

int main() {
    // [listing]
    using num  = inside<{0, 100}>;
    using any  = inside<{0, 10}>;              // may be zero
    using some = inside<{1, 10}>;              // never zero

    auto q = num{7} / some{3};                 // exactly 7/3, a plain value
    auto r = num{7} / any{3};                  // std::expected<..., error>
    auto z = num{7} / any{0};                  // unexpected(error::division_by_zero)
    static_assert(!std::is_same_v<decltype(q), decltype(r)>);

    auto f = div(num{7}, some{3}, round_floor);  // 2: rounding named at the call
    auto m = mod(num{7}, some{3}, round_floor);  // 1, consistent: 2·3 + 1 == 7
    // [/listing]
    if (q != just<rational{7, 3}> || *r != just<rational{7, 3}> || z.has_value() ||
        z.error() != error::division_by_zero || f != 2 || m != 1)
        return 1;
    return 0;
}
