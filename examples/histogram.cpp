// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A latency histogram in ms with 0.1 ms resolution: conversion_overflows rejects outliers before they are stored,
// conversion_loses flags samples off the notch, and div<bin> rounds the bin index once.

#include <iostream>
#include <limits>
#include <vector>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/inside/formats.hpp>
#include <inside/inside/predicates.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;
using namespace ins::formats;

constexpr auto ms = si::milli(si::second);

int main() {
    // Ten bins, 0–9.9, 10–19.9, …, 90–100 ms; a count saturates at a bar height of 9.
    using bar_height = clamp_counter<9>;
    std::vector<bar_height> bins(10, bar_height{0});

    // In-range samples, outliers, and values off the 0.1 ms notch.
    double samples[] = {
        2.5, 7.0, 12.3, 18.7, 25.0, 33.4, 47.8, 51.5, 62.1, 71.9, 88.0, 95.5, 150.0,
        -5.0, // out of range: rejected
        3.05, // off the notch: lossy, as is any double with a decimal fraction (12.3 is 12.300000000000000711)
    };

    using milliseconds = quantity<ms, inside<{{0, 100}, per<10>}, round_half_away>>;
    using bin = inside<{0, 9}, clamp | round_floor>; // 100 ms → the last bin; bins takes it as an index
    clamp_counter<1'000'000> rejected{0};
    clamp_counter<1'000'000> lossy{0};

    for (double s : samples) {
        if (conversion_overflows<milliseconds>(s, ms)) {
            ++rejected;
            continue;
        }
        if (conversion_loses<milliseconds>(s, ms))
            ++lossy; // accepted, but flagged

        const auto stored = milliseconds::try_make(s, ms); // a NaN would be its error
        if (!stored) {
            ++rejected;
            continue;
        }
        const milliseconds lat = *stored;
        // floor(lat / 10 ms), by bin's round_floor (9.55 floors to 9); clamp folds 100 ms, quotient 10, into bin 9.
        const auto b = div<bin>(lat, just<10> * ms);
        ++bins[b]; // the counter saturates on ++, so the bar caps at 9
    }

    // values<bin> iterates every bin, and each indexes bins with no check left: the index type is the proof.
    using bin_label = quantity<ms, inside<{0, 99}>>;
    std::cout << "bin          count\n";
    for (bin b : values<bin>) {
        const auto lo = b * (just<10> * ms); // {0,9}×{10,10} ms → {0,90} ms
        bin_label hi{lo + just<9> * ms};     // {9,99} ms: fits bin_label statically
        std::cout << " " << lo << "-" << hi << "  " << bins[b] << "\n";
    }

    std::cout << "\nrejected outliers: " << rejected << "\n";
    std::cout << "lossy (off-notch): " << lossy << "\n";
    std::cout << "bin capacity (numeric_limits<bar_height>::max()): " << std::numeric_limits<bar_height>::max()
              << "\n";
    if (rejected != 2 || lossy != 7 || bins[9] != 1)
        return 1;
    return 0;
}
