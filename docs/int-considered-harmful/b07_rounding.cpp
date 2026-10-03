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

  constexpr auto floored = make_policy<round_floor>();
  constexpr auto ceiled  = make_policy<round_ceil>();

  std::cout << "round_to_nearest   = " << div(n, d, round_to_nearest) << "\n";
  std::cout << "floored            = " << div(n, d, floored) << "\n";
  std::cout << "ceiled             = " << div(n, d, ceiled)  << "\n";
  std::cout << "truncated          = " << div(n, d, truncated) << "\n";

  // And the exact answer, if you want no rounding at all.
  auto exact = n / d;
  if (exact)
    std::cout << "exact              = " << *exact << "\n";
  return 0;
}
