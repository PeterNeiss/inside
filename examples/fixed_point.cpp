// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Fixed point as grids: a notch of 1/2^n is a Qm.n format, any other notch works the same way, and the library
// does the scaling. Products land on a finer grid, so they are exact; storing back rounds by the policy.

#include <iomanip>
#include <iostream>

#include <inside/inside/formats.hpp>
#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;
using namespace ins::formats;

int main() {
    // The predefined Q formats and two of our own.
    using q1_7 = inside<{{0, 1}, per<(1 << 7)>}>; // Q1.7 in [0, 1]
    using half_steps = inside<{{-50, 50}, 0.5}>;  // 201 half steps, offset-encoded
    static_assert(sizeof(uq4_4) == 1 && sizeof(uq8_8) == 2 && sizeof(uq16_16) == 4);
    static_assert(sizeof(q1_7) == 1 && sizeof(half_steps) == 1);

    std::cout << std::left << std::setw(13) << "format" << std::setw(8) << "bytes" << "x + x\n";
    auto row = [](const char* name, auto x) {
        std::cout << std::left << std::setw(13) << name << std::setw(8) << sizeof(x) << x + x << "\n";
    };
    row("Q4.4", uq4_4{3.25});
    row("Q1.7", q1_7{0.75});
    row("Q8.8", uq8_8{42.5});
    row("Q16.16", uq16_16{1000.125});
    row("half steps", half_steps{-12.5});

    // Sums and products are exact: the product of two Q8.8 lives on 1/65536.
    const uq8_8 a{3.5}, b{7.25};
    std::cout << "\n3.5 + 7.25 = " << a + b << ", 3.5 * 7.25 = " << a * b << "\n";
    if (a * b != just<203, 8>) // 25.375
        return 1;

    // Back onto Q8.8: mul<uq8_8> rounds once, by the mode given (an expected: the product can pass Q8.8's range).
    const uq8_8 gain{just<257, (1 << 8)>}; // 1.00390625
    const auto scaled = mul<uq8_8>(b, gain, round_half_away);
    if (!scaled)
        return 1;
    std::cout << "7.25 * 1.00390625 = " << b * gain << " -> Q8.8 " << *scaled << "\n";
    if (*scaled != just<1863, (1 << 8)>) // 1863.25/256 rounds to 1863/256
        return 1;

    // A 0.5 ℃ grid with its unit: a reading is a point on the ℃ scale, and body − room is a difference, 15.5 ℃.
    using celsius = inside<{{-40, 60}, 0.5}, round_half_away>;
    const auto room = point<si::degree_Celsius>(celsius{21.4}); // stored by the type's rounding: 21.5
    const auto body = point<si::degree_Celsius>(celsius{37});
    std::cout << "room " << room << ", body - room " << body - room << " = " << (body - room).in(si::kelvin) << "\n";
    if (to_string(room) != "21.5 ℃" || body - room != just<31, 2> * si::kelvin)
        return 1;
    return 0;
}
