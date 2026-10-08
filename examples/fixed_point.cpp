// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Fixed point as grids: a notch of 1/2^n is a Qm.n format, any other notch
// (0.5 °C, 0.25 steps) works the same way, and the library does the scaling —
// no shifts, no hand-tracked radix point. Products land on a finer grid, so
// they are exact; storing back onto a coarse grid rounds by the policy.

#include <iomanip>
#include <iostream>

#include <beman/inside/formats.hpp>
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;
using beman::inside::detail::rational;

int main() {
    // Q-format reference: the predefined formats and two custom ones.
    using q1_7        = inside<{{0, 1}, per<128>}>; // Q1.7 in [0, 1]
    using half_signed = inside<{{-50, 50}, 0.5}>;   // 201 half steps, offset-encoded
    static_assert(sizeof(q4_4) == 1 && sizeof(q8_8) == 2 && sizeof(q16_16) == 4);
    static_assert(sizeof(q1_7) == 1 && sizeof(half_signed) == 1);

    std::cout << std::left << std::setw(13) << "format" << std::setw(8) << "bytes" << "x + x\n";
    auto row = [](const char* name, auto x) {
        std::cout << std::left << std::setw(13) << name << std::setw(8) << sizeof(x) << x + x << "\n";
    };
    row("Q4.4", q4_4{3.25});
    row("Q1.7", q1_7{0.75});
    row("Q8.8", q8_8{42.5});
    row("Q16.16", q16_16{1000.125});
    row("half steps", half_signed{-12.5});

    // Sums and products are exact: the product of two Q8.8 lives on 1/65536.
    const q8_8 a{3.5}, b{7.25};
    std::cout << "\n3.5 + 7.25 = " << a + b << ", 3.5 * 7.25 = " << a * b << "\n";
    if (rational{a * b} != rational{203, 8}) // 25.375
        return 1;

    // Storing back onto the 0.5 °C grid rounds by the type's policy.
    using celsius = inside<{{-40, 60}, 0.5}, round_nearest>;
    const celsius room{21.4}, body{37};
    std::cout << "room " << room << " C, body - room " << body - room << " C\n";
    if (room != celsius{21.5} || rational{body - room} != rational{31, 2})
        return 1;
    return 0;
}
