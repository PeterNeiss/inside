// RGB color channels clamped to [0, 255].
// Brightening a pixel by adding to each channel naturally saturates.

#include <iostream>

#include "bound/bound.hpp"

using namespace bnd;

using channel = bound<{0, 255}, clamp>;

struct rgb
{
  channel r, g, b;

  rgb(int r, int g, int b) : r(r), g(g), b(b) {}

  void brighten(int amount)
  {
    r += amount;
    g += amount;
    b += amount;
  }

  void darken(int amount) { brighten(-amount); }

  friend std::ostream& operator<<(std::ostream& os, const rgb& c)
  {
    return os << "rgb(" << c.r << ", " << c.g << ", " << c.b << ")";
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
