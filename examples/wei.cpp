// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Ether amounts to the wei: a grid of 10^-18 ETH up to a billion ETH has 10^27
// points — more than 64 bits number — so the raw is a two-limb wide integer,
// and every amount, fee and total stays exact.
//   1. Parse amounts exactly from text; a double keeps only ~17 digits.
//   2. A fee is gas × price, exactly; an overdraft is reported, not wrapped.
//   3. A total over many accounts: wide raws add like any other.
//   4. Convert to dollars at a price in cents, rounded once, to the cent.

#include <array>
#include <format>
#include <iostream>
#include <string>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;
using beman::inside::detail::rational;

inline constexpr rational wei{1, 1'000'000'000'000'000'000}; // 10^-18 ETH

using eth   = inside<{{0, 1'000'000'000}, wei}>;                   // one account
using total = inside<{{0, 1'000'000'000'000}, wei}>;               // a thousand of them
using gas   = inside<{0, 30'000'000}>;                             // units of work
using gwei  = inside<{{0, 10'000}, rational{1, 1'000'000'000}}>;   // price per unit, ETH
using quote = inside<{0, 100'000'000}>;                            // cents per ETH
using cents = inside<{0, 100'000'000'000'000'000}, round_nearest>; // a whole number of cents

int main() {
    static_assert(sizeof(eth) == 16); // 10^27 + 1 points: two 64-bit limbs

    // 1. Exact parsing; the nearest double has lost the last ten digits.
    const std::string text    = "123456789.123456789123456789";
    const eth         balance = *from_chars<eth>(text);
    std::cout << "balance  " << balance << " ETH\n"
              << "double   " << std::format("{:.17g}", std::stod(text)) << "\n";
    if (balance != *from_chars<eth>("123456789.123456789123456789"))
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
    total sum{0};
    for (const eth& a : accounts)
        sum += a;
    std::cout << "sum      " << sum << " ETH\n";
    if (sum != *from_chars<total>("999999999999.999999999999999"))
        return 1;

    // 4. At $3141.59 (314159 cents per ETH): the exact product rounds once, to
    //    a whole cent; dividing by 100 is exact on the cents grid.
    const cents value{balance * quote{314'159}};
    std::cout << "value    $" << value * just<rational{1, 100}> << "\n";
    if (value != 38'785'061'414'236)
        return 1;
    return 0;
}
