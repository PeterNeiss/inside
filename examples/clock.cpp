// A 24-hour clock using wrap with carry.
// When seconds overflow 59, the excess carries into minutes.
// When minutes overflow 59, the excess carries into hours.
// Hours wrap around at 24.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

struct clock24
{
  using hour_t   = bound<{0, 23}, wrap>;
  using minute_t = bound<{0, 59}, wrap>;
  using second_t = bound<{0, 59}, wrap>;

  hour_t   hours{0};
  minute_t minutes{0};
  second_t seconds{0};

  clock24() = default;
  clock24(int h, int m, int s) : hours(h), minutes(m), seconds(s) {}

  void add_seconds(auto n)
  {
    seconds.on_wrap([&](auto, auto carry) { add_minutes(carry); }) += n;
  }

  void add_minutes(auto n)
  {
    minutes.on_wrap([&](auto, auto carry) { add_hours(carry); }) += n;
  }

  void add_hours(auto n)
  {
    hours += n;
  }

  friend std::ostream& operator<<(std::ostream& os, const clock24& c)
  {
    auto pad = [&](auto b) {
      if (b < 10) os << '0';
      os << b;
    };
    pad(c.hours); os << ':'; pad(c.minutes); os << ':'; pad(c.seconds);
    return os;
  }
};

int main()
{
  clock24 t(23, 59, 45);
  std::cout << "start:      " << t << "\n";

  // Pass the delta as a bound (`_b` literal): the `+=` is bound-RHS, so the wrap
  // carry threaded through the sec→min→hour cascade is itself a bound, not a raw
  // integer — the whole cascade stays in bound-space. (Large deltas land on a
  // disjoint sub-grid, which wrap accepts.)
  t.add_seconds(20_b);
  std::cout << "+20 sec:    " << t << "\n";

  t.add_minutes(90_b);
  std::cout << "+90 min:    " << t << "\n";

  t.add_seconds(just<3600 + 1800 + 30>);
  std::cout << "+5430 sec:  " << t << "\n";

  return 0;
}

