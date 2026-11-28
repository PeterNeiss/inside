// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Ether to the wei: 10^-18 ETH up to a billion ETH is 10^27 points, more than 64 bits number,
// so the raw is a two-limb integer and every amount, fee and total stays exact.
// Ether, gas and dollars are base dimensions of their own,
// so a gas price is gwei/gas and an exchange rate USD/ETH, never a plain number.

#include <array>
#include <format>
#include <iostream>
#include <string>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;

namespace
{
  inline constexpr auto ETH  = base_unit<"ETH", "ether">;
  inline constexpr auto gwei = unit<"gwei", 1e-9_q  * ETH>;
  inline constexpr auto wei  = unit<"wei" , 1e-18_q * ETH>;
  inline constexpr auto gas  = base_unit<"gas", "gas">;
  inline constexpr auto USD  = base_unit<"USD", "dollar">;
} // namespace

inline constexpr auto to_the_wei = per<1'000'000'000'000'000'000>;

int main()
{
  using account_eth = id<quantity_key<ETH, id<key{grid{{0, 1'000'000'000}, to_the_wei}}>>>; // one account
  static_assert(sizeof(account_eth) == 16); // 10^27 + 1 points: two 64-bit limbs

  // 1. Exact parsing from text with its unit, where the nearest double has lost the last ten digits.
  const std::string text = "123456789.123456789123456789 ETH";
  const account_eth balance{*parse<account_eth>(text)};
  std::cout << "balance  " << balance << "\n"
            << "double   " << std::format("{:.17g}", std::stod(text)) << "\n";
  if (to_string(balance) != text)
    return 1;
  static_assert(wei == 1e-18_q * ETH); // one wei is the notch of the grid

  // 2. A fee is gas × price: 21000 gas at 30 gwei/gas is 630000 gwei, exactly 0.00063 ETH.
  using gas_amount   = id<"[0..30'000'000] [gas]">;                   // units of work
  using gwei_per_gas = id<"[0..10'000] [gwei/gas]">;                  // whole gwei per gas
  const auto fee{gas_amount{21'000, gas} * gwei_per_gas{30, gwei / gas}};
  std::cout << "fee      " << fee << " = " << fee.in(ETH) << "\n";
  if (fee != 0.00063_q * ETH || to_string(fee) != "630000 gwei")
    return 1;

  // An overdraft is reported, not wrapped: 1.4999 ETH + the fee is more than 1.5 ETH.
  const account_eth alice{*parse<account_eth>("1.5 ETH")};
  const auto        spent{*parse<account_eth>("1.4999 ETH") + fee}; // 1.50053 ETH
  const auto        after{value_cast<account_eth>(alice - spent)};
  std::cout << "send     " << (after ? to_string(*after) : error_message(after.error())) << "\n";
  if (after || after.error() != error::overflow)
    return 1;

  // 3. A thousand accounts near the top of the grid:
  // sum adds the wide raws exactly (an expected: the count is a run-time number).
  using total_eth = id<quantity_key<ETH, id<key{grid{{0, 1'000'000'000'000}, to_the_wei}}>>>;
  std::array<account_eth, 1000> accounts;
  accounts.fill(*parse<account_eth>("999999999.999999999999999999 ETH"));
  const auto sum{ins::sum<total_eth>(accounts)};
  if (!sum)
    return 1;
  std::cout << "sum      " << *sum << "\n";
  if (to_string(*sum) != "999999999999.999999999999999000 ETH") // a 10^-18 grid: 18 decimals
    return 1;

  // 4. In dollars at 3141.59 USD/ETH: balance × rate has a 10^-20 notch, past 64-bit numbers,
  // so the exact product rounds straight onto the cents. ETH·USD/ETH is USD.
  using usd_per_eth = id<"[0..1'000'000] 1/100 [USD/ETH]">; // to the cent
  using dollars     = id<"[0..1'000'000'000'000'000] 1/100 [USD] round_half_away">;
  const dollars value{mul<dollars>(balance, *parse<usd_per_eth>("3141.59 USD/ETH"))};
  std::cout << "value    " << value << "\n";
  if (to_string(value) != "387850614142.36 USD")
    return 1;
  return 0;
}
