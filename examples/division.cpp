// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Division produces rational results by default.
// The result is std::expected<inside, errc> whenever the divisor grid holds
// zero (division by zero yields errc::division_by_zero).
// With snap, division uses native integer division instead.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

int main()
{
  using val = inside<{0, 100}>;
  val a = 7;
  val b = 3;

  // Exact rational result (default)
  auto result = a / b;
  if (result)
    std::cout << "7 / 3 (exact)    = " << *result << "\n";  // 7/3

  // Integer division with per-call policy
  auto quotient = div(a, b, snapped);
  if (quotient)
    std::cout << "7 / 3 (integer)  = " << *quotient << "\n";  // 2

  // The result type uses rational storage for exact fractions
  val c = 22;
  val d = 7;
  auto pi_ish = c / d;
  if (pi_ish)
    std::cout << "22 / 7 (exact)   = " << *pi_ish << "\n";  // 22/7

  // Division by zero returns errc::division_by_zero
  val zero = 0;
  auto div_zero = val(10) / zero;
  std::cout << "10 / 0           = "
            << (div_zero.has_value() ? "value" : errc_message(div_zero.error())) << "\n";

  return 0;
}
