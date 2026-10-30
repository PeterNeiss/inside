// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Ether amounts to the wei: a grid of 10^-18 ETH up to a billion ETH has 10^27
// points — more than 64 bits number — so the raw is a two-limb wide integer,
// and every amount, fee and total stays exact. Ether, gas and dollars are
// base dimensions of their own: ETH, gwei and wei are units of one, a gas
// price is gwei/gas, and an exchange rate is USD/ETH, never a plain number.
//   1. Parse amounts exactly from text; a double keeps only ~17 digits.
//   2. A fee is gas × price, exactly; an overdraft is reported, not wrapped.
//   3. A total over many accounts: sum adds the wide raws exactly.
//   4. Convert to dollars at 3141.59 USD/ETH: the exact product, rounded once
//      to the cent.

#include <array>
#include <format>
#include <iostream>
#include <string>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units_io.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
inline constexpr base_dimension ether_dim{"ether"};
inline constexpr quantity_spec  ether_def{"ether", ether_dim};
inline constexpr named_unit     eth_def{"ETH", spec_c<ether_def>};
inline constexpr auto           ETH = unit_c<eth_def>;
inline constexpr named_unit     gwei_def{"gwei", mag<rational{1, 1'000'000'000}> * ETH};
inline constexpr auto           gwei = unit_c<gwei_def>;
inline constexpr named_unit     wei_def{"wei", mag<rational{1, 1'000'000'000'000'000'000}> * ETH};
inline constexpr auto           wei = unit_c<wei_def>;

inline constexpr base_dimension work_dim{"gas"};
inline constexpr quantity_spec  work_def{"gas", work_dim};
inline constexpr named_unit     gas_def{"gas", spec_c<work_def>};
inline constexpr auto           gas = unit_c<gas_def>;

inline constexpr base_dimension dollar_dim{"dollar"};
inline constexpr quantity_spec  dollars_def{"dollars", dollar_dim};
inline constexpr named_unit     usd_def{"USD", spec_c<dollars_def>};
inline constexpr auto           USD = unit_c<usd_def>;
} // namespace

inline constexpr auto to_the_wei = per<1'000'000'000'000'000'000>;
using eth                        = quantity<ETH, inside<{{0, 1'000'000'000}, to_the_wei}>>;     // one account
using total                      = quantity<ETH, inside<{{0, 1'000'000'000'000}, to_the_wei}>>; // a thousand of them
using gas_used                   = quantity<gas, inside<{0, 30'000'000}>>;                      // units of work
using gas_price                  = quantity<gwei / gas, inside<{0, 10'000}>>;                   // whole gwei per gas
using rate                       = quantity<USD / ETH, inside<{{0, 1'000'000}, per<100>}>>;     // to the cent
using usd = quantity<USD, inside<{{0, 1'000'000'000'000'000}, per<100>}, round_nearest>>;       // to the cent

int main() {
    static_assert(sizeof(eth) == 16); // 10^27 + 1 points: two 64-bit limbs

    // 1. Exact parsing; the nearest double has lost the last ten digits.
    const std::string text    = "123456789.123456789123456789";
    const eth         balance = *parse<eth>(text);
    std::cout << "balance  " << balance << "\n"
              << "double   " << std::format("{:.17g}", std::stod(text)) << "\n";
    if (to_string(balance) != text + " ETH")
        return 1;
    // One wei is the notch of the grid: 10^-18 ETH.
    static_assert(just<1> * wei == just<rational{1, 1'000'000'000'000'000'000}> * ETH);

    // 2. 21000 gas at 30 gwei/gas is 630000 gwei, exactly 0.00063 ETH.
    const auto fee = gas_used{21'000} * gas_price{30};
    std::cout << "fee      " << fee << " = " << fee.in(ETH) << "\n";
    if (fee != just<rational{63, 100'000}> * ETH || to_string(fee) != "630000 gwei")
        return 1;

    const eth  alice = *parse<eth>("1.5");
    const auto spent = *parse<eth>("1.4999") + fee; // 1.50053 ETH: more than she has
    const auto after = try_value_cast<eth>(alice - spent);
    std::cout << "send     " << (after ? to_string(*after) : errc_message(after.error())) << "\n";
    if (after || after.error() != errc::overflow)
        return 1;

    // 3. A thousand accounts near the top of the grid.
    std::array<eth, 1000> accounts;
    accounts.fill(*parse<eth>("999999999.999999999999999999"));
    const total sum = units::sum<total>(accounts);
    std::cout << "sum      " << sum << "\n";
    if (to_string(sum) != "999999999999.999999999999999000 ETH") // a 10^-18 grid: 18 decimals
        return 1;

    // 4. balance × rate has a 10^-20 notch — a grid past 64-bit numbers — so
    //    round the exact product straight onto the cents grid. ETH·USD/ETH is USD.
    const usd value = mul_into<usd>(balance, *parse<rate>("3141.59"));
    std::cout << "value    " << value << "\n";
    if (to_string(value) != "387850614142.36 USD")
        return 1;
    return 0;
}
