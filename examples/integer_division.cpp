// Integer division with ignore_round policy.
// By default, division produces exact rational results.
// With ignore_round, division uses native integer division for zero overhead.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  using val = bound<{0, 100}>;

  val a = 7;
  val b = 3;

  // Default: exact rational result
  auto exact = a / b;
  if (exact)
    std::cout << "7 / 3 (rational) = " << *exact << std::endl;  // 7/3

  // Per-call integer division: truncates like native C++ division
  auto truncated = div(a, b, make_policy<ignore_round>());
  if (truncated)
    std::cout << "7 / 3 (integer)  = " << *truncated << std::endl;  // 2

  // Type-level policy: operator/ uses native division automatically
  using fast = bound<{0, 100}, ignore_round>;
  fast x = 22;
  fast y = 7;
  auto q = x / y;
  if (q)
    std::cout << "22 / 7 (fast)    = " << *q << std::endl;  // 3

  // Division by zero always returns nullopt
  fast zero = 0;
  auto bad = x / zero;
  std::cout << "22 / 0           = "
            << (bad.has_value() ? "value" : "nullopt") << std::endl;

  // Exact division works as expected
  val ten = 10;
  val two = 2;
  auto five = div(ten, two, make_policy<ignore_round>());
  if (five)
    std::cout << "10 / 2 (integer) = " << *five << std::endl;  // 5

  return 0;
}
