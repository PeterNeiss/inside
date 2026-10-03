// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Hazard 3: two's complement is asymmetric. There is no positive INT_MIN,
// so negation and absolute value have no correct answer to give.

#include <climits>
#include <cstdio>
#include <cstdlib>

int main()
{
  volatile int m = INT_MIN;

  std::printf("INT_MIN            = %d\n", INT_MIN);
  std::printf("INT_MAX            = %d\n", INT_MAX);
  std::printf("-INT_MIN           = %d   <- still negative\n", -m);
  std::printf("abs(INT_MIN)       = %d   <- still negative\n", std::abs(static_cast<int>(m)));

  // INT_MIN / -1 is also undefined: the true answer is INT_MAX+1.
  // On x86-64 it does not wrap, it raises SIGFPE. See h04_divzero.cpp.
  return 0;
}
