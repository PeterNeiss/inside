// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// MIGRATION, AFTER: the same invoice in inside.
//
// The overflow is gone (the product widens), the tax is exact to the cent
// by an explicitly chosen rounding mode, and the discount keeps its exact
// one-third value until the moment it is snapped back onto the cent grid.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

// Dollars in 1-cent steps, up to $10M. The scale is part of the type.
using money = inside<{{0, 10'000'000}, per<100>}, round_nearest>;

int main()
{
  money subtotal{2'684'354.56};

  // subtotal * 0.08 widens; nothing can overflow. Assigning back into
  // `money` snaps to the nearest cent, because `money` says round_nearest.
  auto  tax_exact = subtotal * 0.08_ins;
  money tax       = tax_exact;

  // A third of the subtotal is not representable in cents. It stays exact
  // until it is stored, and then rounds by the stated rule.
  auto  disc_exact = subtotal * frac<1, 3>;
  money discount   = disc_exact;

  money total = subtotal + tax - discount;

  std::cout << "subtotal           = $" << subtotal << "\n";
  std::cout << "tax  (8%)          = $" << tax      << "   (exact, no overflow)\n";
  std::cout << "discount (1/3)     = $" << discount << "   (rounded to nearest cent)\n";
  std::cout << "total              = $" << total    << "\n";

  // The intermediate really is exact: a grid in thirds of a cent stores
  // the same value with nothing rounded away.
  using third_cent = inside<{{0, 10'000'000}, per<300>}, round_nearest>;
  third_cent discount_exact = disc_exact;

  std::cout << "\ndiscount on a 1/300 grid       = $" << discount_exact
            << "   (nothing was lost)\n";
  return 0;
}
