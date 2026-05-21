// Reorder buffer with `sentinel` policy: each slot holds a packet's seq
// number, and an out-of-range write trips on_sentinel — exactly the "this
// packet arrived too late to fit the window" event a jitter buffer needs.
//
// Demonstrates:
//   - `sentinel` policy and `on_sentinel` callback (per-write hook)
//   - `checked_cast` for slot-index validation
//   - `bound_range` to walk every slot in playback order
//   - Fixed-point packet timestamps (1/8 ms resolution)

#include <iostream>
#include <vector>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

// Window of 16 slots indexed 0..15.
using slot_id_t = bound<{0, 15}>;

// Slot contents: relative offset from the playback base (0..63), with a
// sentinel slot indicating "empty / dropped".
using packet_id_t = bound<{0, 63}, sentinel>;

// Timestamp in 1/8 ms — fixed-point precision tied to the audio frame.
using ts_t = bound<{{0, 8000}, notch<1, 8>}, round_nearest>;

int main()
{
  std::vector<packet_id_t> slots(16);
  for (auto& s : slots) s = packet_id_t{0};   // initialise to a non-sentinel value

  int dropped = 0;

  // Per-packet insertion. The "offset" is (seq - base). If it exceeds the
  // window (offset > 63), the write trips the sentinel and on_sentinel fires.
  auto insert = [&](int seq, int base) {
    int offset = seq - base;
    int slot   = (seq % 16);

    // Validate the slot index up front. `checked_cast` throws on garbage
    // input — useful at the trust boundary (e.g. parsing a packet header).
    auto idx = checked_cast<slot_id_t>(slot);

    slots[static_cast<std::size_t>(static_cast<int>(idx))]
      .on_sentinel([&](auto&, auto original) {
        std::cout << "[dropped: offset " << original
                  << " too far past base " << base << "]\n";
        ++dropped;
      }) = offset;
  };

  // Insert 20 packets: most within window, two stragglers way past base.
  std::cout << "inserting packets relative to base seq=0:\n";
  for (int seq : { 0, 3, 1, 5, 4, 2, 8, 6, 9, 11, 10, 13, 12, 14, 15, 7,
                   90, 100 })
    insert(seq, 0);

  std::cout << "\nplayback order (sentinel = dropped/empty):\n";
  for (auto slot : bound_range<{0, 15}>{})
  {
    std::size_t idx = static_cast<std::size_t>(static_cast<int>(slot));
    auto& s = slots[idx];
    // `is_sentinel()` is the public probe for the slot's empty state under
    // the `sentinel` policy.
    if (s.is_sentinel())
      std::cout << "  slot " << slot << "  <empty>\n";
    else
      std::cout << "  slot " << slot << "  packet offset " << s << "\n";
  }

  std::cout << "\ndrop events: " << dropped << "\n";

  // Companion timestamps live on a fractional notch so the API can interop
  // with sub-millisecond audio clocks without slipping into floating point.
  ts_t arrival{125.375};
  ts_t playback{125.5};
  std::cout << "\narrival - playback (1/8 ms grid): "
            << (playback - arrival) << " ms\n";

  return 0;
}
