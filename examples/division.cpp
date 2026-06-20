// Division produces rational results by default.
// The result is always slim::optional (division by zero yields nullopt).
// With snap, division uses native integer division instead.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/io.hpp"

using namespace bnd;

int main()
{
  using val = bound<{0, 100}>;
  val a = 7;
  val b = 3;

  // Exact rational result (default)
  auto result = a / b;
  if (result)
    std::cout << "7 / 3 (exact)    = " << *result << "\n";  // 7/3

  // Integer division with per-call policy
  auto quotient = div(a, b, truncated);
  if (quotient)
    std::cout << "7 / 3 (integer)  = " << *quotient << "\n";  // 2

  // The result type uses rational storage for exact fractions
  val c = 22;
  val d = 7;
  auto pi_ish = c / d;
  if (pi_ish)
    std::cout << "22 / 7 (exact)   = " << *pi_ish << "\n";  // 22/7

  // Division by zero returns nullopt
  val zero = 0;
  auto div_zero = val(10) / zero;
  std::cout << "10 / 0           = "
            << (div_zero.has_value() ? "value" : "nullopt") << "\n";

  return 0;
}
