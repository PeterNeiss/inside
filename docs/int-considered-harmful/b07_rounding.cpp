// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Answer to hazard 7: rounding is a decision you state, not a default you
// inherit. Native integer division always truncates toward zero; here each
// mode is honoured, including for negative values.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

int main()
{
  using val = inside<{-100, 100}>;
  using pos = inside<{1, 100}>;   // divisor grid excludes zero

  val  n{-7};
  pos  d{2};

  std::cout << "native  -7 / 2     = " << (-7 / 2) << "   (toward zero, always)\n";

  std::cout << "rounded_nearest    = " << div(n, d, rounded_nearest) << "\n";
  std::cout << "rounded_floor      = " << div(n, d, rounded_floor) << "\n";
  std::cout << "rounded_ceil       = " << div(n, d, rounded_ceil)  << "\n";
  std::cout << "snapped (trunc)    = " << div(n, d, snapped) << "\n";

  // And the exact answer, if you want no rounding at all.
  auto exact = n / d;
  if (exact)
    std::cout << "exact              = " << *exact << "\n";
  return 0;
}
