// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Modular packet sequence numbers (TCP/QUIC-style) with epoch tracking.
// Packets are a base dimension of their own (unit pkt): a sequence number,
// a window and a ring slot count packets, while the epoch and the lap stay
// plain counts of wraps.
//
// Demonstrates:
//   - `wrap` policy for modular-2^N arithmetic
//   - `on_wrap` callback feeding a wrap-epoch counter (cascade pattern)
//   - `_ins` integer literal times a unit: `100_ins * pkt`
//   - `divmod(s, 256_ins * pkt, round_toward_zero)` for the ring-buffer lap and slot

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/formats.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units_io.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
inline constexpr base_dimension packet_dim{"packets"};
inline constexpr quantity_spec  packets_def{"packets", packet_dim};
inline constexpr named_unit     pkt_def{"pkt", spec_c<packets_def>};
inline constexpr auto           pkt = unit_c<pkt_def>;
} // namespace

// 16-bit modular sequence space: the full uint16 range, wrapping under `wrap`.
using seq_t = quantity<pkt, inside<{0, 65535}, wrap>>;

// Epoch counter — how many times the SEQ space has wrapped (saturating tally).
using epoch_t = clamp_counter<1000>;

int main() {
    seq_t   seq{65500};
    epoch_t epoch{0};

    // A quantity delta (`_ins` times pkt): `seq += window` wraps, and the
    // on_wrap carry is an inside (here ignored — we only bump the epoch counter).
    constexpr auto window = 100_ins * pkt;

    // `_ins` builds a single-value `inside<{N, N}>`, and times a unit a
    // single-value quantity. Adding it to another quantity widens the grid via
    // the addition machinery — a constant in type-safe arithmetic that never
    // turns into a runtime int.
    auto preview = window + seq;
    std::cout << "preview after +100 pkt: " << preview << "\n";

    std::cout << "step  seq    epoch\n";
    for (int i = 0; i < 8; ++i) {
        // on_wrap callback fires when seq + window crosses the upper bound.
        // The handler bumps the epoch counter — TCP/QUIC's "wrap count" idiom.
        seq.on_wrap([&](auto&, auto) { ++epoch; }) += window;
        std::cout << "  " << i << "    " << seq << "    " << epoch << "\n";
    }

    std::cout << "\nepoch count after burst: " << epoch << "\n";

    // Ring-buffer indexing — one integer division (`round_toward_zero`, per call) gives
    // the lap through the 256-slot ring and the slot in it. 256 pkt excludes
    // zero, so the result is plain, not an expected: the lap a number, the
    // slot in packets.
    std::cout << "\nslot assignment (seq / 256, seq % 256):\n";
    seq_t s{65500};
    for (int i = 0; i < 6; ++i) {
        s += window;
        const auto [lap, slot] = divmod(s, 256_ins * pkt, round_toward_zero);
        std::cout << "  seq=" << s << "  ->  lap " << lap << ", slot " << slot << "\n";
        if (i == 5 && (lap != 2 || slot != 52_ins * pkt))
            return 1;
    }

    if (epoch != 1 || to_string(seq) != "764 pkt")
        return 1;
    return 0;
}
