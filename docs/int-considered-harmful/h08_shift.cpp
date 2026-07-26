// Hazard 8: shifts have undefined and implementation-defined corners.

#include <climits>
#include <cstdio>

int main()
{
  volatile int one = 1;
  volatile int shift = 31;

  // Shifting a 1 into the sign bit of a 32-bit int is UB.
  std::printf("1 << 31            = %d   <- undefined behaviour\n", one << shift);

  // Shifting by >= the width is UB, not "zero".
  // (Left as a comment so this program stays runnable.)
  //   one << 32;   // undefined
  //   one << -1;   // undefined

  // Right-shifting a negative value was implementation-defined before C++20.
  std::printf("-8 >> 1            = %d\n", -8 >> 1);
  return 0;
}
