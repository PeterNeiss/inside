// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Division: exact by default, integer division on request, and std::expected
// only when the divisor's grid holds zero.
//   - `a / b` is the exact rational quotient.
//   - `snap` (per call, `div(a, b, snapped)`, or in the type) divides natively;
//     a rounding flag picks the direction (`rounded_nearest`, `rounded_floor`, ...).
//   - A divisor grid that holds 0 makes the result std::expected<inside, errc>
//     (errc::division_by_zero); a zero-free divisor gives a plain value.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;
using beman::inside::detail::rational;

int main() {
    using val = inside<{0, 100}>;
    const val a{7}, b{3};

    // Exact rational quotient; b's grid holds 0, so it is an expected.
    const auto exact = a / b;
    std::cout << "7 / 3 (exact)          = " << *exact << "\n";
    if (rational{*exact} != rational{7, 3})
        return 1;

    // Integer division, per call: truncates like C++ `/`, or rounds.
    const auto trunc   = div(a, b, snapped);
    const auto nearest = div(val{22}, val{7}, rounded_nearest);
    const auto ceil    = div(val{22}, val{7}, rounded_ceil);
    std::cout << "7 / 3 (snapped)        = " << *trunc << "\n"
              << "22 / 7 (nearest, ceil) = " << *nearest << ", " << *ceil << "\n";
    if (*trunc != 2 || *nearest != 3 || *ceil != 4)
        return 1;

    // In the type: operator/ divides natively; the remainder follows the
    // rounded quotient, so (a / b) * b + a % b == a under every mode.
    using floored = inside<{-100, 100}, round_floor>;
    const auto fq = floored{-8} / floored{3};
    const auto fr = floored{-8} % floored{3};
    std::cout << "-8 / 3, -8 % 3 (floor) = " << *fq << ", " << *fr << "\n";
    if (*fq != -3 || *fr != 1)
        return 1;

    // Division by zero is a value, not a crash.
    const auto by_zero = val{10} / val{0};
    std::cout << "10 / 0                 = " << errc_message(by_zero.error()) << "\n";
    if (by_zero || by_zero.error() != errc::division_by_zero)
        return 1;

    // A divisor grid without 0 cannot fail: a plain inside, nothing to unwrap.
    using pos        = inside<{1, 10}>;
    const auto plain = val{42} / pos{4};
    static_assert(insidable<decltype(plain)>);
    std::cout << "42 / 4 (zero-free)     = " << plain << "\n";
    if (rational{plain} != rational{21, 2})
        return 1;
    return 0;
}
