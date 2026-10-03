// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Double hazard 4: beyond 2^53 a double cannot represent consecutive
// integers, so counters and ids silently stop incrementing.
// (inside encodes exactly this limit as its `double_exact` rule.)

#include <cstdio>

int main()
{
  double x = 9007199254740992.0;   // 2^53

  std::printf("2^53               = %.1f\n", x);
  std::printf("2^53 + 1           = %.1f   <- unchanged\n", x + 1.0);
  std::printf("2^53 + 1 == 2^53   ? %s\n", (x + 1.0 == x) ? "true" : "false");

  // Catastrophic cancellation: subtracting near-equal values destroys
  // every significant digit that mattered.
  double big = 1e16;
  std::printf("(1e16 + 1) - 1e16  = %.1f   (should be 1)\n", (big + 1.0) - big);
  return 0;
}
