// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Ether amounts to the wei: a grid of 10^-18 ETH up to a billion ETH has 10^27
// points — more than 64 bits number — so the raw is a two-limb wide integer,
// and every amount, fee and total stays exact.
//   1. Parse amounts exactly from text; a double keeps only ~17 digits.
//   2. A fee is gas × price, exactly; an overdraft is reported, not wrapped.
//   3. A total over many accounts: sum adds the wide raws exactly.
//   4. Convert to dollars at $3141.59 per ETH: the exact product, rounded once
//      to the cent.

#include <array>
#include <format>
#include <iostream>
#include <string>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

inline constexpr rational wei{1, 1'000'000'000'000'000'000}; // 10^-18 ETH

using eth   = inside<{{0, 1'000'000'000}, wei}>;                             // one account
using total = inside<{{0, 1'000'000'000'000}, wei}>;                         // a thousand of them
using gas   = inside<{0, 30'000'000}>;                                       // units of work
using gwei  = inside<{{0, 10'000}, rational{1, 1'000'000'000}}>;             // price per unit, ETH
using price = inside<{{0, 1'000'000}, per<100>}>;                            // dollars per ETH, to the cent
using usd   = inside<{{0, 1'000'000'000'000'000}, per<100>}, round_nearest>; // dollars, to the cent

int main() {
    static_assert(sizeof(eth) == 16); // 10^27 + 1 points: two 64-bit limbs

    // 1. Exact parsing; the nearest double has lost the last ten digits.
    const std::string text    = "123456789.123456789123456789";
    const eth         balance = *from_chars<eth>(text);
    std::cout << "balance  " << balance << " ETH\n"
              << "double   " << std::format("{:.17g}", std::stod(text)) << "\n";
    if (to_string(balance) != text)
        return 1;

    // 2. 21000 gas at 30 gwei is exactly 0.00063 ETH.
    const auto fee = gas{21'000} * gwei{rational{30, 1'000'000'000}};
    std::cout << "fee      " << fee << " ETH\n";
    if (fee != just<rational{63, 100'000}>)
        return 1;

    const eth  alice = *from_chars<eth>("1.5");
    const auto spent = *from_chars<eth>("1.4999") + fee; // 1.50053: more than she has
    const auto after = eth::try_make(alice - spent);
    std::cout << "send     " << (after ? to_string(*after) : errc_message(after.error())) << "\n";
    if (after || after.error() != errc::overflow)
        return 1;

    // 3. A thousand accounts near the top of the grid.
    std::array<eth, 1000> accounts;
    accounts.fill(*from_chars<eth>("999999999.999999999999999999"));
    const total sum = beman::inside::sum<total>(accounts);
    std::cout << "sum      " << sum << " ETH\n";
    if (to_string(sum) != "999999999999.999999999999999")
        return 1;

    // 4. balance × price has a 10^-20 notch — a grid past 64-bit numbers —
    //    so round the exact product straight onto the cents grid.
    const usd value = mul_into<usd>(balance, *from_chars<price>("3141.59"));
    std::cout << "value    $" << value << "\n";
    if (to_string(value) != "387850614142.36")
        return 1;
    return 0;
}
