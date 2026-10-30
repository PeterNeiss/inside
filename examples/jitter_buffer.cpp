// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Reorder buffer with a checked slot type: each slot holds a packet's seq
// number, and an out-of-range write trips on_error — exactly the "this
// packet arrived too late to fit the window" event a jitter buffer needs.
// Packet offsets are quantities of a unit of our own (pkt) and timestamps are
// in ms; the slot numbers are indices into the window and stay plain.
//
// Demonstrates:
//   - `on_error` callback (per-write hook) on a checked inside
//   - `wrap_cast` to fold a sequence number into its slot
//   - `values<T>` to size the window and walk every slot in playback order
//   - Fixed-point packet timestamps (1/8 ms resolution)

#include <iostream>
#include <vector>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units_io.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
inline constexpr base_dimension packet_dim{"packets"};
inline constexpr quantity_spec  packets_def{"packets", packet_dim};
inline constexpr named_unit     pkt_def{"pkt", spec_c<packets_def>};
inline constexpr auto           pkt = unit_c<pkt_def>;
constexpr auto                  ms  = si::milli(si::second);
} // namespace

// Window of 16 slots indexed 0..15.
using slot_id_t = inside<{0, 15}>;

// Slot contents: relative offset from the playback base (0..63 pkt). Whether
// a slot holds a packet is tracked separately, so the slot keeps its native
// 8-bit storage.
using packet_offset_t = quantity<pkt, inside<{0, 63}>>;

// Timestamp in 1/8 ms — fixed-point precision tied to the audio frame.
using timestamp_t = quantity<ms, inside<{{0, 8000}, per<8>}, round_nearest>>;

int main() {
    constexpr std::size_t        window = values<slot_id_t>.size(); // 16
    std::vector<packet_offset_t> slots(window, packet_offset_t{0});
    std::vector<bool>            filled(window, false);

    int dropped = 0;

    // Per-packet insertion. The "offset" is (seq - base). If it exceeds the
    // window (offset > 63), the write is rejected and on_error fires.
    auto insert = [&](int seq, int base) {
        const quantity<pkt, inside<{-1024, 1024}>> offset{seq - base};

        // The slot is seq modulo the window: wrap_cast folds it into slot_id_t.
        const auto idx = wrap_cast<slot_id_t>(seq);

        bool stored = true;
        slots[idx].on_error([&](auto&, errc, auto) {
            std::cout << "[dropped: offset " << offset << " too far past base " << base << "]\n";
            ++dropped;
            stored = false;
        })          = offset;
        filled[idx] = stored;
    };

    // Insert 18 packets: most within window, two stragglers way past base.
    std::cout << "inserting packets relative to base seq=0:\n";
    for (int seq : {0, 3, 1, 5, 4, 2, 8, 6, 9, 11, 10, 13, 12, 14, 15, 7, 90, 100})
        insert(seq, 0);

    std::cout << "\nplayback order:\n";
    for (slot_id_t slot : values<slot_id_t>) {
        if (!filled[slot])
            std::cout << "  slot " << slot << "  <empty>\n";
        else
            std::cout << "  slot " << slot << "  packet offset " << slots[slot] << "\n";
    }

    std::cout << "\ndrop events: " << dropped << "\n";

    // Companion timestamps live on a fractional notch so the API can interop
    // with sub-millisecond audio clocks without slipping into floating point.
    timestamp_t arrival{125.375};
    timestamp_t playback{125.5};
    std::cout << "\nplayback - arrival (1/8 ms grid): " << (playback - arrival) << "\n";

    if (dropped != 2 || to_string(slots[slot_id_t{5}]) != "5 pkt" || to_string(playback - arrival) != "0.125 ms")
        return 1;
    return 0;
}
