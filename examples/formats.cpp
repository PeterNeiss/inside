// Predefined hardware-width types from <bound/formats.hpp>.
// Each maps to a native byte width (uint8/int16/...), so they read and store
// like the machine types you hand to an audio buffer, pixel, or DSP register —
// while still carrying a compile-time range and policy.

#include <iostream>

#include "bound/formats.hpp"
#include "bound/io.hpp"

using namespace bnd;

int main()
{
  // 1. Native byte widths — the whole point of the set.
  static_assert(sizeof(byte) == 1 && sizeof(sword) == 2 && sizeof(unorm16) == 2
             && sizeof(q8_8) == 2 && sizeof(q16_16) == 4);
  std::cout << "byte widths: byte=" << sizeof(byte)
            << " sword=" << sizeof(sword)
            << " unorm16=" << sizeof(unorm16)
            << " q8_8=" << sizeof(q8_8)
            << " q16_16=" << sizeof(q16_16) << "\n";

  // 2. PCM-style mixing: sum two 16-bit samples, saturate back into sword.
  sword a{30000}, b{20000};
  sword mixed{0};
  mixed.with_clamp() = a + b;          // 50000 saturates to +32767
  std::cout << "mix " << a << " + " << b << " -> " << mixed << " (clamped)\n";

  // 3. Apply a normalized [0,1] gain to a sample, round back to sword.
  unorm16 gain{0.5_b};                 // unorm reaches exactly 0 and 1
  sword sample{10000};
  sword out{0};
  out.with_snap<round_nearest>() = gain * sample;
  std::cout << "gain " << gain << " * " << sample << " -> " << out << "\n";

  // 4. Fixed-point Q-formats.
  q8_8 x{42.5}, y{2.25};
  auto qsum = x + y;
  std::cout << "q8_8 " << x << " + " << y << " = " << qsum << "\n";
  q16_16 fine{1000.125};
  std::cout << "q16_16 " << fine << "\n";

  // 5. slim::optional<bound> stays zero-overhead (sentinel-encoded).
  static_assert(sizeof(slim::optional<byte>) == sizeof(byte));
  slim::optional<byte> maybe = byte{200};
  std::cout << "optional<byte> holds " << *maybe
            << " (sizeof " << sizeof(maybe) << " == " << sizeof(byte) << ")\n";
  maybe = slim::nullopt;
  std::cout << "after reset: has_value=" << std::boolalpha << maybe.has_value() << "\n";

  return 0;
}
