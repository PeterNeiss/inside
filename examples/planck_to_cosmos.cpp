// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// One length type from the Planck length (1.616255·10^-35 m) to the observable universe (8.8·10^26 m),
// on a 10^-41 m grid of about 2^226 points; the _g literal spells its grid numbers exactly.

#include <format>
#include <iostream>

#include <inside/math.hpp>
#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

using namespace ins;

using length_number = id<"[1e-41..1e27] 1e-41 round_half_away">; // positive: log10 needs it
using metres        = id<"[1e-41..1e27] 1e-41 [m] round_half_away">;

int main()
{
  const metres planck   = just<1.616255e-35_g> * si::metre;
  const metres universe = just<8.8e26_g> * si::metre;

  // 1. The universe in Planck lengths:
  // an exact plain ratio of 226-bit integers, and one rounding for its log10.
  const auto ratio{universe / planck};
  std::cout << "universe / planck = " << ratio << "\n";
  const auto d{math::log10<per<(1 << 20)>>(ratio)}; // to 2^-20: about 61.74
  std::cout << std::format("log10              = {:.6f}\n", d);
  if (to_string(ratio) != "17600000000000000000000000000000000000000000000000000000000000000000/323251"
      || d != just<64'734'859, (1 << 20)>)
    return 1;

  // 2. Their geometric mean, halfway on a log scale:
  // sqrt of m² is m again, exact to the last of 41 decimals.
  const metres mean = math::sqrt<length_number>(planck * universe);
  std::cout << "sqrt(planck * universe) = " << mean << "\n";
  if (mean != *parse<metres>("0.00011926040415829555623064347860850441433 m"))
    return 1;
  return 0;
}
