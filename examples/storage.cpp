// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Which raw an inside stores: always deduced from the grid.
//   1. The smallest integer that numbers every point — the value itself for a
//      whole-number range where that costs no width, else the 0-based index.
//   2. Presets from <beman/inside/formats.hpp>: native byte widths (byte, sword,
//      unorm16, q8_8, …) that still carry a range and a policy.
//   3. A fixed wire layout: write `to<T>()` into the field.

#include <cstdint>
#include <iostream>

#include <beman/inside/formats.hpp>
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp> // to_string_debug: value, raw and raw type

using namespace beman::inside;

int main() {
    // 1. Deduced storage.
    using pct      = inside<{0, 100}>;       // uint8_t, raw == value
    using reg      = inside<{5, 100}>;       // uint8_t, raw == value (no wider than the index)
    using offset   = inside<{200, 300}>;     // uint8_t, raw == value − 200 (300 needs 16 bits)
    using temp     = inside<{-40, 85}>;      // int8_t, raw == value (signed, notch 1)
    using altitude = inside<{-500, 9000}>;   // int16_t
    using half     = inside<{{-5, 5}, 0.5}>; // uint8_t index: a fractional notch is offset-encoded
    static_assert(sizeof(pct) == 1 && sizeof(temp) == 1 && sizeof(altitude) == 2 && sizeof(half) == 1);
    std::cout << to_string_debug(offset{242}) << "\n" << to_string_debug(temp{-12}) << "\n";
    if (reg{42}.raw() != 42 || offset{242}.raw() != 42 || temp{-12}.raw() != -12 || half{-4.5}.raw() != 1)
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

    // 3. A fixed wire layout: the field's type is the wire's, the value checked.
    struct packet {
        std::uint16_t level; // 0..65535
    };
    using level = inside<{0, 1000}>;
    const packet p{*level{750}.to<std::uint16_t>()};
    std::cout << "packet level " << p.level << "\n";
    if (p.level != 750)
        return 1;

    return 0;
}
