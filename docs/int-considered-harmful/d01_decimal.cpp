// Double hazard 1: decimal fractions are not representable in binary.
// The value you wrote is not the value you got.

#include <cstdio>

int main()
{
  double a = 0.1;
  double b = 0.2;

  std::printf("0.1 + 0.2          = %.20f\n", a + b);
  std::printf("0.3                = %.20f\n", 0.3);
  std::printf("0.1 + 0.2 == 0.3   ? %s\n", (a + b == 0.3) ? "true" : "false");
  return 0;
}
