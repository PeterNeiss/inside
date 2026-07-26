// Answer to hazard 3: negation returns the REFLECTED type. The result of
// negating a [Lower, Upper] value lives in [-Upper, -Lower], so the
// asymmetry that makes -INT_MIN impossible simply does not arise.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/io.hpp"

using namespace bnd;

int main()
{
  using temp = bound<{-40, 125}>;   // an asymmetric range, on purpose

  constexpr temp coldest{-40};

  // -coldest has type bound<{-125, 40}> -- the interval is mirrored.
  static_assert(-coldest == 40);
  std::cout << "coldest            = " << coldest << "\n";
  std::cout << "-coldest           = " << (-coldest) << "   (representable by construction)\n";

  // The same holds at the extreme of the range: there is no value whose
  // negation falls outside the negated type.
  constexpr temp hottest{125};
  static_assert(-hottest == -125);
  std::cout << "-hottest           = " << (-hottest) << "\n";

  return 0;
}
