// Modular packet sequence numbers (TCP/QUIC-style) with epoch tracking.
//
// Demonstrates:
//   - `wrap` policy for modular-2^N arithmetic
//   - `on_wrap` callback feeding a wrap-epoch counter (cascade pattern)
//   - `_b` integer literal in arithmetic expressions
//   - Modulo `%` for ring-buffer slot index (under `snapping`)

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

// 16-bit modular sequence space. UINT16_MAX is reserved as the optional
// sentinel slot, so use {0, 65534} — still wraps cleanly under `wrap`.
using seq_t  = bound<{0, 65534}, wrap | snapping>;

// Epoch counter — how many times the SEQ space has wrapped.
using epoch_t = bound<{0, 1000}>;

int main()
{
  seq_t   seq{65500};
  epoch_t epoch{0};

  // A bound delta (`_b`): `seq += window` is a bound-RHS wrap, so the on_wrap
  // carry is itself a bound (here ignored — we only bump the epoch counter).
  constexpr auto window = 100_b;

  // `_b` literal builds a single-value `bound<{N, N}>`. Composing it with
  // another bound widens the grid via the addition machinery — handy for
  // expressing constants that participate in type-safe arithmetic without
  // turning into runtime ints.
  auto preview = 100_b + seq;
  std::cout << "preview after +100_b: " << preview << "\n";

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
  // Both operands need `snapping` so the integer-division path fires.
  std::cout << "\nslot assignment (seq % 256):\n";
  using divisor_t = bound<{1, 256}, snapping>;
  constexpr divisor_t slots{256};
  seq_t s{65500};
  for (int i = 0; i < 6; ++i)
  {
    s += window;
    // `slots` has grid {1,256} (excludes zero), so `%` yields a plain bound.
    auto slot = s % slots;
    std::cout << "  seq=" << s << "  ->  slot " << slot << "\n";
  }

  return 0;
}
