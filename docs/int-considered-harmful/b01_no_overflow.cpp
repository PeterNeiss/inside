// Answer to hazards 1, 2 and 10: arithmetic cannot overflow, because the
// result TYPE widens at compile time to hold every value the operands
// could possibly produce. There is no overflow to be undefined.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

int main()
{
  using u8 = inside<{10, 255}>;
  constexpr u8 a{16};
  constexpr u8 b{220};

  // The sum of two [10,255] values lives in [20,510]. Checked at compile time.
  static_assert(a + b == 236);
  std::cout << "a + b              = " << (a + b) << "\n";

  // A difference that would underflow a native uint8 widens to signed instead.
  static_assert(a - b == -204);
  std::cout << "a - b              = " << (a - b) << "   (no wraparound)\n";

  // The midpoint computation that breaks binary search cannot break here:
  // lo + hi is computed in a grid wide enough to hold it.
  using idx = inside<{0, 2147483647}>;
  idx lo{2147483644};
  idx hi{2147483646};
  auto mid = (lo + hi) / just<2>;
  if (mid)
    std::cout << "(lo + hi) / 2      = " << *mid << "   (still in range)\n";

  return 0;
}
