// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Answer to the double hazards: an exact 1-cent notch. The value is
// Lower + k*Notch over an exact rational, so a running total cannot drift.
// Compare d02_money_drift.cpp, which is the same computation in double.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

int main()
{
  // 0..$1,000,000 in 1-cent steps. 0.01 is not exact in binary, so the
  // notch is given as an exact rational -- not as 0.01.
  using money = inside<{{0, 1'000'000}, per<100>}, round_nearest>;
  static_assert(sizeof(money) == 4);   // still four bytes

  money total{0};
  for (int i = 0; i < 10000; ++i)
    total += money{0.01};

  std::cout << "10000 x $0.01      = $" << total << "\n";
  std::cout << "exactly $100.00    ? "
            << (total == money{100.00} ? "true" : "false") << "\n";

  // Read the value back out exactly, as an integer pair.
  std::cout << "as a fraction      = " << total.numerator()
            << "/" << total.denominator() << "\n";
  return 0;
}
