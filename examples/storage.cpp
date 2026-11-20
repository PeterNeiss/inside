// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Which raw an inside stores, always deduced from the grid:
// the smallest integer that numbers every point, native widths from <inside/inside/formats.hpp>,
// and a fixed wire layout written with to<T>().

#include <cstdint>
#include <iostream>

#include <inside/inside/formats.hpp>
#include <inside/inside.hpp>
#include <inside/inside/io.hpp> // to_string_debug: value, raw and raw type
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;
using namespace ins::formats;

namespace
{
  inline constexpr auto LSB = base_unit<"LSB", "pcm_code">; // a PCM code: one least significant bit
} // namespace

constexpr auto deg_C = si::degree_Celsius;
constexpr auto m     = si::metre;

int main()
{
  // 1. Deduced storage: the value itself where a whole-number range costs no width, else the 0-based index.
  using register_value = inside<{5   , 100 }>;   // uint8_t, raw == value (no wider than the index)
  using offset         = inside<{200 , 300 }>;   // uint8_t, raw == value − 200 (300 needs 16 bits)
  using half_steps     = inside<{{-5, 5}, 0.5}>; // uint8_t index: a fractional notch is offset-encoded
  // A unit adds no byte: a quantity, or a point, stores its number.
  using percentage     = quantity<percent, inside<{0, 100}>>;                    // uint8_t, raw == value
  using celsius        = quantity_point<deg_C, si::ice_point, inside<{-40, 85}>>; // int8_t, raw == value (signed)
  using altitude       = quantity<si::metre, inside<{-500, 9000}>>;              // int16_t
  static_assert(sizeof(half_steps) == 1 && sizeof(percentage) == 1 && sizeof(celsius) == 1
                && sizeof(altitude) == 2);
  const celsius cold{-12, deg_C};
  const auto    cold_number{cold.quantity_from_origin().numerical_value_in(deg_C)}; // the inside it stores
  std::cout << to_string_debug(offset{242}) << "\n" << cold << ": " << to_string_debug(cold_number) << "\n";
  if (register_value{42}.raw() != 42 || offset{242}.raw() != 42 || cold_number.raw() != -12
      || half_steps{-4.5}.raw() != 1)
    return 1;

  // Signed arithmetic stays exact and its result grid widens: no overflow.
  const auto climb{altitude{4500, m} - altitude{-200, m}};
  std::cout << "climb " << climb << "\n";
  if (climb != just<4700> * m)
    return 1;

  // 2. Presets at native widths that still carry a range and a policy, here as the numbers of PCM samples.
  static_assert(sizeof(uint8) == 1 && sizeof(int16) == 2 && sizeof(unorm16) == 2 && sizeof(uq8_8) == 2
                && sizeof(uq16_16) == 4);
  using sample = quantity<LSB, int16>;
  static_assert(sizeof(sample) == 2);
  sample mixed{0, LSB};
  mixed.with<clamp>() = sample{30000, LSB} + sample{20000, LSB};                  // a mix: 50000 saturates
  const sample out{mul<sample>(unorm16{0.5_ins}, sample{10001, LSB}, round_half_away)}; // a gain, rounded once
  std::cout << "mix " << mixed << ", gain " << out << "\n";
  if (mixed != just<32767> * LSB || out != just<5001> * LSB)
    return 1;

  // 3. A fixed wire layout: the field's type is the wire's, the value checked.
  struct packet
  {
    std::uint16_t slot; // 0..65535
  };
  using slot = inside<{0, 1000}>;
  const packet p{slot{750}}; // an index-like inside converts to an integer that holds its grid
  std::cout << "packet slot " << p.slot << "\n";
  if (p.slot != 750)
    return 1;
  return 0;
}
