// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Which raw an inside stores, and how to choose it.
//   1. Deduced: the smallest integer that numbers every point — unsigned offset
//      encoding, or the value itself for a signed integer range.
//   2. Presets from <beman/inside/formats.hpp>: native byte widths (byte, sword,
//      unorm16, q8_8, …) that still carry a range and a policy.
//   3. Width flags (i8 … u64) pin the backing type for a fixed wire layout,
//      checked at compile time; add `indexed` for a notched grid.

#include <iostream>

#include <beman/inside/formats.hpp>
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp> // to_string_debug: value, raw and raw type

using namespace beman::inside;

int main() {
    // 1. Deduced storage.
    using pct      = inside<{0, 100}>;       // uint8_t, raw == value
    using offset   = inside<{5, 100}>;       // uint8_t, raw == value − 5
    using temp     = inside<{-40, 85}>;      // int8_t, raw == value (signed, notch 1)
    using altitude = inside<{-500, 9000}>;   // int16_t
    using half     = inside<{{-5, 5}, 0.5}>; // uint8_t index: a fractional notch is offset-encoded
    static_assert(sizeof(pct) == 1 && sizeof(temp) == 1 && sizeof(altitude) == 2 && sizeof(half) == 1);
    std::cout << to_string_debug(offset{42}) << "\n" << to_string_debug(temp{-12}) << "\n";
    if (offset{42}.raw() != 37 || temp{-12}.raw() != -12 || half{-4.5}.raw() != 1)
        return 1;

    // Signed arithmetic stays exact and its result grid widens: no overflow.
    const auto climb = altitude{4500} - altitude{-200};
    std::cout << "climb " << climb << " m\n";
    if (climb != 4700)
        return 1;

    // 2. Presets at native widths.
    static_assert(sizeof(byte) == 1 && sizeof(sword) == 2 && sizeof(unorm16) == 2 && sizeof(q8_8) == 2 &&
                  sizeof(q16_16) == 4);
    sword mixed{0};
    mixed.with_clamp() = sword{30000} + sword{20000}; // PCM mix: 50000 saturates
    sword out{0};
    out.with_snap<round_nearest>() = unorm16{0.5_ins} * sword{10001}; // gain, rounded back
    std::cout << "mix " << mixed << ", gain " << out << "\n";
    if (mixed != 32767 || out != 5001)
        return 1;

    // 3. Width flags: pin the type; value storage unless `indexed`.
    using field = inside<{0, 100}, u16>;                     // uint16_t, raw == value
    using reg   = inside<{5, 100}, u8>;                      // raw == value (not 37)
    using level = inside<{{0, 1}, per<256>}, u16 | indexed>; // uint16_t notch index
    static_assert(sizeof(field) == 2);
    std::cout << to_string_debug(reg{42}) << "\n" << to_string_debug(level{0.5_ins}) << "\n";
    if (reg{42}.raw() != 42 || level{0.5_ins}.raw() != 128)
        return 1;

    // Too small a type is a compile error, not a silent widening:
    //   inside<{0, 100000}, u8>            // the value range overflows uint8_t
    //   inside<{-200, 0}, u8>              // unsigned can't hold negatives
    //   inside<{0, 100}, u8 | u16>         // one width flag at a time
    //   inside<{{0, 1}, per<256>}, u16>    // value storage needs notch 1
    return 0;
}
