// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Which raw an inside stores, always deduced from the grid: the smallest integer that numbers every point, native
// widths from <inside/inside/formats.hpp>, and a fixed wire layout written with to<T>().

#include <cstdint>
#include <iostream>

#include <inside/inside/formats.hpp>
#include <inside/inside.hpp>
#include <inside/inside/io.hpp> // to_string_debug: value, raw and raw type

using namespace ins;
using namespace ins::formats;

int main() {
    // 1. Deduced storage: the value itself where a whole-number range costs no width, else the 0-based index.
    using percent = inside<{0, 100}>;          // uint8_t, raw == value
    using register_value = inside<{5, 100}>;   // uint8_t, raw == value (no wider than the index)
    using offset = inside<{200, 300}>;         // uint8_t, raw == value − 200 (300 needs 16 bits)
    using temperature = inside<{-40, 85}>;     // int8_t, raw == value (signed, notch 1)
    using altitude = inside<{-500, 9000}>;     // int16_t
    using half_steps = inside<{{-5, 5}, 0.5}>; // uint8_t index: a fractional notch is offset-encoded
    static_assert(sizeof(percent) == 1 && sizeof(temperature) == 1 && sizeof(altitude) == 2
                  && sizeof(half_steps) == 1);
    std::cout << to_string_debug(offset{242}) << "\n" << to_string_debug(temperature{-12}) << "\n";
    if (register_value{42}.raw() != 42 || offset{242}.raw() != 42 || temperature{-12}.raw() != -12
        || half_steps{-4.5}.raw() != 1)
        return 1;

    // Signed arithmetic stays exact and its result grid widens: no overflow.
    const auto climb = altitude{4500} - altitude{-200};
    std::cout << "climb " << climb << " m\n";
    if (climb != 4700)
        return 1;

    // 2. Presets at native widths that still carry a range and a policy.
    static_assert(sizeof(uint8) == 1 && sizeof(int16) == 2 && sizeof(unorm16) == 2 && sizeof(uq8_8) == 2
                  && sizeof(uq16_16) == 4);
    int16 mixed{0};
    mixed.with<clamp>() = int16{30000} + int16{20000};                             // a PCM mix: 50000 saturates
    const int16 out = mul<int16>(unorm16{0.5_ins}, int16{10001}, round_half_away); // a gain, rounded once
    std::cout << "mix " << mixed << ", gain " << out << "\n";
    if (mixed != 32767 || out != 5001)
        return 1;

    // 3. A fixed wire layout: the field's type is the wire's, the value checked.
    struct packet {
        std::uint16_t level; // 0..65535
    };
    using level = inside<{0, 1000}>;
    const packet p{level{750}}; // an index-like inside converts to an integer that holds its grid
    std::cout << "packet level " << p.level << "\n";
    if (p.level != 750)
        return 1;
    return 0;
}
