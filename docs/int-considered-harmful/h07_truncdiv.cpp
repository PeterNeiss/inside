// Hazard 7: integer division silently discards the remainder, and it
// rounds toward zero -- which is neither floor nor nearest. You do not
// get to say which you wanted.

#include <cstdio>

int main()
{
  std::printf(" 7 / 2             = %d   (exact answer 3.5)\n", 7 / 2);
  std::printf("-7 / 2             = %d   (rounds toward zero, not down)\n", -7 / 2);
  std::printf(" 7 %% 2             = %d\n", 7 % 2);
  std::printf("-7 %% 2             = %d   <- negative remainder\n", -7 % 2);

  // Averaging three test scores: 90, 90, 91 -> 90, not 90.33
  int total = 90 + 90 + 91;
  std::printf("mean(90,90,91)     = %d   (true mean 90.333...)\n", total / 3);
  return 0;
}
