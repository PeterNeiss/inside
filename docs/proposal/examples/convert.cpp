// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour: a conversion names what it does with a value that does not fit.
#include <beman/inside/inside.hpp>

#include <expected>

using namespace beman::inside;

int main() {
    // [listing]
    using level  = inside<{0, 10}>;
    using sensor = inside<{0, 1000}>;
    sensor raw   = 725;

    static_assert(conversion_overflows<level>(725));
    auto top  = level{raw, clamp};                      // 10: saturated
    auto deg  = inside<{0, 359}>{raw, wrap};            // 5: 725 mod 360
    auto fit  = exact_cast<level>(sensor{7});              // expected<level, error>: 7
    auto near = level{6.5, clamp | round_half_away};       // 7: a tie goes away from zero

    using quarter = inside<{{0, 10}, per<4>}>;
    quarter q = 2.75_ins;
    auto num  = q.numerator();                               // 11: the value is 11/4
    auto i    = q.to<int>();                                 // unexpected(error::rounding_error)
    auto down = q.to<int, round_floor>();                    // 2: rounding named in the call
    auto neg  = inside<{-5, 5}>{-1}.to<unsigned>();          // unexpected(error::overflow)
    // [/listing]
    if (top != 10 || deg != 5 || *fit != 7 || near != 7)
        return 1;
    if (num != 11 || q.denominator() != 4 || i.has_value() || i.error() != error::rounding_error || down != 2 || neg.has_value() || neg.error() != error::overflow)
        return 1;
    return 0;
}
