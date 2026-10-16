// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Latency histogram with fixed-point bin edges and outlier rejection.
//
// Demonstrates:
//   - `values<T>` for iterating over bin indices
//   - `div_into` to round the bin quotient once by the index type's policy
//   - `conversion_overflows` to gate out-of-range samples before insert
//   - `conversion_loses` to count samples that don't land on a notch
//   - Fixed-point bin boundaries (latency in ms with 0.1 resolution)

#include <iostream>
#include <iomanip>
#include <vector>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/formats.hpp>
#include <beman/inside/numeric_limits.hpp>
#include <beman/inside/predicates.hpp>

using namespace beman::inside;

// Latency samples in [0, 100] ms with 0.1 ms resolution.
using latency_t = inside<{{0, 100}, per<10>}, round_nearest>;

// 10 bins: 0-9.9, 10-19.9, ..., 90-100 ms.
using bin_id_t  = clamp_counter<9>;                    // saturating per-bin count (caps at bar height 9)
using bin_idx_t = inside<{0, 9}, clamp | round_floor>; // bin index: floor; 100 ms → last bin

int main() {
    std::vector<bin_id_t> bins(10, bin_id_t{0});

    // Mixed input: in-range samples, out-of-range outliers, sub-notch values.
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
        3.05, // sub-notch (1/10 grid doesn't include 3.05) → counts as lossy
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

        latency_t lat{s};
        // bin index = floor(lat / 10 ms). div_into rounds by `bin_idx_t`'s
        // `round_floor` (9.55 floors to 9 before the range check); `clamp` folds
        // 100 ms, whose quotient 10 floors to 10, into the last bin.
        const auto bin = div_into<bin_idx_t>(lat, just<10>);
        // `clamp_counter` saturates on `++`, so the bar caps at 9 — no manual guard.
        ++bins[bin];
    }

    // values<bin_idx_t> iterates every bin index. The implicit `operator imax()`
    // (plus the standard imax → size_t conversion) lets `bins[b]` index the
    // vector without an explicit `.as<>()`.
    using bin_label_t = inside<{0, 99}>;
    std::cout << "bin    count\n";
    for (bin_idx_t b : values<bin_idx_t>) {
        bin_label_t lo{b * just<10>}; // inside × inside: {0,9}×{10,10} → {0,90}, fits statically
        bin_label_t hi{lo + just<9>}; // inside + inside, widened then narrowed to bin_label_t
        std::cout << " " << lo << "-" << hi << "  " << bins[b] << "\n";
    }

    std::cout << "\nrejected outliers: " << rejected << "\n";
    std::cout << "lossy (off-notch): " << lossy << "\n";
    std::cout << "bin capacity (numeric_limits<bin_id_t>::max()): " << std::numeric_limits<bin_id_t>::max() << "\n";
    return 0;
}
