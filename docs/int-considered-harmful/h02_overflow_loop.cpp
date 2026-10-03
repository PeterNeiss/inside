// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Hazard 2: the same assumption turns a terminating loop into an endless one.
// `i > 0` is true for every i the compiler believes is reachable, because
// reaching INT_MAX and incrementing would be UB.
//
//   -O0 -> the counter wraps negative and the loop ends after 3 iterations
//   -O2 -> the loop never ends (the escape hatch below saves us)

#include <climits>
#include <cstdio>

int main()
{
  int count = 0;

  for (int i = INT_MAX - 2; i > 0; ++i)
  {
    ++count;
    if (count > 10)
    {
      std::printf("still going after %d iterations -- loop never terminates\n", count);
      break;
    }
  }

  std::printf("iterations = %d\n", count);
  return 0;
}
