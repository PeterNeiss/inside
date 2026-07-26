// Hazard 1: signed overflow is undefined behaviour, not wraparound.
// The compiler is entitled to assume it never happens -- and then to
// delete the code you wrote to check for it.
//
//   g++ -std=c++23 -O0 h01_overflow_ub.cpp -o h01_O0   ->  "no"
//   g++ -std=c++23 -O2 h01_overflow_ub.cpp -o h01_O2   ->  "yes, always"

#include <climits>
#include <cstdio>

// Does adding 1 to an int always make it bigger?
[[gnu::noinline]] static bool grows(int x)
{
  return x + 1 > x;
}

int main()
{
  volatile int big = INT_MAX;

  std::printf("INT_MAX            = %d\n", INT_MAX);
  std::printf("grows(INT_MAX)     = %s\n", grows(big) ? "yes, always" : "no");
  std::printf("INT_MAX + 1        = %d\n", big + 1);
  return 0;
}
