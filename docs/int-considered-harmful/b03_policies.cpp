// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Answer to the "what happens at the edge" question. Out-of-range is only
// reachable on assignment into a narrower type, and the policy in the type,
// or a channel at the store, decides the outcome -- explicitly, before it
// runs. Nothing throws.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>

using namespace ins;

using checked_100 = inside<{0, 100}, checked>;
using clamp_100   = inside<{0, 100}, clamp>;
using wrap_360    = inside<{0, 359}, wrap>;
using digit       = inside<{0, 9}>;

int main(int argc, char**)
{
  const int n = 199 + argc;          // 200, read at run time

  // checked (the default): a store the grids cannot prove names its channel.
  // checked_100 x = 200;            // does not compile: 200 is outside
  // checked_100 y = n;              // does not compile: n may not fit
  error        ec{};
  checked_100 x{50};
  x.on_error(ec) = n;                // reports; x keeps 50
  std::cout << "checked  200 ->    " << error_message(ec) << "\n";

  // clamp: saturate at the boundary.
  clamp_100 cl = 150;
  std::cout << "clamp    150 ->    " << cl << "\n";

  // wrap: modular arithmetic, on purpose this time.
  wrap_360 wr = 370;
  std::cout << "wrap     370 ->    " << wr << "\n";

  // try_make: failure is a value you test.
  auto tm = digit::try_make(n / 20);
  std::cout << "try_make  10 ->    " << (tm ? "has value" : error_message(tm.error())) << "\n";

  // Per-operation override on an otherwise-checked value.
  checked_100 p{50};
  p.with<clamp>() = 150;
  std::cout << "with<clamp>() 150 -> " << p << "\n";

  return 0;
}
