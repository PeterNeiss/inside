// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Modular packet sequence numbers (TCP/QUIC-style) with epoch tracking.
//
// Demonstrates:
//   - `wrap` policy for modular-2^N arithmetic
//   - `on_wrap` callback feeding a wrap-epoch counter (cascade pattern)
//   - `_ins` integer literal in arithmetic expressions
//   - `divmod(s, 256_ins, snapped)` for the ring-buffer lap and slot

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/formats.hpp>

using namespace beman::inside;

// 16-bit modular sequence space: the full uint16 range, wrapping under `wrap`.
using seq_t = inside<{0, 65535}, wrap>;

// Epoch counter — how many times the SEQ space has wrapped (saturating tally).
using epoch_t = clamp_counter<1000>;

int main() {
    seq_t   seq{65500};
    epoch_t epoch{0};

    // An inside delta (`_ins`): `seq += window` is an inside-RHS wrap, so the on_wrap
    // carry is itself an inside (here ignored — we only bump the epoch counter).
    constexpr auto window = 100_ins;

    // `_ins` literal builds a single-value `inside<{N, N}>`. Composing it with
    // another inside widens the grid via the addition machinery — handy for
    // expressing constants that participate in type-safe arithmetic without
    // turning into runtime ints.
    auto preview = 100_ins + seq;
    std::cout << "preview after +100_ins: " << preview << "\n";

    std::cout << "step  seq    epoch\n";
    for (int i = 0; i < 8; ++i) {
        // on_wrap callback fires when seq + window crosses the upper bound.
        // The handler bumps the epoch counter — TCP/QUIC's "wrap count" idiom.
        seq.on_wrap([&](auto&, auto) { ++epoch; }) += window;
        std::cout << "  " << i << "    " << seq << "    " << epoch << "\n";
    }

    std::cout << "\nepoch count after burst: " << epoch << "\n";

    // Ring-buffer indexing — one integer division (`snapped`, per call) gives
    // the lap through the 256-slot ring and the slot in it. 256_ins excludes
    // zero, so the result is plain, not an expected.
    std::cout << "\nslot assignment (seq / 256, seq % 256):\n";
    seq_t s{65500};
    for (int i = 0; i < 6; ++i) {
        s += window;
        const auto [lap, slot] = divmod(s, 256_ins, snapped);
        std::cout << "  seq=" << s << "  ->  lap " << lap << ", slot " << slot << "\n";
    }

    return 0;
}
