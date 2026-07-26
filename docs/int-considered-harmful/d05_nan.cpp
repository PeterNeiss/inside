// Double hazard 5: NaN propagates silently and breaks the ordering that
// containers and algorithms rely on. It is not equal to itself.

#include <cmath>
#include <cstdio>

int main()
{
  double nan = 0.0 / 0.0;
  double inf = 1.0 / 0.0;

  std::printf("0.0 / 0.0          = %f\n", nan);
  std::printf("1.0 / 0.0          = %f   <- no error reported\n", inf);
  std::printf("nan == nan         ? %s   <- breaks equality\n",
              (nan == nan) ? "true" : "false");
  std::printf("nan < 1            ? %s\n", (nan < 1.0) ? "true" : "false");
  std::printf("nan > 1            ? %s   <- neither; breaks sorting\n",
              (nan > 1.0) ? "true" : "false");
  std::printf("nan + 1000         = %f   <- contaminates the whole total\n", nan + 1000.0);
  return 0;
}
