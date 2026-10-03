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
using sentinel_9  = inside<{0, 9}, sentinel>;

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
    std::cout << "checked  200 ->    " << errc_message(e.code) << "\n";
  }

  // clamp: saturate at the boundary.
  clamp_100 cl = 150;
  std::cout << "clamp    150 ->    " << cl << "\n";

  // wrap: modular arithmetic, on purpose this time.
  wrap_360 wr = 370;
  std::cout << "wrap     370 ->    " << wr << "\n";

  // sentinel: absence is representable.
  auto se = sentinel_9::try_make(10);
  std::cout << "sentinel  10 ->    " << (se ? "has value" : "nullopt") << "\n";

  // Per-operation override on an otherwise-checked value.
  checked_100 p{50};
  p.with_clamp() = 150;
  std::cout << "with_clamp() 150 -> " << p << "\n";

  return 0;
}
