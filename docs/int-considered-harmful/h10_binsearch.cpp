// Hazard 10: "Nearly All Binary Searches Are Broken" (Bentley 1986,
// Bloch 2006). The midpoint computation overflows for large ranges --
// a bug that lived in the JDK for nine years.

#include <climits>
#include <cstdio>

int main()
{
  volatile int lo = INT_MAX - 3;
  volatile int hi = INT_MAX - 1;

  int mid_broken = (lo + hi) / 2;          // overflows
  int mid_safe   = lo + (hi - lo) / 2;     // does not

  std::printf("lo                 = %d\n", lo);
  std::printf("hi                 = %d\n", hi);
  std::printf("(lo + hi) / 2      = %d   <- outside [lo, hi]\n", mid_broken);
  std::printf("lo + (hi - lo) / 2 = %d   <- correct\n", mid_safe);
  return 0;
}
