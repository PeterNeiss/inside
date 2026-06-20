// Integer division with snapping policy.
// By default, division produces exact rational results.
// With snapping, division uses native integer division for zero overhead.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/io.hpp"

using namespace bnd;

int main()
{
  using val = bound<{0, 100}>;

  val a = 7;
  val b = 3;

  // Default: exact rational result
  auto exact = a / b;
  if (exact)
    std::cout << "7 / 3 (rational) = " << *exact << "\n";  // 7/3

  // Per-call integer division: truncates like native C++ division
  auto quotient = div(a, b, truncated);
  if (quotient)
    std::cout << "7 / 3 (integer)  = " << *quotient << "\n";  // 2

  // Type-level policy: operator/ uses native division automatically
  using fast = bound<{0, 100}, snapping>;
  fast x = 22;
  fast y = 7;
  auto q = x / y;
  if (q)
    std::cout << "22 / 7 (fast)    = " << *q << "\n";  // 3

  // Division by zero always returns nullopt
  fast zero = 0;
  auto bad = x / zero;
  std::cout << "22 / 0           = "
            << (bad.has_value() ? "value" : "nullopt") << "\n";

  // Exact division works as expected
  val ten = 10;
  val two = 2;
  auto five = div(ten, two, truncated);
  if (five)
    std::cout << "10 / 2 (integer) = " << *five << "\n";  // 5

  return 0;
}
