// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Currency arithmetic with cents precision.
// A 0.01 notch over [0, 1'000'000] gives exact two-decimal storage in uint32 —
// no float drift on running totals. `sum` adds the line items with one range
// check, and `mul_into` rounds the tax once onto the cent.

#include <iostream>
#include <vector>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

int main() {
    // 0..$1,000,000 in 1¢ steps -> 100M+1 steps fits uint32.
    // 0.01 is not exact in binary, so the notch is given as an exact rational.
    using money = inside<{{0, 1'000'000}, per<100>}, round_nearest>;
    static_assert(sizeof(money) == 4);

    // Running total: line items add exactly, no float drift.
    std::vector<money> items    = {19.99, 4.50, 12.34, 0.99, 7.25};
    const money        subtotal = sum<money>(items);
    std::cout << "subtotal: $" << subtotal << "\n";

    // Apply 8% sales tax: the exact product, rounded once to the nearest 1¢
    // by `money`'s `round_nearest`.
    const money tax = mul_into<money>(subtotal, 0.08_ins);
    std::cout << "tax (8%): $" << tax << "\n";

    money total = subtotal + tax;
    std::cout << "total:    $" << total << "\n";

    // Issue a refund, capped at the total. `std::min` works on two `money`
    // values directly via the inside's spaceship operator.
    const money requested_refund{100.00};
    const money refund = std::min(requested_refund, total);
    std::cout << "refund (req $" << requested_refund << ", cap $" << total << "): $" << refund << "\n";

    return 0;
}
