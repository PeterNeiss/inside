// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The strongest correctness claim: the checks run at compile time.
// Everything below is a static_assert -- if any of it were wrong, this
// file would not build, and no test would need to run.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>

using namespace beman::inside;

using pct   = inside<{0, 100}>;
using angle = inside<{0, 359}, wrap>;

int main()
{
  // Range violations are knowable before the program exists.
  static_assert(conversion_overflows<pct>(150));
  static_assert(conversion_overflows<pct>(-1));
  static_assert(!conversion_overflows<pct>(50));

  // Saturating and wrapping casts are constant-evaluable.
  static_assert(pct{150, clamp} == 100);
  static_assert(angle{370, wrap} == 10);
  static_assert(angle{-10, wrap} == 350);

  // Arithmetic results are exact and checkable at compile time.
  constexpr pct a{60};
  constexpr pct b{55};
  static_assert(a + b == 115);   // widened past 100, and known to be so

  std::cout << "every check in this file ran at compile time\n";
  std::cout << "a + b              = " << (a + b) << "\n";
  std::cout << "pct{150, clamp} = " << pct{150, clamp} << "\n";
  std::cout << "angle{370, wrap} = " << angle{370, wrap} << "\n";
  return 0;
}
