// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Answer to hazard 9: the scale is part of the type. A value on a
// 1-cent grid and a value on a 1-dollar grid are different types, and
// combining them converts rather than pretending they are the same
// integer. There is no "is this cents or dollars?" left to get wrong.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

using money   = inside<{{0, 1'000'000}, notch<1, 100>}, round_nearest>;
using dollars = inside<{0, 1000}>;   // whole dollars only

int main()
{
  money subtotal{0};
  subtotal += money{19.99};
  subtotal += money{4.50};
  subtotal += money{12.34};
  std::cout << "subtotal           = $" << subtotal << "\n";

  dollars shipping{5};   // "5" -- but 5 what? The type says: whole dollars.
  std::cout << "shipping           = $" << shipping << "\n";

  // Adding them scales the dollar value onto the cent grid automatically.
  auto total = subtotal + shipping;
  std::cout << "total              = $" << total << "   (not $36.88)\n";

  // A bare integer cannot enter the arithmetic at all: `subtotal + 5` is
  // ill-formed, because 5 carries no grid. See b06_compile_time.cpp.
  return 0;
}
