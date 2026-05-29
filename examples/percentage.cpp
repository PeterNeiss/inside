// Clamped percentage: values outside [0, 100] are saturated to the boundary.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  using pct = bound<{0, 100}, clamp>;

  pct brightness = 80;
  std::cout << "brightness: " << brightness << "\n";

  brightness = 120;  // clamped to 100
  std::cout << "set 120:    " << brightness << "\n";

  brightness = -10;  // clamped to 0
  std::cout << "set -10:    " << brightness << "\n";

  // adjust with +=, clamping is automatic
  brightness = 80;
  brightness += 50;  // clamped to 100
  std::cout << "+= 50:     " << brightness << "\n";

  brightness += -200;  // clamped to 0
  std::cout << "+= -200:   " << brightness << "\n";

  // per-operation clamp on a strict (throwing) type
  bound<{0, 100}> strict_pct(50);
  strict_pct.with_clamp() = 200;
  std::cout << "with_clamp: " << strict_pct << "\n";

  return 0;
}
