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
    auto top  = clamp_cast<level>(raw);                      // 10: saturated
    auto deg  = wrap_cast<inside<{0, 359}>>(raw);            // 5: 725 mod 360
    auto fit  = checked_cast<level>(sensor{7});              // 7, or inside_error
    auto near = clamp_cast<level, round_nearest>(6.5);       // 7: a tie goes away from zero

    using quarter = inside<{{0, 10}, per<4>}>;
    quarter q = 2.75_ins;
    auto num  = q.numerator();                               // 11: the value is 11/4
    auto i    = q.to<int>();                                 // 2: truncated toward zero
    auto neg  = inside<{-5, 5}>{-1}.to<unsigned>();          // unexpected(errc::domain_error)
    // [/listing]
    if (top != just<10> || deg != just<5> || fit != just<7> || near != just<7>)
        return 1;
    if (num != 11 || q.denominator() != 4 || i != 2 || neg.has_value() || neg.error() != errc::domain_error)
        return 1;
    return 0;
}
