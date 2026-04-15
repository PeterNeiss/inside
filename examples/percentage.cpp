// Clamped percentage: values outside [0, 100] are saturated to the boundary.

#include <iostream>

#include "bound/bound.hpp"

using namespace bnd;

int main()
{
  using pct = bound<{0, 100}, clamp>;

  pct brightness = 80;
  std::cout << "brightness: " << brightness << std::endl;

  brightness = 120;  // clamped to 100
  std::cout << "set 120:    " << brightness << std::endl;

  brightness = -10;  // clamped to 0
  std::cout << "set -10:    " << brightness << std::endl;

  // adjust with +=, clamping is automatic
  brightness = 80;
  brightness += 50;  // clamped to 100
  std::cout << "+= 50:     " << brightness << std::endl;

  brightness += -200;  // clamped to 0
  std::cout << "+= -200:   " << brightness << std::endl;

  // per-operation clamp on a strict (throwing) type
  bound<{0, 100}> strict_pct(50);
  strict_pct.with_clamp() = 200;
  std::cout << "with_clamp: " << strict_pct << std::endl;

  return 0;
}
