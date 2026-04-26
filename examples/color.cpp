// RGB color channels clamped to [0, 255].
// Brightening a pixel by adding to each channel naturally saturates.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

using channel = bound<{0, 255}, clamp>;

struct rgb
{
  channel R, G, B;

  rgb(int r, int g, int b) : R(r), G(g), B(b) {}

  void brighten(int amount)
  {
    R += amount;
    G += amount;
    B += amount;
  }

  void darken(int amount) { brighten(-amount); }

  friend std::ostream& operator<<(std::ostream& os, const rgb& c)
  {
    return os << "rgb(" << c.R << ", " << c.G << ", " << c.B << ")";
  }
};

int main()
{
  rgb pixel{200, 100, 50};
  std::cout << "original:  " << pixel << std::endl;

  pixel.brighten(80);
  std::cout << "brighten:  " << pixel << std::endl;  // rgb(255, 180, 130)

  pixel.darken(300);
  std::cout << "darken:    " << pixel << std::endl;  // rgb(0, 0, 0)

  return 0;
}
