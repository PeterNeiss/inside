// Tour of every error-handling mechanism plus the policy alternatives
// that avoid errors entirely (clamp / wrap / sentinel).
// See clock.cpp for a fuller on_wrap workflow.

#include <iostream>
#include <limits>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

using checked_100 = bound<{0, 100}, checked>;
using clamp_100   = bound<{0, 100}, clamp>;
using wrap_360    = bound<{0, 359}, wrap>;
using sentinel_9  = bound<{0, 9}, sentinel>;
using coarse      = bound<{{0, 10}, 2}>;   // notch 2: rounding_error demo

int main()
{
  // === Section 1: Reporting strategies for an out-of-range value ===
  // Same logical event (200 outside [0,100]), three different reports.

  try
  {
    checked_100 x = 200;
    (void)x;
  }
  catch (std::system_error& e)
  {
    std::cout << "throw:    " << e.code().message() << std::endl;
  }

  std::error_code ec;
  checked_100 y(200, ec);
  (void)y;
  std::cout << "ec:       " << (ec ? ec.message() : "no error") << std::endl;

  auto maybe = checked_100::try_make(200);
  std::cout << "optional: " << (maybe ? "has value" : "nullopt") << std::endl;

  auto ok = checked_100::try_make(50);
  std::cout << "in-range: " << *ok << std::endl;


  // === Section 2: Per-operation overrides on a default-checked bound ===

  ec.clear();
  checked_100 z(50);
  z.policy(ec) = 200;
  std::cout << "policy ec:" << (ec ? ec.message() : "no error")
            << " (z=" << z << ")" << std::endl;

  checked_100 r(50);
  r.on_error([](auto& self, errc code, std::string_view msg) {
    std::cout << "on_error: [" << make_error_code(code).message()
              << "] " << msg << " -> recover to 0" << std::endl;
    self = 0;
  }) = 200;
  std::cout << "          (r=" << r << ")" << std::endl;

  bound<{0, 100}> w(50);
  w.policy<ignore_domain>() = 200;
  std::cout << "ignored:  raw=" << w << " (domain check skipped)" << std::endl;


  // === Section 3: Every errc value gets a demo ===
  // domain_error already shown in Sections 1-2. Now: rounding_error,
  // overflow, division_by_zero. Note: on_error catches domain_error only;
  // rounding_error reports through ec/throw, overflow & division_by_zero
  // use the on_overflow callback channel.

  ec.clear();
  coarse c(0);
  c.policy(ec) = 3.0;  // 3 doesn't land on the notch-2 grid
  std::cout << "rounding: " << (ec ? ec.message() : "no error")
            << " (c=" << c << ")" << std::endl;

  checked_100 acc(50);
  acc.on_overflow([](auto& self, errc code) {
    std::cout << "overflow: [" << make_error_code(code).message()
              << "] += saturated -> 0" << std::endl;
    self = 0;
  }) += std::numeric_limits<imax>::max();
  std::cout << "          (acc=" << acc << ")" << std::endl;

  auto q = div(checked_100(10), checked_100(0),
    on_overflow([](auto& res, errc code) {
      std::cout << "div/0:    [" << make_error_code(code).message()
                << "] -> recover to 0" << std::endl;
      res = std::remove_cvref_t<decltype(res)>{0};
    }));
  std::cout << "          (q=" << q << ")" << std::endl;


  // === Section 4: Policies that avoid errors entirely ===
  // clamp saturates, wrap is modular, sentinel yields nullopt.

  clamp_100 cl = 150;
  std::cout << "clamp:    150 -> " << cl << std::endl;

  wrap_360 wr = 370;
  std::cout << "wrap:     370 -> " << wr << std::endl;

  auto se = sentinel_9::try_make(10);
  std::cout << "sentinel: 10  -> " << (se ? "has value" : "nullopt") << std::endl;


  // === Section 5: Inspection callbacks for non-error policies ===

  // on_clamp / on_wrap auto-merge their implied policy bit, so the base
  // bound need not be a clamp/wrap type.
  bound<{0, 100}> p(80);
  p.on_clamp([](auto& self, auto overshoot) {
    std::cout << "on_clamp: overshoot=" << overshoot
              << " (saturated to " << self << ")" << std::endl;
  }) = 150;

  bound<{0, 59}> sec(50);
  sec.on_wrap([](auto& self, auto carry) {
    std::cout << "on_wrap:  carry=" << carry
              << " (wrapped to " << self << ")" << std::endl;
  }) = 75;

  // .with(...) packs multiple callbacks for one operation: the imax-overflow
  // probe fires on_overflow, the post-probe narrowing fires on_clamp.
  clamp_100 combo(50);
  combo.with(
    on_overflow([](auto& self, errc) {
      std::cout << "combo:    on_overflow fired (self=" << self << ")" << std::endl;
    }),
    on_clamp([](auto&, auto over) {
      std::cout << "combo:    on_clamp overshoot=" << over << std::endl;
    })
  ) += 200;
  std::cout << "          (combo=" << combo << ")" << std::endl;

  return 0;
}
