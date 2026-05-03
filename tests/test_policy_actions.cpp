#include "bound/bound.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string_view>
#include <system_error>

using namespace bnd;

TEST_CASE("clamp policy on assignment", "[bound][policy][clamp]")
{
  using pct = bound<{0, 100}, clamp>;
  pct over{150};
  REQUIRE(static_cast<rational>(over) == 100);

  pct under{-5};
  REQUIRE(static_cast<rational>(under) == 0);

  pct ok{50};
  REQUIRE(static_cast<rational>(ok) == 50);

  // compound +
  pct a{90};
  a += bound<{0, 100}>{20};
  REQUIRE(static_cast<rational>(a) == 100);
}

TEST_CASE("wrap policy on assignment", "[bound][policy][wrap]")
{
  using angle = bound<{0, 359}, wrap>;
  angle a{370};   REQUIRE(static_cast<rational>(a) == 10);
  angle b{-10};   REQUIRE(static_cast<rational>(b) == 350);
  angle c{360};   REQUIRE(static_cast<rational>(c) == 0);
  angle d{180};   REQUIRE(static_cast<rational>(d) == 180);
}

TEST_CASE("sentinel policy hides overflow as nullopt", "[bound][policy][sentinel]")
{
  using idx = bound<{0, 9}, sentinel>;

  idx a{5};
  REQUIRE(static_cast<rational>(a) == 5);

  slim::optional<idx> opt{5};
  ++opt;
  REQUIRE(opt.has_value());
  REQUIRE(*opt == 6);

  slim::optional<idx> top{9};
  ++top;
  REQUIRE_FALSE(top.has_value());

  slim::optional<idx> bot{0};
  --bot;
  REQUIRE_FALSE(bot.has_value());
}

TEST_CASE("legacy policy<wrap>(lambda) callback form", "[bound][policy][callback]")
{
  using sec   = bound<{0, 59}, wrap>;
  using min_t = bound<{0, 59}>;
  sec seconds{0};
  min_t minutes{0};

  seconds.policy<wrap>([&](auto carry){
    minutes = static_cast<int>(static_cast<rational>(minutes).Numerator)
            + static_cast<int>(carry);
  }) = 65;

  REQUIRE(static_cast<rational>(seconds) == 5);
  REQUIRE(static_cast<rational>(minutes) == 1);

  using pct = bound<{0, 100}, clamp>;
  pct p{0};
  imax excess = 0;
  p.policy<clamp>([&](auto e){ excess = static_cast<imax>(e); }) = 150;
  REQUIRE(static_cast<rational>(p) == 100);
  REQUIRE(excess == 50);
}

TEST_CASE("on_wrap action receives bound& and carry", "[bound][policy][on_wrap]")
{
  using sec = bound<{0, 59}, wrap>;
  sec s{0};
  imax carry = 0;
  s.on_wrap([&](auto& self, auto c){ carry = static_cast<imax>(c); (void)self; }) = 65;
  REQUIRE(static_cast<rational>(s) == 5);
  REQUIRE(carry == 1);

  // handler may override the wrapped value
  sec s2{0};
  s2.on_wrap([](auto& self, auto c){ if (c > 0) self = 0; }) = 65;
  REQUIRE(static_cast<rational>(s2) == 0);
}

TEST_CASE("on_clamp action receives overshoot", "[bound][policy][on_clamp]")
{
  using u100 = bound<{0, 100}>;
  u100 x{0};
  imax overshoot = 0;
  x.on_clamp([&](auto& self, auto over){
    overshoot = static_cast<imax>(over);
    (void)self;
  }) = 150;
  REQUIRE(static_cast<rational>(x) == 100);
  REQUIRE(overshoot == 50);
}

TEST_CASE("on_error action receives code and message", "[bound][policy][on_error]")
{
  using c100 = bound<{0, 100}, checked>;
  c100 e{50};
  bool fired = false;
  e.on_error([&](auto& self, errc code, std::string_view msg){
    fired = (code == errc::domain_error) && !msg.empty();
    self = 0;
  }) = 200;
  REQUIRE(fired);
  REQUIRE(static_cast<rational>(e) == 0);
}

