// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A calibration pipeline where each step can fail, and the cause matters:
//   text ──from_chars──▶ reading ──div_into<volts>(reading − offset, gain)──▶ volts
// Every step returns std::expected<…, errc>, so the steps chain with
// and_then, and the first failure — malformed text, a value off the grid or
// out of range, a zero gain — comes out the end with its cause. Steps whose
// grids prove them total (a gain that cannot be 0) return plain values, and
// their error handling disappears at compile time.
// errors.cpp tours the other reporting mechanisms (throw, policies, callbacks).

#include <expected>
#include <iostream>
#include <string_view>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

using reading = inside<{{frac<-1, 10>, frac<1, 10>}, per<1'000'000>}>; // ±100 mV, 1 µV steps
using gain    = inside<{{0, 10}, per<100>}>;                           // 0 is possible: a dead channel
using volts   = inside<{{-1, 1}, per<1'000'000>}, round_nearest>;      // the result, 1 µV

constexpr reading offset{frac<1, 4000>}; // 0.25 mV

// One sample: a reading and the channel's gain, both as text.
std::expected<volts, errc> calibrate(std::string_view r, std::string_view g) {
    return from_chars<reading>(r).and_then([&](reading x) {
        return from_chars<gain>(g).and_then([&](gain k) {
            // (x − offset) / k, rounded once into volts: k's grid holds 0, so the
            // result is an expected carrying a zero gain or a failed store.
            return div_into<volts>(x - offset, k);
        });
    });
}

int main() {
    struct sample {
        std::string_view r, g;
        errc             expect; // errc{} for success
    };
    const sample samples[] = {
        {"0.0125", "2", errc{}},                  // (12.5 − 0.25) mV / 2 = 0.006125 V
        {"0.0125x", "2", errc::invalid_format},   // not a number
        {"0.0000001", "2", errc::rounding_error}, // 0.1 µV: off the 1 µV grid
        {"0.25", "2", errc::overflow},            // past ±100 mV
        {"0.0125", "0", errc::division_by_zero},  // a dead channel
        {"-0.09", "0.01", errc::overflow},        // −9.025 V: past ±1 V
    };
    for (const auto& s : samples) {
        const auto v = calibrate(s.r, s.g);
        std::cout << "(" << s.r << " - 0.00025) / " << s.g << " -> "
                  << (v ? to_string(*v) + " V" : std::string{"error: "} + errc_message(v.error())) << "\n";
        if (v ? s.expect != errc{} : v.error() != s.expect)
            return 1;
    }

    // or_else supplies a fallback for one cause and passes the rest on.
    const auto fallback = calibrate("0.0125", "0").or_else([](errc e) -> std::expected<volts, errc> {
        if (e == errc::division_by_zero)
            return volts{0};
        return std::unexpected(e);
    });
    std::cout << "dead channel with fallback -> " << *fallback << " V\n";

    // The lift operators chain without and_then: an expected operand makes the
    // whole expression an expected, and the first error wins.
    const auto chained = (reading{frac<1, 100>} - offset) / gain{0} * just<2> + reading{frac<1, 1000>};
    std::cout << "chained -> " << errc_message(chained.error()) << "\n";

    // A gain that cannot be 0 makes the division total: a plain value.
    using live_gain   = inside<{{0.5, 10}, per<100>}>;
    const auto always = (reading{frac<1, 100>} - offset) / live_gain{2};
    static_assert(insidable<decltype(always)>); // not an expected
    std::cout << "zero-free gain -> " << always << " V\n";
    return fallback && *fallback == 0 && !chained && chained.error() == errc::division_by_zero ? 0 : 1;
}
