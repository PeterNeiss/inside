// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Division: exact by default, integer division on request, and std::expected
// only when the divisor's grid holds zero.
//   - `a / b` is the exact rational quotient.
//   - `snap` (per call, `div(a, b, rounded_toward_zero)`, or in the type) divides natively;
//     a rounding flag picks the direction (`rounded_nearest`, `rounded_floor`, ...).
//   - A divisor grid that holds 0 makes the result std::expected<inside, errc>
//     (errc::division_by_zero); a zero-free divisor gives a plain value.
//   - A quotient's type bounds its denominators, so arithmetic on it stays
//     exact and plain: its precision is known at compile time.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

int main() {
    using number = inside<{0, 100}>;
    const number a{7}, b{3};

    // Exact rational quotient; b's grid holds 0, so it is an expected.
    const auto exact = a / b;
    std::cout << "7 / 3 (exact)          = " << *exact << "\n";
    if (*exact != just<7, 3>)
        return 1;

    // Integer division, per call: truncates like C++ `/`, or rounds.
    const auto trunc   = div(a, b, rounded_toward_zero);
    const auto nearest = div(number{22}, number{7}, rounded_nearest);
    const auto ceil    = div(number{22}, number{7}, rounded_ceil);
    std::cout << "7 / 3 (toward zero)    = " << *trunc << "\n"
              << "22 / 7 (nearest, ceil) = " << *nearest << ", " << *ceil << "\n";
    if (*trunc != 2 || *nearest != 3 || *ceil != 4)
        return 1;

    // In the type: `/` and `%` divide natively, and divmod gives both from one
    // division. The remainder follows the rounded quotient, so
    // q * b + r == a under every mode.
    using floored       = inside<{-100, 100}, round_floor>;
    const auto [fq, fr] = *divmod(floored{-8}, floored{3});
    std::cout << "-8 / 3, -8 % 3 (floor) = " << fq << ", " << fr << "\n";
    if (fq != -3 || fr != 1 || *(floored{-8} / floored{3}) != fq)
        return 1;

    // Division by zero is a value, not a crash.
    const auto by_zero = number{10} / number{0};
    std::cout << "10 / 0                 = " << errc_message(by_zero.error()) << "\n";
    if (by_zero || by_zero.error() != errc::division_by_zero)
        return 1;

    // A divisor grid without 0 cannot fail: a plain inside, nothing to unwrap.
    using positive   = inside<{1, 10}>;
    const auto plain = number{42} / positive{4};
    static_assert(insidable<decltype(plain)>);
    std::cout << "42 / 4 (zero-free)     = " << plain << "\n";
    if (plain != just<21, 2>)
        return 1;

    // The quotient's denominators are at most 1·max|positive| = 10 (x = p/a over
    // y = r/b is p·b/(a·r)): the type says so, and what is computed from it
    // knows its own bound — exact, and still nothing to unwrap.
    using per_item       = decltype(number{} / positive{});
    const per_item price = number{20} / positive{3}; // 20/3 per item
    const auto     total = price * positive{7} + price / positive{2};
    static_assert(grid_of<per_item>.MaxDenominator == 10);
    static_assert(insidable<decltype(total)>);
    std::cout << "20/3 * 7 + 20/3 / 2    = " << total << "\n"; // 140/3 + 10/3
    if (total != 50)
        return 1;
    return 0;
}
