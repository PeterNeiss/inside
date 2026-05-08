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
  REQUIRE(over == 100);

  pct under{-5};
  REQUIRE(under == 0);

  pct ok{50};
  REQUIRE(ok == 50);

  // compound +
  pct a{90};
  a += bound<{0, 100}>{20};
  REQUIRE(a == 100);
}

TEST_CASE("wrap policy on assignment", "[bound][policy][wrap]")
{
  using angle = bound<{0, 359}, wrap>;
  angle a{370};   REQUIRE(a == 10);
  angle b{-10};   REQUIRE(b == 350);
  angle c{360};   REQUIRE(c == 0);
  angle d{180};   REQUIRE(d == 180);
}

TEST_CASE("sentinel policy hides overflow as nullopt", "[bound][policy][sentinel]")
{
  using idx = bound<{0, 9}, sentinel>;

  idx a{5};
  REQUIRE(a == 5);

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

TEST_CASE("sentinel policy on fixed-point grids", "[bound][policy][sentinel][fixed]")
{
  SECTION("Q8.8 (notch 1/256) — uint16 storage")
  {
    using fp = bound<{{0, 255}, 1.0/256}, sentinel>;
    static_assert(sizeof(fp) == 2);

    fp ok{42.5};
    REQUIRE(double(ok) == 42.5);

    // out-of-range write produces nullopt via optional
    slim::optional<fp> high{42.5};
    high = 300.0;
    REQUIRE_FALSE(high.has_value());

    slim::optional<fp> low{0.0};
    low = -0.5;
    REQUIRE_FALSE(low.has_value());

    // notch-aligned in-range value survives
    slim::optional<fp> mid{100.0};
    mid = 200.125;
    REQUIRE(mid.has_value());
    REQUIRE(double(*mid) == 200.125);
  }

  SECTION("half-step (notch 0.5) — uint8 storage")
  {
    using sensor = bound<{{0, 50}, 0.5}, sentinel>;
    static_assert(sizeof(sensor) == 1);

    slim::optional<sensor> s{23.5};
    REQUIRE(double(*s) == 23.5);

    s = 50.5;
    REQUIRE_FALSE(s.has_value());
  }

  SECTION("signed Q1.14 (notch 1/16384) — uint16 storage")
  {
    using sample = bound<{{-1, 1}, *(1_r/16384)}, sentinel | round_nearest>;
    static_assert(sizeof(sample) == 2);

    slim::optional<sample> s{0.5};
    REQUIRE(double(*s) == 0.5);

    s = 1.5;
    REQUIRE_FALSE(s.has_value());

    s = sample{0.0};
    s = -1.25;
    REQUIRE_FALSE(s.has_value());
  }

  SECTION("Q16.16 (notch 1/65536) — uint32 storage")
  {
    using fp = bound<{{0, 65535}, *(1_r/65536)}, sentinel>;
    static_assert(sizeof(fp) == 4);

    slim::optional<fp> v{1000.125};
    REQUIRE(double(*v) == 1000.125);

    v = -0.001;
    REQUIRE_FALSE(v.has_value());
  }
}

TEST_CASE("on_sentinel action on fixed-point grids", "[bound][policy][on_sentinel][fixed]")
{
  using fp = bound<{{0, 50}, 0.5}, sentinel>;

  fp v{10.5};
  rational orig{0u};
  v.on_sentinel([&](auto& self, auto orig_in){
    orig = static_cast<rational>(orig_in);
    self = 0;
  }) = 75.5;

  REQUIRE(double(v) == 0);
  REQUIRE(orig == rational{151, 2});  // 75.5
}

TEST_CASE("legacy policy<wrap>(lambda) callback form", "[bound][policy][callback]")
{
  using sec   = bound<{0, 59}, wrap>;
  using min_t = bound<{0, 59}>;
  sec seconds{0};
  min_t minutes{0};

  seconds.policy<wrap>([&](auto carry){
    minutes += carry;
  }) = 65;

  REQUIRE(seconds == 5);
  REQUIRE(minutes == 1);

  using pct = bound<{0, 100}, clamp>;
  pct p{0};
  imax excess = 0;
  p.policy<clamp>([&](auto e){ excess = e; }) = 150;
  REQUIRE(p == 100);
  REQUIRE(excess == 50);
}

TEST_CASE("on_wrap action receives bound& and carry", "[bound][policy][on_wrap]")
{
  using sec = bound<{0, 59}, wrap>;
  sec s{0};
  imax carry = 0;
  s.on_wrap([&](auto& self, auto c){ carry = c; (void)self; }) = 65;
  REQUIRE(s == 5);
  REQUIRE(carry == 1);

  // handler may override the wrapped value
  sec s2{0};
  s2.on_wrap([](auto& self, auto c){ if (c > 0) self = 0; }) = 65;
  REQUIRE(s2 == 0);
}

TEST_CASE("on_clamp action receives overshoot", "[bound][policy][on_clamp]")
{
  using u100 = bound<{0, 100}>;
  u100 x{0};
  imax overshoot = 0;
  x.on_clamp([&](auto& self, auto over){
    overshoot = over;
    (void)self;
  }) = 150;
  REQUIRE(x == 100);
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
  REQUIRE(e == 0);
}

TEST_CASE("on_sentinel action receives original value", "[bound][policy][on_sentinel]")
{
  using s100 = bound<{0, 100}, sentinel>;
  s100 sv{50};
  imax orig = 0;
  sv.on_sentinel([&](auto& self, auto orig_in){
    orig = orig_in;
    self = 50;
  }) = 200;
  REQUIRE(sv == 50);
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
  REQUIRE(acc == 0);
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
    REQUIRE(a == 7);
  }

  SECTION("post-probe narrowing fires on_clamp only")
  {
    c100 b{50};
    bool of = false, cl = false;
    imax over = 0;
    b.with(on_overflow([&](auto&, errc){ of = true; }),
           on_clamp([&](auto&, auto o){ cl = true; over = o; }))
      += 200;   // 50+200=250 fits imax, but overshoots [0,100]
    REQUIRE_FALSE(of);
    REQUIRE(cl);
    REQUIRE(over == 150);
    REQUIRE(b == 100);
  }

  SECTION("single-action via with()")
  {
    c100 c{50};
    bool fired = false;
    c.with(on_overflow([&](auto& self, errc){ fired = true; self = 0; }))
      += std::numeric_limits<imax>::max();
    REQUIRE(fired);
    REQUIRE(c == 0);
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
