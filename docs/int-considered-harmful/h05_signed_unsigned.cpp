// Hazard 5: mixed signed/unsigned comparison. The usual arithmetic
// conversions quietly turn the signed operand unsigned, so a negative
// number becomes an enormous positive one.

#include <cstdio>
#include <vector>

int main()
{
  std::printf("-1 < 0u            = %s\n", (-1 < 0u) ? "true" : "false");

  // The classic: iterate backwards over an empty container.
  std::vector<int> v;   // empty
  std::printf("v.size()           = %zu\n", v.size());
  std::printf("v.size() - 1       = %zu   <- not -1\n", v.size() - 1);

  int visited = 0;
  for (std::size_t i = 0; i < v.size() - 1; ++i)
  {
    ++visited;
    if (visited > 3) break;   // escape hatch: this loop runs ~2^64 times
  }
  std::printf("loop body entered  = %d times on an EMPTY vector\n", visited);
  return 0;
}
