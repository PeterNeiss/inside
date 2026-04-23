// Division produces rational results.
// The result is always slim::optional (division by zero yields nullopt).

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  using val = bound<{1, 100}>;
  val a = 7;
  val b = 3;

  auto result = a / b;
  if (result)
    std::cout << "7 / 3 = " << *result << std::endl;

  // The result type uses rational storage for exact fractions
  val c = 22;
  val d = 7;
  auto pi_ish = c / d;
  if (pi_ish)
    std::cout << "22 / 7 = " << *pi_ish << std::endl;

  // Division by zero returns nullopt
  using maybe_zero = bound<{0, 10}>;
  maybe_zero zero = 0;
  auto div_zero = val(10) / zero;
  std::cout << "10 / 0 = " << (div_zero.has_value() ? "value" : "nullopt") << std::endl;

  return 0;
}
