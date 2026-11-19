// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Money to the cent in a currency of your own: USD is a base dimension (and the cent ¢ = 1/100 USD), so an amount
// never adds to a plain number, and running totals have no float drift. A tax rounds once onto the cent.

#include <algorithm>
#include <iostream>
#include <vector>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;

namespace {
inline constexpr auto USD = base_unit<"USD", "dollar">;
inline constexpr auto cent = unit<"¢", 0.01_ins * USD, {.ascii = "c"}>;
} // namespace

int main() {
    // 0 … 1,000,000 USD in exact steps of 1/100 (0.01 is no binary fraction): 100M+1 steps fit a uint32.
    using dollars = quantity<USD, inside<{{0, 1'000'000}, per<100>}, round_half_away>>;
    static_assert(sizeof(dollars) == 4);

    // The line items add exactly; sum<dollars> checks the total once (an expected: the count is a run-time number).
    const std::vector<dollars> items = {
        dollars{19.99, USD}, dollars{4.50, USD}, dollars{12.34, USD}, dollars{0.99, USD}, dollars{7.25, USD}};
    const auto checked_subtotal = sum<dollars>(items);
    if (!checked_subtotal)
        return 1;
    const dollars subtotal = *checked_subtotal;
    std::cout << "subtotal: " << subtotal << "\n";

    // 8 % sales tax: the exact product, rounded once to the cent by dollars' round_half_away (USD·% is USD/100).
    const auto rate = just<8> * percent;
    const dollars tax = mul<dollars>(subtotal, rate);
    std::cout << "tax (" << rate << "): " << tax << "\n";

    const auto checked_total = value_cast<dollars>(subtotal + tax); // may pass 1,000,000 USD: stored with a check
    if (!checked_total)
        return 1;
    const dollars total = *checked_total;
    std::cout << "total:    " << total << " = " << total.in(cent) << "\n";

    // A refund capped at the total: std::min compares two quantities directly.
    const dollars requested_refund{100.00, USD};
    const dollars refund = std::min(requested_refund, total);
    std::cout << "refund (req " << requested_refund << ", cap " << total << "): " << refund << "\n";

    const auto with_tip = total + just<50> * cent; // a tip in cents adds to dollars exactly, in cents
    std::cout << "with tip: " << with_tip << "\n";

    if (to_string(subtotal) != "45.07 USD" || to_string(tax) != "3.61 USD" || to_string(total.in(cent)) != "4868 ¢"
        || refund != total || to_string(with_tip) != "4918 ¢")
        return 1;
    return 0;
}
