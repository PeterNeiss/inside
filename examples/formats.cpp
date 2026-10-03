// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Predefined hardware-width types from <beman/inside/formats.hpp>.
// Each maps to a native byte width (uint8/int16/...), so they read and store
// like the machine types you hand to an audio buffer, pixel, or DSP register —
// while still carrying a compile-time range and policy.

#include <iostream>

#include <beman/inside/formats.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

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
  unorm16 gain{0.5_ins};                 // unorm reaches exactly 0 and 1
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

  // 5. Native widths use their full range: byte is [0, 255] in one byte.
  static_assert(sizeof(byte) == 1);
  byte top{255};
  std::cout << "byte max " << top << " (sizeof " << sizeof(top) << ")\n";

  // 6. Fallible arithmetic returns std::expected<inside, errc> with the cause.
  auto q = byte{200} / byte{0};
  std::cout << "200 / 0: has_value=" << std::boolalpha << q.has_value()
            << ", error=" << errc_message(q.error()) << "\n";

  return 0;
}
