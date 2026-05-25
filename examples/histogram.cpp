// Latency histogram with fixed-point bin edges and outlier rejection.
//
// Demonstrates:
//   - `bound_range` for iterating over bin indices
//   - `will_conversion_overflow` to gate out-of-range samples before insert
//   - `is_conversion_lossy` to count samples that don't land on a notch
//   - Fixed-point bin boundaries (latency in ms with 0.1 resolution)

#include <iostream>
#include <iomanip>
#include <vector>

#include "bound/bound.hpp"
#include "bound/numeric_limits.hpp"
#include "bound/predicates.hpp"
#include "bound/print.hpp"

using namespace bnd;

// Latency samples in [0, 100] ms with 0.1 ms resolution.
using latency_t = bound<{{0, 100}, notch<1, 10>}, round_nearest>;

// 10 bins: 0-9.9, 10-19.9, ..., 90-100 ms.
using bin_id_t  = bound<{0, 9}>;                // counter slot per bin
using bin_idx_t = bound<{0, 9}, round_floor>;   // computed bin index (floor on assignment)

int main()
{
  std::vector<bin_id_t> bins(10, bin_id_t{0});

  // Mixed input: in-range samples, out-of-range outliers, sub-notch values.
  double samples[] = {
     2.5,  7.0, 12.3, 18.7, 25.0, 33.4, 47.8, 51.5, 62.1, 71.9,
    88.0, 95.5,
    150.0, -5.0,        // out-of-range → rejected by will_conversion_overflow
     3.05,              // sub-notch (1/10 grid doesn't include 3.05) → counts as lossy
  };

  int rejected = 0;
  int lossy    = 0;

  for (double s : samples)
  {
    if (will_conversion_overflow<latency_t>(s))
    {
      ++rejected;
      continue;
    }
    if (is_conversion_lossy<latency_t>(s))
      ++lossy;   // accept but flag

    latency_t lat{s};
    // bin index = floor(lat / 10 ms). `bin_idx_t` has `round_floor`, so
    // constructing it from a rational quotient snaps onto the integer grid.
    bin_idx_t bin{lat / rational{10}};
    // Saturate the counter at its max instead of overflowing the bin_id_t.
    if (bins[bin] < bin_id_t{std::numeric_limits<bin_id_t>::max()})
      ++bins[bin];
  }

  // bound_range iterates every bin index — works because the grid has
  // notch 1 and integer lower bound. The implicit `operator size_t()`
  // lets `bins[b]` index the vector without an explicit `.as<>()`.
  using bin_label_t = bound<{0, 99}>;
  std::cout << "bin    count\n";
  for (bin_idx_t b : bound_range<{0, 9}>{})
  {
    bin_label_t lo{b * 10};
    bin_label_t hi{lo + 9};       // bound + int → rational → bin_label_t
    std::cout << " " << lo << "-" << hi << "  " << bins[b] << "\n";
  }

  std::cout << "\nrejected outliers: " << rejected << "\n";
  std::cout << "lossy (off-notch): " << lossy << "\n";
  std::cout << "bin capacity (numeric_limits<bin_id_t>::max()): "
            << std::numeric_limits<bin_id_t>::max() << "\n";
  return 0;
}
