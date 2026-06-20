// Fixed-point arithmetic using fractional notch grids.
// A grid with notch 1/256 gives 8-bit fractional precision (8.8 fixed-point).
// The library handles all scaling automatically — no manual bit shifting.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/io.hpp"
#include "bound/formats.hpp"

using namespace bnd;

int main()
{
  // 8.8 fixed-point: values from 0 to 255 in steps of 1/256.
  // This is exactly the predefined `bnd::q8_8` from <bound/formats.hpp>.
  using fp8 = q8_8;

  fp8 a = 3.5;
  fp8 b = 7.25;

  std::cout << "a = " << a << "\n";  // 3.5
  std::cout << "b = " << b << "\n";  // 7.25

  // Addition preserves precision
  auto sum = a + b;
  std::cout << "a + b = " << sum << "\n";  // 10.75

  // Multiplication scales correctly
  auto prod = a * b;
  std::cout << "a * b = " << prod << "\n";  // 25.375

  // Half-step grid: sensor readings at 0.5 resolution
  using sensor = bound<{{0, 50}, 0.5}>;

  sensor reading = 23.5;
  sensor offset  = 2.5;

  auto adjusted = reading + offset;
  std::cout << "reading + offset = " << adjusted << "\n";  // 26

  // Quarter-step grid: fine-grained control
  using knob = bound<{{0, 10}, 0.25}>;

  knob volume = 7.75;
  std::cout << "volume = " << volume << "\n";  // 7.75

  // Storage is compact: knob needs only uint8_t for 40 steps
  static_assert(sizeof(knob) == 1);
  std::cout << "sizeof(knob) = " << sizeof(knob) << "\n";

  return 0;
}
