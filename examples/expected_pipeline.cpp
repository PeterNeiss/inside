// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A calibration pipeline, text → reading → (reading − offset) / gain in volts,
// where each step can fail and the cause matters.
// Every step returns std::expected, so the steps chain and the first failure comes out with its cause;
// a step whose grids prove it total returns a plain value, and its error handling disappears.

#include <expected>
#include <iostream>
#include <string_view>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;

using reading_volts = id<"[-1/10..1/10] 1/1'000'000 [V]">; // ±100 mV to 1 µV
using gain          = id<"[0..10] 1/100">; // 0 is possible: a dead channel
using volts         = id<"[-1..1] 1/1'000'000 [V] round_half_away">; // the result, to 1 µV

constexpr reading_volts offset{"1/4000"_q * si::volt}; // 0.25 mV

// One sample, a reading with its unit and the channel's gain as text.
// The reading may come in any unit of voltage (mV, µV): parse converts it exactly, or reports why not.
// k's grid holds 0, so div<volts> (one rounding into volts) is an expected
// carrying a zero gain or a failed store.
std::expected<volts, error> calibrate(std::string_view r, std::string_view g)
{
  return parse<reading_volts>(r).and_then(
    [&](reading_volts x)
    {
      return parse<gain>(g).and_then([&](gain k) { return div<volts>(x - offset, k); });
    });
}

int main()
{
  struct sample
  {
    std::string_view r, g;
    error            expect; // error{} for success
  };
  const sample samples[] = {
    {"12.5 mV",   "2",    error{}},                  // (12.5 − 0.25) mV / 2 = 0.006125 V
    {"12.5x mV",  "2",    error::invalid_format},    // not a number
    {"12.5",      "2",    error::missing_unit},      // a number without its unit
    {"12.5 mA",   "2",    error::incompatible_unit}, // a current, no voltage
    {"0.1 µV",    "2",    error::rounding_error},    // off the 1 µV grid
    {"250 mV",    "2",    error::overflow},          // past ±100 mV
    {"12.5 mV",   "0",    error::division_by_zero},  // a dead channel
    {"-90 mV",    "0.01", error::overflow},          // −9.025 V: past ±1 V
  };
  for (const auto& s : samples)
  {
    const auto v{calibrate(s.r, s.g)};
    std::cout << "(" << s.r << " - 0.25 mV) / " << s.g << " -> "
              << (v ? to_string(*v) : std::string{"error: "} + error_message(v.error())) << "\n";
    if (v ? s.expect != error{} : v.error() != s.expect)
      return 1;
  }

  // or_else supplies a fallback for one cause and passes the rest on.
  const auto fallback{calibrate("12.5 mV", "0").or_else(
    [](error e) -> std::expected<volts, error>
    {
      if (e == error::division_by_zero)
        return volts{0, si::volt};
      return std::unexpected(e);
    })};
  std::cout << "dead channel with fallback -> " << *fallback << "\n";

  // The lift operators chain without and_then:
  // an expected operand makes the expression an expected, and the first error wins.
  const auto chained{(reading_volts{0.01_q * si::volt} - offset) / gain{0} * 2_q
                     + reading_volts{0.001_q * si::volt}};
  std::cout << "chained -> " << error_message(chained.error()) << "\n";

  // A gain that cannot be 0 makes the division total: a plain value.
  using live_gain = id<"[0.5..10] 1/100">;
  const auto always{(reading_volts{0.01_q * si::volt} - offset) / live_gain{2}};
  static_assert(quantity_like<decltype(always)>);
  std::cout << "zero-free gain -> " << always << "\n";

  const bool fell_back{fallback && *fallback == volts{0, si::volt}};
  return fell_back && !chained && chained.error() == error::division_by_zero ? 0 : 1;
}
