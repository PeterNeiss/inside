// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Modular packet sequence numbers (TCP/QUIC style) with epoch tracking:
// wrap gives modular-2^16 arithmetic and on_wrap counts the wraps.
// Packets are a base dimension (pkt), the epoch counts wraps in its own unit,
// and the lap, packets over packets, is a plain number.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/inside/formats.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;
using namespace ins::formats;

namespace
{
  inline constexpr auto pkt   = base_unit<"pkt", "packets">;
  inline constexpr auto epoch = base_unit<"epoch", "sequence_wraps">;
} // namespace

int main()
{
  using sequence_number = id<"[0..65535] [pkt] wrap">; // the uint16 sequence space
  using epoch_counter   = id<quantity_key<epoch, clamp_counter<1000>>>;      // how often it wrapped, saturating
  sequence_number seq   {65500, pkt};
  epoch_counter   epochs{0, epoch};

  // 100_ins is the single-value id<"[100..100]">, and times pkt a single-value quantity:
  // added to another quantity it widens the grid, a constant that never turns into a run-time int.
  constexpr auto window {100_ins * pkt};
  auto           preview{window + seq};
  std::cout << "preview after +100 pkt: " << preview << "\n";

  // seq += window wraps past 65535, and on_wrap bumps the epoch: TCP/QUIC's wrap count.
  std::cout << "step  seq    epoch\n";
  for (int i = 0; i < 8; ++i)
  {
    seq.on_wrap([&](auto&, auto) { ++epochs; }) += window;
    std::cout << "  " << i << "    " << seq << "    " << epochs << "\n";
  }
  std::cout << "\nepoch count after burst: " << epochs << "\n";

  // A 256-slot ring: one integer division gives the lap (a number) and the slot (in packets).
  // 256 pkt excludes 0, so the result is plain, not an expected.
  std::cout << "\nslot assignment (seq / 256, seq % 256):\n";
  sequence_number s{65500, pkt};
  for (int i = 0; i < 6; ++i)
  {
    s += window;
    const auto [lap, slot] = divmod(s, 256_ins * pkt, round_toward_zero);
    std::cout << "  seq=" << s << "  ->  lap " << lap << ", slot " << slot << "\n";
    if (i == 5 && (lap != 2 || slot != 52_ins * pkt))
      return 1;
  }

  if (epochs != epoch || to_string(seq) != "764 pkt")
    return 1;
  return 0;
}
