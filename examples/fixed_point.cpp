// Fixed-point arithmetic using fractional notch grids.
// A grid with notch 1/256 gives 8-bit fractional precision (8.8 fixed-point).
// The library handles all scaling automatically — no manual bit shifting.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  // 8.8 fixed-point: values from 0 to 255 in steps of 1/256
  using fp8 = bound<{{0, 255}, 1.0/256}>;

  fp8 a = 3;
  fp8 b = 7;

  std::cout << "a = " << a << std::endl;  // 3
  std::cout << "b = " << b << std::endl;  // 7

  // Addition preserves precision
  auto sum = a + b;
  std::cout << "a + b = " << sum << std::endl;  // 10

  // Multiplication scales correctly
  auto prod = a * b;
  std::cout << "a * b = " << prod << std::endl;  // 21

  // Half-step grid: sensor readings at 0.5 resolution
  using sensor = bound<{{0, 50}, 0.5}>;

  sensor reading = 23;
  sensor offset  = 2;

  auto adjusted = reading + offset;
  std::cout << "reading + offset = " << adjusted << std::endl;  // 25

  // Quarter-step grid: fine-grained control
  using knob = bound<{{0, 10}, 0.25}>;

  knob volume = 7;
  std::cout << "volume = " << volume << std::endl;  // 7

  // Storage is compact: knob needs only uint8_t for 40 steps
  static_assert(sizeof(knob) == 1);
  std::cout << "sizeof(knob) = " << sizeof(knob) << std::endl;

  return 0;
}
