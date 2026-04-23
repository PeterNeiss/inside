// Wrapping angle arithmetic: heading in degrees [0, 359].

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  using degree = bound<{0, 359}, wrap>;

  degree heading = 350;
  std::cout << "heading:  " << heading << std::endl;

  // Turn right 30 degrees (wraps past 360)
  heading += 30;
  std::cout << "+30:      " << heading << std::endl;  // 20

  // Turn left 90 degrees
  heading += -90;
  std::cout << "-90:      " << heading << std::endl;  // 290

  // Full rotation
  heading += 360;
  std::cout << "+360:     " << heading << std::endl;  // 290 (unchanged)

  return 0;
}
