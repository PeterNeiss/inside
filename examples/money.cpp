// Currency arithmetic with cents precision.
// A 0.01 notch over [0, 1'000'000] gives exact two-decimal storage in uint32 —
// no float drift on running totals.

#include <iostream>
#include <vector>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  // 0..$1,000,000 in 1¢ steps -> 100M+1 steps fits uint32.
  // 0.01 is not exact in binary, so the notch is given as an exact rational.
  using money = bound<{{0, 1'000'000}, notch<1, 100>}, round_nearest>;
  static_assert(sizeof(money) == 4);

  // Running total: line items add exactly, no float drift.
  std::vector<money> items = {19.99, 4.50, 12.34, 0.99, 7.25};
  money subtotal{0};
  for (auto i : items) subtotal += i;
  std::cout << "subtotal: $" << subtotal << "\n";

  // Apply 8% sales tax. `bound * rational` returns a rational; `round_nearest`
  // on `money` snaps the assignment to the nearest 1¢ notch.
  money tax = subtotal * 0.08_b;
  std::cout << "tax (8%): $" << tax << "\n";

  money total = subtotal + tax;
  std::cout << "total:    $" << total << "\n";

  // Issue a refund, capped at the total. `clamped` saturates rather than
  // throwing if the requested refund exceeds what was paid. `std::min`
  // works on two `money` values directly via the bound's spaceship operator.
  money requested_refund{100.00};
  money refund{0};
  refund.with_clamp() = std::min(requested_refund, total);
  std::cout << "refund (req $" << requested_refund
            << ", cap $" << total << "): $" << refund << "\n";

  return 0;
}
