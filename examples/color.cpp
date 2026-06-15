// RGB color channels clamped to [0, 255].
// Brightening a pixel by adding to each channel naturally saturates.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

using channel = bound<{0, 255}, clamp>;

// A signed per-channel adjustment. A runtime delta has a range — name it — so
// `R += amount` stays bound += bound (a raw int is no longer an arithmetic RHS).
using channel_delta = bound<{-512, 512}>;

struct rgb
{
  channel R, G, B;

  rgb(int r, int g, int b) : R(r), G(g), B(b) {}

  void brighten(channel_delta amount)
  {
    R += amount;
    G += amount;
    B += amount;
  }

  void darken(channel_delta amount) { brighten(-amount); }

  friend std::ostream& operator<<(std::ostream& os, const rgb& c)
  {
    return os << "rgb(" << c.R << ", " << c.G << ", " << c.B << ")";
  }
};

int main()
{
  rgb pixel{200, 100, 50};
  std::cout << "original:  " << pixel << "\n";

  pixel.brighten(80);
  std::cout << "brighten:  " << pixel << "\n";  // rgb(255, 180, 130)

  pixel.darken(300);
  std::cout << "darken:    " << pixel << "\n";  // rgb(0, 0, 0)

  return 0;
}
