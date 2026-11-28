// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The first things to reach for: what happens at the edge of a range is the type's policy, written once:
// clamp saturates, wrap is modular, and the default, checked, reports (errors.cpp shows how).

#include <array>
#include <iostream>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;

namespace
{
  inline constexpr auto DN = base_unit<"DN", "pixel_value">; // a digital number: a raw pixel value
} // namespace

// A pixel whose channels saturate.
// A run-time delta has a range too: name it, so `R += d` is quantity += quantity.
using channel       = id<"[0..255] [DN] clamp">;
using channel_delta = id<"[-512..512] [DN]">;

struct rgb
{
  channel R, G, B;

  void brighten(channel_delta d)
  {
    R += d;
    G += d;
    B += d;
  }
  friend std::ostream& operator<<(std::ostream& os, const rgb& c)
  {
    return os << "rgb(" << c.R << ", " << c.G << ", " << c.B << ")";
  }
};

int main()
{
  // 1. clamp: a brightness, an RGB pixel.
  using brightness_percent = id<"[0..100] [%] clamp">;
  brightness_percent brightness{120, percent}; // 100 %
  brightness -= 200_q * percent;               // 0 %: a constant steps a clamp type
  std::cout << "brightness " << brightness << "\n";

  id<"[0..100]"> strict{50};   // checked by default …
  strict.with<clamp>() = 200;  // … clamped for this one store
  std::cout << "with<clamp> " << strict << "\n";
  if (brightness != 0_q * percent || strict != 100)
    return 1;

  rgb pixel{{200, DN}, {100, DN}, {50, DN}};
  pixel.brighten(80_q * DN);
  std::cout << "brighten   " << pixel << "\n"; // rgb(255 DN, 180 DN, 130 DN)
  pixel.brighten(-300_q * DN);
  std::cout << "darken     " << pixel << "\n"; // rgb(0 DN, 0 DN, 0 DN)
  if (pixel.R != 0_q * DN || pixel.G != 0_q * DN)
    return 1;

  // 2. wrap: a compass heading in [0°, 360°), whole degrees.
  using heading_degrees = id<"[0..360) [deg] wrap">;
  heading_degrees heading{350, si::degree};
  heading += 30_q  * si::degree; // 20°
  heading += -90_q * si::degree; // 290°
  heading += 360_q * si::degree; // a full turn: 290°
  std::cout << "heading    " << heading << "\n";
  if (heading != 290_q * si::degree)
    return 1;

  // 3. Bounded loops: the index is an inside, so arr[i] is in range.
  const std::array arr{10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
  using arr_index = id<"[0..9]">; // arr takes it as it is
  std::cout << "from 5, wrapping:";
  for (arr_index i : inside_range<grid_of<arr_index>>{5})
    std::cout << ' ' << arr[i];
  std::cout << "\nevery index:     ";
  for (arr_index i : values<arr_index>)
    std::cout << ' ' << arr[i];
  std::cout << "\nevery third:     ";
  for (cursor<arr_index, 3> i; i != end(i); ++i)
    std::cout << ' ' << arr[arr_index{*i}];
  std::cout << "\n";
  return 0;
}
