// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Latency histogram with fixed-point bin edges and outlier rejection.
//
// Demonstrates:
//   - `values<T>` for iterating over bin indices
//   - `div_into` to round the bin quotient once by the index type's policy
//   - `conversion_overflows` to gate out-of-range samples before insert
//   - `conversion_loses` to count samples that don't land on a notch
//   - Fixed-point bin boundaries (latency in ms with 0.1 resolution)
//   - Latencies as quantities in ms: the bin index is latency / 10 ms, a
//     plain number, and the bin labels are back in ms

#include <iostream>
#include <iomanip>
#include <vector>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/formats.hpp>
#include <beman/inside/numeric_limits.hpp>
#include <beman/inside/predicates.hpp>
#include <beman/inside/units.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

constexpr auto ms = si::milli(si::second);

// Latency samples in [0, 100] ms with 0.1 ms resolution.
using latency_t = quantity<ms, inside<{{0, 100}, per<10>}, round_nearest>>;

// 10 bins: 0-9.9, 10-19.9, ..., 90-100 ms.
using bin_count_t = clamp_counter<9>;                    // saturating per-bin count (caps at bar height 9)
using bin_index_t = inside<{0, 9}, clamp | round_floor>; // bin index: floor; 100 ms → last bin

int main() {
    std::vector<bin_count_t> bins(10, bin_count_t{0});

    // Mixed input in ms: in-range samples, out-of-range outliers, sub-notch values.
    double samples[] = {
        2.5,
        7.0,
        12.3,
        18.7,
        25.0,
        33.4,
        47.8,
        51.5,
        62.1,
        71.9,
        88.0,
        95.5,
        150.0,
        -5.0, // out-of-range → rejected by conversion_overflows
        3.05, // sub-notch (1/10 grid doesn't include 3.05) → counts as lossy;
              // so does any double with a decimal fraction (12.3 is 12.300000000000000711)
    };

    clamp_counter<1'000'000> rejected{0};
    clamp_counter<1'000'000> lossy{0};

    for (double s : samples) {
        if (conversion_overflows<latency_t>(s)) {
            ++rejected;
            continue;
        }
        if (conversion_loses<latency_t>(s))
            ++lossy; // accept but flag

        const latency_t lat{s};
        // bin index = floor(lat / 10 ms), a plain number. div_into rounds by
        // `bin_index_t`'s `round_floor` (9.55 floors to 9 before the range check);
        // `clamp` folds 100 ms, whose quotient 10 floors to 10, into the last bin.
        const auto bin = div_into<bin_index_t>(lat, just<10> * ms);
        // `clamp_counter` saturates on `++`, so the bar caps at 9 — no manual guard.
        ++bins[bin];
    }

    // values<bin_index_t> iterates every bin index. The implicit `operator imax()`
    // (plus the standard imax → size_t conversion) lets `bins[b]` index the
    // vector without an explicit `.as<>()`.
    using bin_label_t = quantity<ms, inside<{0, 99}>>;
    std::cout << "bin          count\n";
    for (bin_index_t b : values<bin_index_t>) {
        bin_label_t lo{b * (just<10> * ms)}; // {0,9}×{10,10} ms → {0,90} ms, fits statically
        bin_label_t hi{lo + just<9> * ms};   // widened then narrowed to bin_label_t
        std::cout << " " << lo << "-" << hi << "  " << bins[b] << "\n";
    }

    std::cout << "\nrejected outliers: " << rejected << "\n";
    std::cout << "lossy (off-notch): " << lossy << "\n";
    std::cout << "bin capacity (numeric_limits<bin_count_t>::max()): " << std::numeric_limits<bin_count_t>::max()
              << "\n";
    if (rejected != 2 || lossy != 7 || bins[9] != 1)
        return 1;
    return 0;
}
