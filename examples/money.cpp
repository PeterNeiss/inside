// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Currency arithmetic with cents precision, in a currency of your own.
// Dollars are a base dimension (USD, and the cent ¢ = 1/100 USD), so an
// amount never adds to a plain number or a length. A 0.01 notch over
// [0, 1'000'000] USD gives exact two-decimal storage in uint32 — no float
// drift on running totals. `sum` adds the line items with one range check,
// and `mul_into` rounds the 8 % tax once onto the cent.

#include <algorithm>
#include <iostream>
#include <vector>

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units/io.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
inline constexpr base_dimension dollar_dim{"dollar"};
inline constexpr quantity_spec  dollars_def{"dollars", dollar_dim};
inline constexpr named_unit     usd_def{"USD", spec_c<dollars_def>};
inline constexpr auto           USD = unit_c<usd_def>;
inline constexpr named_unit     cent_def{"¢", mag<rational{1, 100}> * USD, {.ascii = "c"}};
inline constexpr auto           cent = unit_c<cent_def>;
} // namespace

int main() {
    // 0..1,000,000 USD in 1¢ steps -> 100M+1 steps fits uint32.
    // 0.01 is not exact in binary, so the notch is given as an exact rational.
    using money = quantity<USD, inside<{{0, 1'000'000}, per<100>}, round_nearest>>;
    static_assert(sizeof(money) == 4);

    // Running total: line items add exactly, no float drift.
    const std::vector<money> items    = {money{19.99}, money{4.50}, money{12.34}, money{0.99}, money{7.25}};
    const money              subtotal = sum<money>(items);
    std::cout << "subtotal: " << subtotal << "\n";

    // Apply 8 % sales tax: the exact product, rounded once to the nearest 1¢
    // by `money`'s `round_nearest`. USD·% is USD/100, folded into the rounding.
    const auto  rate = just<8> * percent;
    const money tax  = mul_into<money>(subtotal, rate);
    std::cout << "tax (" << rate << "): " << tax << "\n";

    const money total = subtotal + tax;
    std::cout << "total:    " << total << " = " << total.in(cent) << "\n";

    // Issue a refund, capped at the total. `std::min` works on two `money`
    // values directly via the quantity's spaceship operator.
    const money requested_refund{100.00};
    const money refund = std::min(requested_refund, total);
    std::cout << "refund (req " << requested_refund << ", cap " << total << "): " << refund << "\n";

    // A tip in cents adds to dollars exactly, in cents.
    const auto with_tip = total + just<50> * cent;
    std::cout << "with tip: " << with_tip << "\n";

    if (to_string(subtotal) != "45.07 USD" || to_string(tax) != "3.61 USD" || to_string(total.in(cent)) != "4868 ¢" ||
        refund != total || to_string(with_tip) != "4918 ¢")
        return 1;
    return 0;
}
