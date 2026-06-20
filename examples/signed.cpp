// Signed integer bounds: negative ranges with direct storage.
// When lower < 0 and notch == 1, Raw stores the value directly as a signed
// integer — no offset arithmetic, matching native int performance.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/io.hpp"

using namespace bnd;

int main()
{
  // Temperature sensor: -40 to +85 degrees, fits in int8_t
  using temp = bound<{-40, 85}>;
  static_assert(sizeof(temp) == 1);

  temp room = 22;
  temp outside = -15;
  std::cout << "room:    " << room << "\n";
  std::cout << "outside: " << outside << "\n";

  // Arithmetic on signed bounds
  auto diff = room - outside;
  std::cout << "diff:    " << diff << "\n";  // 37

  // Negation
  auto neg = -room;
  std::cout << "-room:   " << neg << "\n";   // -22

  // Altitude: -500 to 9000 meters, fits in int16_t
  using altitude = bound<{-500, 9000}>;
  static_assert(sizeof(altitude) == 2);

  altitude mountain  = 4500;
  altitude cave      = -200;

  auto climb = mountain - cave;
  std::cout << "climb:   " << climb << "\n";  // 4700

  // Signed bounds with clamp
  using voltage = bound<{-12, 12}, clamp>;
  voltage v = 15;   // clamped to 12
  std::cout << "15V clamped: " << v << "\n";

  v = -20;          // clamped to -12
  std::cout << "-20V clamped: " << v << "\n";

  return 0;
}
