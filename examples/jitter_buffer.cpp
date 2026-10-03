// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Reorder buffer with a checked slot type: each slot holds a packet's seq
// number, and an out-of-range write trips on_error — exactly the "this
// packet arrived too late to fit the window" event a jitter buffer needs.
//
// Demonstrates:
//   - `on_error` callback (per-write hook) on a checked inside
//   - `checked_cast` for slot-index validation
//   - `inside_range` to walk every slot in playback order
//   - Fixed-point packet timestamps (1/8 ms resolution)

#include <iostream>
#include <vector>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

// Window of 16 slots indexed 0..15.
using slot_id_t = inside<{0, 15}>;

// Slot contents: relative offset from the playback base (0..63). Whether a
// slot holds a packet is tracked separately, so the slot keeps its native
// 8-bit storage.
using packet_id_t = inside<{0, 63}>;

// Timestamp in 1/8 ms — fixed-point precision tied to the audio frame.
using ts_t = inside<{{0, 8000}, notch<1, 8>}, round_nearest>;

int main()
{
  std::vector<packet_id_t> slots(16, packet_id_t{0});
  std::vector<bool>        filled(16, false);

  int dropped = 0;

  // Per-packet insertion. The "offset" is (seq - base). If it exceeds the
  // window (offset > 63), the write is rejected and on_error fires.
  auto insert = [&](int seq, int base) {
    int offset = seq - base;
    int slot   = (seq % 16);

    // Validate the slot index up front. `checked_cast` throws on garbage
    // input — useful at the trust boundary (e.g. parsing a packet header).
    auto idx = checked_cast<slot_id_t>(slot);

    bool stored = true;
    slots[idx]
      .on_error([&](auto&, errc, auto) {
        std::cout << "[dropped: offset " << offset
                  << " too far past base " << base << "]\n";
        ++dropped;
        stored = false;
      }) = offset;
    filled[idx] = stored;
  };

  // Insert 20 packets: most within window, two stragglers way past base.
  std::cout << "inserting packets relative to base seq=0:\n";
  for (int seq : { 0, 3, 1, 5, 4, 2, 8, 6, 9, 11, 10, 13, 12, 14, 15, 7,
                   90, 100 })
    insert(seq, 0);

  std::cout << "\nplayback order:\n";
  for (auto slot : inside_range<{0, 15}>{})
  {
    if (!filled[slot])
      std::cout << "  slot " << slot << "  <empty>\n";
    else
      std::cout << "  slot " << slot << "  packet offset " << slots[slot] << "\n";
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
