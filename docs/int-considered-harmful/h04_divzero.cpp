// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Hazard 4: integer division has two undefined inputs, and neither one
// produces a value you can test for. There is no integer NaN.
//
// Run with an argument to pick the case:
//   ./h04_divzero 0    -> x / 0        (undefined; SIGFPE on x86-64)
//   ./h04_divzero 1    -> INT_MIN / -1 (undefined; SIGFPE on x86-64)

#include <climits>
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv)
{
  int which = (argc > 1) ? std::atoi(argv[1]) : 0;

  volatile int num = (which == 1) ? INT_MIN : 10;
  volatile int den = (which == 1) ? -1 : 0;

  std::printf("about to evaluate %d / %d ...\n", num, den);
  std::fflush(stdout);

  int result = num / den;

  std::printf("result = %d (never reached)\n", result);
  return 0;
}
