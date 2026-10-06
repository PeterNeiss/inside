// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Answer to hazard 7: rounding is a decision you state, not a default you
// inherit. Native integer division always truncates toward zero (-7 / 2 is
// -3); here the default is the exact quotient, and each rounding mode you
// name is honoured, including for negative values.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

int main()
{
  using val = inside<{-100, 100}>;
  using pos = inside<{1, 100}>;   // divisor grid excludes zero

  val n{-7};
  pos d{2};

  // The default is the exact quotient. The divisor cannot be zero and the
  // quotient provably fits, so it is a plain value: nothing to unwrap.
  std::cout << "n / d              = " << n / d << "   (exact)\n";

  // Or, if you ask for a rounding, the one you name -- per call.
  std::cout << "rounded_nearest    = " << div(n, d, rounded_nearest) << "\n";
  std::cout << "rounded_floor      = " << div(n, d, rounded_floor) << "\n";
  std::cout << "rounded_ceil       = " << div(n, d, rounded_ceil) << "\n";
  std::cout << "snapped (trunc)    = " << div(n, d, snapped) << "\n";
  return 0;
}
