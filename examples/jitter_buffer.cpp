// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A reorder buffer whose slots are checked: an out-of-range write trips on_error,
// which is exactly the "arrived too late to fit the window" event a jitter buffer needs.
// Offsets are in pkt, a unit of our own; times in ms.

#include <iostream>
#include <vector>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;

namespace
{
  inline constexpr auto pkt = base_unit<"pkt", "packets">;
  constexpr auto        ms  = si::milli(si::second);
} // namespace

int main()
{
  // 16 slots; each holds a packet's offset from the playback base, 0..63 pkt
  // (in one byte, so whether a slot is filled is kept apart).
  using slot_index  = inside<{0, 15}>;
  using slot_offset = quantity<pkt, inside<{0, 63}>>;
  constexpr std::size_t    window{values<slot_index>.size()}; // 16
  std::vector<slot_offset> slots(window, slot_offset{0, pkt});
  std::vector<bool>        filled(window, false);
  int                      dropped{0};

  // A packet's offset is seq − base; past 63 pkt the write is rejected and on_error fires.
  using seq_offset = quantity<pkt, inside<{-1024, 1024}>>;
  auto insert = [&](int seq, int base)
  {
    const auto stored_offset{seq_offset::try_make(seq - base, pkt)}; // a run-time count: try_make reports
    if (!stored_offset)
    {
      ++dropped;
      return;
    }
    const auto offset{*stored_offset};
    const auto idx   {slot_index{seq, wrap}}; // seq modulo the window: the wrap flag folds it into a slot

    bool stored{true};
    slots[idx].on_error(
      [&](auto&, error, auto)
      {
        std::cout << "[dropped: offset " << offset << " too far past base " << base << "]\n";
        ++dropped;
        stored = false;
      }) = offset;
    filled[idx] = stored;
  };

  // 18 packets: most within the window, two stragglers far past the base.
  std::cout << "inserting packets relative to base seq=0:\n";
  for (int seq : {0, 3, 1, 5, 4, 2, 8, 6, 9, 11, 10, 13, 12, 14, 15, 7, 90, 100})
    insert(seq, 0);

  std::cout << "\nplayback order:\n";
  for (slot_index slot : values<slot_index>)
  {
    if (!filled[slot])
      std::cout << "  slot " << slot << "  <empty>\n";
    else
      std::cout << "  slot " << slot << "  packet offset " << slots[slot] << "\n";
  }
  std::cout << "\ndrop events: " << dropped << "\n";

  // Timestamps on a 1/8 ms notch meet sub-millisecond audio clocks without floating point.
  using milliseconds = quantity<ms, inside<{{0, 8000}, per<8>}, round_half_away>>;
  milliseconds arrival {125.375, ms};
  milliseconds playback{125.5  , ms};
  std::cout << "\nplayback - arrival (1/8 ms grid): " << (playback - arrival) << "\n";

  if (dropped != 2 || to_string(slots[5]) != "5 pkt" || to_string(playback - arrival) != "0.125 ms")
    return 1;
  return 0;
}
