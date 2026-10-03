// Modular packet sequence numbers (TCP/QUIC-style) with epoch tracking.
//
// Demonstrates:
//   - `wrap` policy for modular-2^N arithmetic
//   - `on_wrap` callback feeding a wrap-epoch counter (cascade pattern)
//   - `_ins` integer literal in arithmetic expressions
//   - Modulo `%` for ring-buffer slot index (under `snap`)

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/formats.hpp>

using namespace beman::inside;

// 16-bit modular sequence space. UINT16_MAX is reserved as the optional
// sentinel slot, so use {0, 65534} — still wraps cleanly under `wrap`.
using seq_t  = inside<{0, 65534}, wrap | snap>;

// Epoch counter — how many times the SEQ space has wrapped (saturating tally).
using epoch_t = counter<1000>;

int main()
{
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
  for (int i = 0; i < 8; ++i)
  {
    // on_wrap callback fires when seq + window crosses the upper bound.
    // The handler bumps the epoch counter — TCP/QUIC's "wrap count" idiom.
    seq.on_wrap([&](auto&, auto carry) {
      (void)carry;
      ++epoch;
    }) += window;
    std::cout << "  " << i << "    " << seq << "    " << epoch << "\n";
  }

  std::cout << "\nepoch count after burst: " << epoch << "\n";

  // Ring-buffer indexing — modulo % collapses any SEQ value into a slot.
  // Both operands need `snap` so the integer-division path fires.
  std::cout << "\nslot assignment (seq % 256):\n";
  using divisor_t = inside<{1, 256}, snap>;
  constexpr divisor_t slots{256};
  seq_t s{65500};
  for (int i = 0; i < 6; ++i)
  {
    s += window;
    // `slots` has grid {1,256} (excludes zero), so `%` yields a plain inside.
    auto slot = s % slots;
    std::cout << "  seq=" << s << "  ->  slot " << slot << "\n";
  }

  return 0;
}
