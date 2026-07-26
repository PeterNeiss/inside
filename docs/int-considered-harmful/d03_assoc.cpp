// Double hazard 3: floating-point addition is not associative, so the
// answer depends on the order you happened to visit the data in.
// Parallelise a sum, change the chunk count, get a different result.

#include <cstdio>

int main()
{
  double a = 1e16;
  double b = -1e16;
  double c = 1.0;

  std::printf("(a + b) + c        = %.1f\n", (a + b) + c);
  std::printf("a + (b + c)        = %.1f\n", a + (b + c));
  std::printf("equal              ? %s\n",
              (((a + b) + c) == (a + (b + c))) ? "true" : "false");
  return 0;
}
