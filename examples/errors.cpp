// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Every way to handle an error, and the policies that avoid one (clamp, wrap).
// Nothing throws: a store clamps, wraps, takes an alternative value, reports, or does not compile.
// clock.cpp has a fuller on_wrap workflow.

#include <iostream>

#include <inside/inside/formats.hpp>
#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;

// An int read at run time, as from input.
int read(int v)
{
  volatile int x{v};
  return x;
}

int main()
{
  // 1. Reporting an out-of-range 200 %.
  // A constant is checked while compiling (`percentage x{200, percent};` does not compile),
  // and a run-time int may not fit, so `percentage x{n, percent};` does not compile either: name a channel.
  using percentage = id<"[0..100] [%]">;
  const int  n{read(200)};
  const auto n_percent{formats::int32{n} * percent}; // any int fits int32: a quantity to store and check

  auto maybe{percentage::try_make(n, percent)};
  std::cout << "expected: " << (maybe ? "has value" : error_message(maybe.error())) << "\n";

  auto ok{percentage::try_make(50, percent)};
  std::cout << "in-range: " << *ok << "\n";

  // 2. Per-store overrides on a default-checked inside: an error&, a hook with an alternative value, clamp.
  ins::error ec{};
  percentage z{50, percent};
  z.on_error(ec) = n_percent;
  std::cout << "policy ec:" << (ec != error{} ? error_message(ec) : "no error") << " (z=" << z << ")" << "\n";

  percentage r{50, percent};
  r.on_error(
    [](auto& self, error code, std::string_view msg)
    {
      std::cout << "on_error: [" << error_message(code) << "] " << msg << " -> recover to 0 %" << "\n";
      self = 0_q * percent; // the alternative value
    }) = n_percent;
  std::cout << "          (r=" << r << ")" << "\n";

  id<"[0..100]"> w(50);
  w.with<clamp>() = 200;
  std::cout << "clamped:  value=" << w << " (this store saturates)" << "\n";

  // 3. The other error values:
  // rounding_error (in a hook) and division_by_zero (in the expected of a division).
  using coarse = id<"[0..10] 2">; // notch 2
  coarse c(0);
  c.on_error(
    [](auto& self, error code, std::string_view msg)
    {
      std::cout << "rounding: [" << error_message(code) << "] " << msg << " -> recover to 4" << "\n";
      self = 4;
    }) = 3.0; // 3 is not on the notch-2 grid
  std::cout << "          (c=" << c << ")" << "\n";

  // An average speed over 0 s.
  using metres  = id<"[0..100] [m]">;
  using seconds = id<"[0..60] [s]">;
  const auto quotient{div(metres{10, si::metre}, seconds{0, si::second})};
  if (!quotient)
    std::cout << "div/0:    [" << error_message(quotient.error()) << "] -> recover to 0 m/s" << "\n";
  const auto q{quotient.value_or(std::remove_cvref_t<decltype(*quotient)>{0, si::metre / si::second})};
  std::cout << "          (q=" << q << ")" << "\n";

  // 4. add / sub / mul / div / mod take an error& too: ec is set, and the result holds the error.
  ec = error{};
  auto qz{div(metres{10, si::metre}, seconds{0, si::second}, ec)};
  std::cout << "free-fn:  " << (ec != error{} ? error_message(ec) : "no error")
            << " (q has_value=" << (qz.has_value() ? "true" : "false") << ")" << "\n";

  // 5. Policies that avoid errors: clamp saturates, wrap is modular; try_make reports.
  using clamped_percentage = id<"[0..100] [%] clamp">;
  using heading            = id<"[0..359] [deg] wrap">;
  using digit              = id<"[0..9]">;

  clamped_percentage cl{150, percent};
  std::cout << "clamp:    150 % -> " << cl << "\n";

  heading wr{370, si::degree};
  std::cout << "wrap:     370° -> " << wr << "\n";

  auto tm{digit::try_make(10)};
  std::cout << "try_make: 10  -> " << (tm ? "has value" : error_message(tm.error())) << "\n";

  // 6. Watching clamp and wrap: on_clamp and on_wrap add their policy, so the inside need not have it.
  id<"[0..100]"> p(80);
  p.on_clamp(
    [](auto& self, auto overshoot)
    {
      std::cout << "on_clamp: overshoot=" << overshoot << " (saturated to " << self << ")" << "\n";
    }) = 150;

  id<"[0..59] [s]"> sec{50, si::second}; // the seconds of a clock
  sec.on_wrap(
    [](auto& self, auto carry)
    {
      std::cout << "on_wrap:  carry=" << carry << " (wrapped to " << self << ")" << "\n";
    }) = 75_q * si::second;

  // A hook on a compound op: the sum cannot fail, but storing it back past 100 % fires on_clamp.
  clamped_percentage combo{50, percent};
  combo.on_clamp([](auto&, auto over) { std::cout << "combo:    on_clamp overshoot=" << over << "\n"; })
    += 200_q * percent;
  std::cout << "          (combo=" << combo << ")" << "\n";
  return 0;
}