TEST_CASE("on_sentinel action receives original value", "[bound][policy][on_sentinel]")
{
  using s100 = bound<{0, 100}, sentinel>;
  s100 sv{50};
  imax orig = 0;
  sv.on_sentinel([&](auto& self, auto orig_in){
    orig = static_cast<imax>(orig_in);
    self = 50;
  }) = 200;
  REQUIRE(static_cast<rational>(sv) == 50);
  REQUIRE(orig == 200);
}

TEST_CASE("on_overflow on compound op", "[bound][policy][on_overflow]")
{
  using c100 = bound<{0, 100}, checked>;
  c100 acc{50};
  bool fired = false;
  acc.on_overflow([&](auto& self, errc code){
    fired = (code == errc::overflow);
    self = 0;
  }) += std::numeric_limits<imax>::max();

  REQUIRE(fired);
  REQUIRE(static_cast<rational>(acc) == 0);
}

TEST_CASE("multi-action with(...) — overflow vs clamp paths fire correctly",
          "[bound][policy][multi]")
{
  using c100 = bound<{0, 100}, checked>;

  SECTION("imax-overflow path fires on_overflow only")
  {
    c100 a{50};
    bool of = false, cl = false;
    a.with(on_overflow([&](auto& self, errc){ of = true; self = 7; }),
           on_clamp([&](auto&, auto){ cl = true; })) += std::numeric_limits<imax>::max();
    REQUIRE(of);
    REQUIRE_FALSE(cl);
    REQUIRE(static_cast<rational>(a) == 7);
  }

  SECTION("post-probe narrowing fires on_clamp only")
  {
    c100 b{50};
    bool of = false, cl = false;
    imax over = 0;
    b.with(on_overflow([&](auto&, errc){ of = true; }),
           on_clamp([&](auto&, auto o){ cl = true; over = static_cast<imax>(o); }))
      += 200;   // 50+200=250 fits imax, but overshoots [0,100]
    REQUIRE_FALSE(of);
    REQUIRE(cl);
    REQUIRE(over == 150);
    REQUIRE(static_cast<rational>(b) == 100);
  }

  SECTION("single-action via with()")
  {
    c100 c{50};
    bool fired = false;
    c.with(on_overflow([&](auto& self, errc){ fired = true; self = 0; }))
      += std::numeric_limits<imax>::max();
    REQUIRE(fired);
    REQUIRE(static_cast<rational>(c) == 0);
  }
}

TEST_CASE("free-fn pack form", "[bound][policy][pack]")
{
  using c100 = bound<{0, 100}, checked>;
  c100 d{50}, z{0};
  bool of = false;
  errc seen{};
  auto q = div(d, z,
    on_overflow([&](auto& res, errc c){
      of = true; seen = c;
      res = std::remove_cvref_t<decltype(res)>{1};
    }),
    on_clamp([](auto&, auto){}));   // inert here, accepted
  (void)q;
  REQUIRE(of);
  REQUIRE(seen == errc::division_by_zero);

  // SFINAE positive cases: pack with on_overflow must bind. The negative case
  // (pack without on_overflow) is enforced by the overload's `requires` clause
  // and would surface noisy compiler diagnostics if probed here.
  using u100 = bound<{0, 100}>;
  auto noop = [](auto&, auto){};
  STATIC_REQUIRE(requires(u100 x, u100 y) { add(x, y, on_overflow(noop)); });
  STATIC_REQUIRE(requires(u100 x, u100 y) { add(x, y, on_overflow(noop), on_clamp(noop)); });
}

TEST_CASE("mod free-fn with on_overflow recovers from div/0", "[bound][policy][mod]")
{
  using u100ic = bound<{0, 100}, checked | ignore_round>;
  u100ic l{7}, r{0};
  bool fired = false;
  auto m = mod(l, r,
    on_overflow([&](auto& res, errc){
      fired = true;
      res = std::remove_cvref_t<decltype(res)>{0};
    }));
  (void)m;
  REQUIRE(fired);
}
