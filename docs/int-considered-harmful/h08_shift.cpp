// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Hazard 8: shifts. Since C++20 a signed shift into the sign bit is defined
// (modulo 2^N), and >> of a negative value rounds down. Defined is not the
// same as right, and a shift count outside [0, width) is still undefined.

#include <climits>
#include <cstdio>

int main()
{
  volatile int one   = 1;
  volatile int shift = 31;

  // Defined since C++20: 1 * 2^31 modulo 2^32. A positive value turns negative.
  std::printf("1 << 31            = %d   <- defined, and negative\n", one << shift);

  // Defined since C++20: >> rounds down, / rounds toward zero.
  std::printf("-7 >> 1            = %d\n", -7 >> 1);
  std::printf("-7 / 2             = %d   <- same \"halve\", other answer\n", -7 / 2);

  // Still undefined: a count >= the width (or negative). x86 masks the
  // count to 5 bits, so this prints 1 here; the standard promises nothing.
  shift = 32;
  std::printf("1 << 32            = %d   <- undefined behaviour\n", one << shift);
  return 0;
}
