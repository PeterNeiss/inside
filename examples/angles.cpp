// Wrapping angle arithmetic: heading in degrees [0, 359].

#include <iostream>

#include "bound/bound.hpp"
#include "bound/io.hpp"

using namespace bnd;

int main()
{
  using degree = bound<{0, 359}, wrap>;

  degree heading = 350;
  std::cout << "heading:  " << heading << "\n";

  // Turn right 30 degrees (wraps past 360)
  heading += 30_b;
  std::cout << "+30:      " << heading << "\n";  // 20

  // Turn left 90 degrees
  heading += -90_b;
  std::cout << "-90:      " << heading << "\n";  // 290

  // Full rotation
  heading += 360_b;
  std::cout << "+360:     " << heading << "\n";  // 290 (unchanged)

  return 0;
}
