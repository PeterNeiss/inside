// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Answer to the "what happens at the edge" question. Out-of-range is only
// reachable on assignment into a narrower type, and the policy in the type
// decides the outcome -- explicitly, at the point of declaration.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

using checked_100 = inside<{0, 100}, checked>;
using clamp_100   = inside<{0, 100}, clamp>;
using wrap_360    = inside<{0, 359}, wrap>;
using digit       = inside<{0, 9}>;

int main()
{
  // checked (the default): reports rather than storing a wrong value.
  try
  {
    checked_100 x = 200;
    (void)x;
  }
  catch (beman::inside::inside_error& e)
  {
    std::cout << "checked  200 ->    " << errc_message(e.Code) << "\n";
  }

  // clamp: saturate at the boundary.
  clamp_100 cl = 150;
  std::cout << "clamp    150 ->    " << cl << "\n";

  // wrap: modular arithmetic, on purpose this time.
  wrap_360 wr = 370;
  std::cout << "wrap     370 ->    " << wr << "\n";

  // try_make: failure is a value you test, not an exception.
  auto tm = digit::try_make(10);
  std::cout << "try_make  10 ->    " << (tm ? "has value" : errc_message(tm.error())) << "\n";

  // Per-operation override on an otherwise-checked value.
  checked_100 p{50};
  p.with_clamp() = 150;
  std::cout << "with_clamp() 150 -> " << p << "\n";

  return 0;
}
