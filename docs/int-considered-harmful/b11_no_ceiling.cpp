// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The 64-bit ceiling, removed (C++26 with static reflection). Under C++23 a
// grid's limits and notch are 64-bit fractions; under C++26 they have no size
// limit, and the raw storage is a fixed-width integer sized from the grid.
//
//   g++-16 -std=c++26 -freflection -O2 -I ../../include b11_no_ceiling.cpp -o b11

#include <cstdint>
#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

int main()
{
  // int64 stops at 9223372036854775807. A [0, 2^100] grid does not.
  using huge = inside<{0, 0x1p100}>;
  constexpr huge a = 1e30;                    // the double 1e30, an integer, held exactly

  // a * a lives in [0, 2^200]: the product grid is computed at compile time,
  // so the multiplication cannot overflow and returns a plain value.
  constexpr auto sq = a * a;
  std::cout << "a                  = " << a << "\n";
  std::cout << "a * a              = " << sq << "\n";
  std::cout << "sizeof(a), sizeof(a * a) = " << sizeof(a) << ", " << sizeof(sq) << " bytes\n";

  // A notch of exactly 10^-30, which no double and no int64 ratio can hold.
  using fine = inside<{{0, 1}, 1e-30_g}, round_nearest>;
  fine third = frac<1, 3>;                    // rounded once, onto the 1e-30 grid
  std::cout << "1/3 on a 1e-30 grid = " << third << "\n";
  return 0;
}
